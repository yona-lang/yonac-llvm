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

#include "Parser.h"
#include "Codegen.h"

using namespace std;
using namespace yona;
using namespace yona::compiler::codegen;

static string compile_and_run(const string& expr, const string& rt_obj) {
    Codegen codegen("yona_repl");
    parser::Parser parser;
    istringstream stream(expr);
    auto parse_result = parser.parse_input(stream);
    if (!parse_result.node) return "Parse error";

    auto* llvm_mod = codegen.compile(parse_result.node.get());
    if (!llvm_mod) return "";  // errors already printed

    string obj = "/tmp/yona_repl.o";
    string exe = "/tmp/yona_repl";
    if (!codegen.emit_object_file(obj)) return "Codegen error";

    string link = "cc " + obj + " " + rt_obj + " -lm -lpthread -rdynamic -o " + exe + " 2>/dev/null";
    if (system(link.c_str()) != 0) return "Link error";
    filesystem::remove(obj);

    // Run and capture output
    array<char, 4096> buf;
    string output;
    FILE* pipe = popen(exe.c_str(), "r");
    if (!pipe) return "Exec error";
    while (fgets(buf.data(), buf.size(), pipe)) output += buf.data();
    pclose(pipe);
    filesystem::remove(exe);

    // Trim trailing newline
    if (!output.empty() && output.back() == '\n') output.pop_back();
    return output;
}

int main(int argc, char* argv[]) {
    // Find compiled_runtime.o
    string rt_obj;
    auto exe_dir = filesystem::path(argv[0]).parent_path();
    for (auto& candidate : {
        (exe_dir / "compiled_runtime.o").string(),
        string("compiled_runtime.o"),
        string("out/build/x64-debug-linux/compiled_runtime.o")
    }) {
        if (filesystem::exists(candidate)) { rt_obj = candidate; break; }
    }

    if (rt_obj.empty()) {
        // Try to compile it
        for (auto& dir : {".", "..", "../.."}) {
            auto src = filesystem::path(dir) / "src" / "compiled_runtime.c";
            if (filesystem::exists(src)) {
                rt_obj = "/tmp/yona_repl_rt.o";
                string cmd = "cc -c " + src.string() + " -o " + rt_obj;
                system(cmd.c_str());
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

        string result = compile_and_run(line, rt_obj);
        if (!result.empty()) cout << result << endl;
    }

    return 0;
}
