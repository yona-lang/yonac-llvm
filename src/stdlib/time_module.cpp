#include "stdlib/time_module.h"
#include "stdlib/native_args.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

TimeModule::TimeModule() : NativeModule({"Std", "Time"}) {}

void TimeModule::initialize() {
    module->exports["now"] = make_native_function("now", 0, now);
    module->exports["nowMillis"] = make_native_function("nowMillis", 0, nowMillis);
    module->exports["format"] = make_native_function("format", 2, format);
    module->exports["parse"] = make_native_function("parse", 2, parse);
}

RuntimeObjectPtr TimeModule::now(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("now", 0);
    auto tp = chrono::system_clock::now();
    auto secs = chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count();
    return make_int(static_cast<int>(secs));
}

RuntimeObjectPtr TimeModule::nowMillis(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("nowMillis", 0);
    auto tp = chrono::system_clock::now();
    auto millis = chrono::duration_cast<chrono::milliseconds>(tp.time_since_epoch()).count();
    return make_int(static_cast<int>(millis));
}

RuntimeObjectPtr TimeModule::format(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("format", 2);
    string fmt = ARG_STRING(0);
    int timestamp = ARG_INT(1);

    time_t t = static_cast<time_t>(timestamp);
    struct tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif

    char buf[256];
    size_t len = strftime(buf, sizeof(buf), fmt.c_str(), &tm_buf);
    if (len == 0) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "format: strftime failed or buffer too small");
    }
    return make_string(string(buf, len));
}

RuntimeObjectPtr TimeModule::parse(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("parse", 2);
    string fmt = ARG_STRING(0);
    string input = ARG_STRING(1);

    struct tm tm_buf = {};
    istringstream ss(input);
    ss >> get_time(&tm_buf, fmt.c_str());
    if (ss.fail()) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "parse: failed to parse time string '" + input + "' with format '" + fmt + "'");
    }

#ifdef _WIN32
    time_t t = _mkgmtime(&tm_buf);
#else
    time_t t = timegm(&tm_buf);
#endif

    return make_int(static_cast<int>(t));
}

} // namespace yona::stdlib
