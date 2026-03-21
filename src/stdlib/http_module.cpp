#include "stdlib/http_module.h"
#include "stdlib/native_args.h"

#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <sstream>
#include <cstring>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

HttpModule::HttpModule() : NativeModule({"Std", "Http"}) {}

void HttpModule::initialize() {
    // HTTP operations are async — run in thread pool
    module->exports["get"] = make_native_async_function("get", 1, get);
    module->exports["post"] = make_native_async_function("post", 2, post);
}

// Parse URL into host, port, path
static bool parse_url(const string& url, string& host, string& port, string& path) {
    string remaining = url;

    // Strip scheme
    if (remaining.starts_with("http://")) {
        remaining = remaining.substr(7);
        port = "80";
    } else if (remaining.starts_with("https://")) {
        // HTTPS not supported without TLS library
        return false;
    } else {
        port = "80";
    }

    // Split host and path
    auto slash_pos = remaining.find('/');
    if (slash_pos == string::npos) {
        host = remaining;
        path = "/";
    } else {
        host = remaining.substr(0, slash_pos);
        path = remaining.substr(slash_pos);
    }

    // Check for port in host
    auto colon_pos = host.find(':');
    if (colon_pos != string::npos) {
        port = host.substr(colon_pos + 1);
        host = host.substr(0, colon_pos);
    }

    return true;
}

RuntimeObjectPtr HttpModule::http_request(const string& method, const string& url, const string& body) {
    string host, port, path;
    if (!parse_url(url, host, port, path)) {
        return make_result("error", make_string("Failed to parse URL or HTTPS not supported: " + url));
    }

    // Resolve host
    struct addrinfo hints{}, *result;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (status != 0) {
        return make_result("error", make_string("DNS resolution failed: " + string(gai_strerror(status))));
    }

    // Create socket and connect
    int sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock < 0) {
        freeaddrinfo(result);
        return make_result("error", make_string("Failed to create socket"));
    }

    if (connect(sock, result->ai_addr, result->ai_addrlen) < 0) {
        freeaddrinfo(result);
        close(sock);
        return make_result("error", make_string("Connection failed to " + host + ":" + port));
    }
    freeaddrinfo(result);

    // Build HTTP request
    ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Connection: close\r\n";
    if (!body.empty()) {
        request << "Content-Type: application/json\r\n"
                << "Content-Length: " << body.size() << "\r\n";
    }
    request << "\r\n" << body;

    string req_str = request.str();
    if (send(sock, req_str.c_str(), req_str.size(), 0) < 0) {
        close(sock);
        return make_result("error", make_string("Failed to send request"));
    }

    // Read response
    string response;
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        response += buffer;
    }
    close(sock);

    // Parse response: split headers and body
    auto header_end = response.find("\r\n\r\n");
    if (header_end == string::npos) {
        return make_result("error", make_string("Invalid HTTP response"));
    }

    string headers = response.substr(0, header_end);
    string resp_body = response.substr(header_end + 4);

    // Extract status code from first line
    int status_code = 0;
    auto space1 = headers.find(' ');
    if (space1 != string::npos) {
        auto space2 = headers.find(' ', space1 + 1);
        if (space2 != string::npos) {
            try { status_code = stoi(headers.substr(space1 + 1, space2 - space1 - 1)); }
            catch (...) {}
        }
    }

    // Return (status_code, body) tuple
    auto result_tuple = make_shared<TupleValue>();
    result_tuple->fields.push_back(make_int(status_code));
    result_tuple->fields.push_back(make_string(resp_body));
    return make_ok(make_shared<RuntimeObject>(Tuple, result_tuple));
}

RuntimeObjectPtr HttpModule::get(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("get", 1);
    return http_request("GET", ARG_STRING(0), "");
}

RuntimeObjectPtr HttpModule::post(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("post", 2);
    return http_request("POST", ARG_STRING(0), ARG_STRING(1));
}

} // namespace yona::stdlib
