// yona — Yona REPL (compile-and-run)
//
// Reads expressions, compiles to native code via yonac, runs, shows output.

#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <array>
#include <vector>
#include <unordered_set>

#include "Parser.h"
#include "Codegen.h"

using namespace std;
using namespace yona;
using namespace yona::compiler::codegen;
namespace fs = std::filesystem;

static const char* const platform_runtime_sources[] = {
#ifdef _WIN32
    "file_windows.c", "net_windows.c", "os_windows.c",
#else
    "file_linux.c", "net_linux.c", "os_linux.c",
#endif
};

static fs::path repl_temp_dir() {
    if (const char* t = getenv("TMPDIR"))
        return fs::path(t);
    if (const char* t = getenv("TEMP"))
        return fs::path(t);
    if (const char* t = getenv("TMP"))
        return fs::path(t);
    return fs::temp_directory_path();
}

static const char* repl_cc() {
    if (const char* e = getenv("YONAC_CC"))
        if (*e) return e;
#ifdef _WIN32
    return "clang";
#else
    return "cc";
#endif
}

static string q(const fs::path& p) {
    return "\"" + p.string() + "\"";
}

static string err_null() {
#ifdef _WIN32
    return " 2>nul";
#else
    return " 2>/dev/null";
#endif
}

static string exe_suffix() {
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

static fs::path canonical_if_exists(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec)) return {};
    auto c = fs::weakly_canonical(p, ec);
    return ec ? p : c;
}

static void push_unique_root(vector<fs::path>& roots, unordered_set<string>& seen, const fs::path& p) {
    auto c = canonical_if_exists(p);
    if (c.empty()) return;
    string k = c.string();
    if (seen.insert(k).second) roots.push_back(c);
}

static vector<fs::path> discover_sysroots(const char* argv0) {
    vector<fs::path> roots;
    unordered_set<string> seen;
    if (const char* h = getenv("YONA_HOME")) {
        if (*h) push_unique_root(roots, seen, fs::path(h));
    }
    if (argv0 && *argv0) {
        auto exe = canonical_if_exists(fs::path(argv0).parent_path());
        if (!exe.empty()) {
            push_unique_root(roots, seen, exe);
            push_unique_root(roots, seen, exe.parent_path());
        }
    }
    push_unique_root(roots, seen, fs::current_path());
    push_unique_root(roots, seen, fs::current_path().parent_path());
    return roots;
}

static string compile_and_run(const string& expr, const string& rt_obj, const vector<string>& rt_extra_objs) {
    Codegen codegen("yona_repl");
    parser::Parser parser;
    istringstream stream(expr);
    auto parse_result = parser.parse_input(stream);
    if (!parse_result.node) return "Parse error";

    auto* llvm_mod = codegen.compile(parse_result.node.get());
    if (!llvm_mod) return ""; // errors already printed

    fs::path base = repl_temp_dir();
    fs::path obj = base / "yona_repl.o";
    fs::path exe = base / ("yona_repl" + exe_suffix());
    if (!codegen.emit_object_file(obj.string())) return "Codegen error";

    ostringstream link;
    link << repl_cc() << " " << q(obj) << " " << q(fs::path(rt_obj));
    for (const auto& ex : rt_extra_objs) link << " " << q(fs::path(ex));
    link << " -o " << q(exe);
#ifndef _WIN32
    link << " -lm -lpthread -rdynamic";
#else
    link << " -lws2_32 -ldbghelp";
#endif
    link << err_null();
    if (system(link.str().c_str()) != 0) return "Link error";
    fs::remove(obj);

    // Run and capture output
    array<char, 4096> buf;
    string output;
    FILE* pipe = popen(q(exe).c_str(), "r");
    if (!pipe) return "Exec error";
    while (fgets(buf.data(), buf.size(), pipe)) output += buf.data();
    pclose(pipe);
    fs::remove(exe);

    // Trim trailing newline
    if (!output.empty() && output.back() == '\n') output.pop_back();
    return output;
}

int main(int argc, char* argv[]) {
    vector<fs::path> sysroots = discover_sysroots(argc > 0 ? argv[0] : nullptr);
    string rt_obj;
    vector<string> rt_extra_objs;

    // Prefer packaged runtime objects from distribution roots.
    for (const auto& root : sysroots) {
        for (const auto& base : {root / "runtime", root / "lib" / "yona" / "runtime"}) {
            auto main_o = canonical_if_exists(base / "compiled_runtime.o");
            if (main_o.empty()) continue;
            rt_obj = main_o.string();
            for (const char* pf : platform_runtime_sources) {
                auto a = canonical_if_exists(base / ("crt_" + string(pf) + ".o"));
                auto b = canonical_if_exists(base / (string(pf) + ".o"));
                if (!a.empty()) rt_extra_objs.push_back(a.string());
                else if (!b.empty()) rt_extra_objs.push_back(b.string());
            }
            break;
        }
        if (!rt_obj.empty()) break;
    }

    if (rt_obj.empty()) {
        // Fall back to source compile from discovered roots.
        auto out_dir = canonical_if_exists(fs::path(argv[0]).parent_path());
        if (out_dir.empty()) out_dir = repl_temp_dir();
        for (const auto& root : sysroots) {
            auto src = root / "src" / "compiled_runtime.c";
            if (fs::exists(src)) {
                fs::path out_o = out_dir / "compiled_runtime.o";
                ostringstream cmd;
                cmd << repl_cc() << " -c " << q(src) << " -I" << q(root / "src") << " -I"
                    << q(root / "include") << " -o " << q(out_o) << err_null();
                if (system(cmd.str().c_str()) == 0) {
                    rt_obj = out_o.string();
                    for (const char* pf : platform_runtime_sources) {
                        fs::path plat_src = root / "src" / "runtime" / "platform" / pf;
                        if (!fs::exists(plat_src)) continue;
                        fs::path plat_o = out_dir / ("crt_" + string(pf) + ".o");
                        ostringstream pcmd;
                        pcmd << repl_cc() << " -c " << q(plat_src) << " -I" << q(root / "src")
                             << " -I" << q(root / "include") << " -o " << q(plat_o) << err_null();
                        if (system(pcmd.str().c_str()) == 0) {
                            rt_extra_objs.push_back(plat_o.string());
                        }
                    }
                }
                break;
            }
        }
    }

    if (rt_obj.empty()) {
        cerr << "Error: cannot find compiled_runtime.o" << endl;
        return 1;
    }

    cout << "Yona REPL (type expressions, Ctrl-D to exit)" << endl;

    string line;
    while (true) {
        cout << "yona> " << flush;
        if (!getline(cin, line)) break;
        if (line.empty()) continue;
        if (line == ":q" || line == ":quit") break;

        string result = compile_and_run(line, rt_obj, rt_extra_objs);
        if (!result.empty()) cout << result << endl;
    }

    return 0;
}
