// yonac — Yona compiler
//
// Compiles Yona source code to native executables via LLVM.
//
// Usage:
//   yonac input.yona                  # compile to a.out
//   yonac input.yona -o output        # compile to output
//   yonac -e "1 + 2"                  # compile expression
//   yonac --emit-ir -e "1 + 2"        # print LLVM IR
//   yonac --emit-obj -e "1 + 2"       # emit object file

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>

#include <CLI/CLI.hpp>
#include "Parser.h"
#include "Codegen.h"

using namespace std;
using namespace yona;
using namespace yona::compiler::codegen;

int main(int argc, char* argv[]) {
    CLI::App app{"yonac — Yona compiler"};

    string input_file;
    string expression;
    string output_file = "a.out";
    bool emit_ir = false;
    bool emit_obj = false;

    app.add_option("input", input_file, "Input .yona file");
    app.add_option("-e,--expression", expression, "Compile expression");
    app.add_option("-o,--output", output_file, "Output file");
    app.add_flag("--emit-ir", emit_ir, "Print LLVM IR instead of compiling");
    app.add_flag("--emit-obj", emit_obj, "Emit object file only (don't link)");

    CLI11_PARSE(app, argc, argv);

    // Get source code
    string source;
    if (!expression.empty()) {
        source = expression;
    } else if (!input_file.empty()) {
        ifstream file(input_file);
        if (!file.is_open()) {
            cerr << "Error: cannot open " << input_file << endl;
            return 1;
        }
        stringstream buf;
        buf << file.rdbuf();
        source = buf.str();
    } else {
        cerr << "Error: no input. Use 'yonac file.yona' or 'yonac -e \"expr\"'" << endl;
        return 1;
    }

    // Parse
    parser::Parser parser;
    istringstream stream(source);
    auto parse_result = parser.parse_input(stream);
    if (!parse_result.node) {
        cerr << "Parse error" << endl;
        return 1;
    }

    // Codegen
    Codegen codegen("yona_program");
    auto module = codegen.compile(parse_result.node.get());
    if (!module) {
        cerr << "Codegen error" << endl;
        return 1;
    }

    // Emit IR
    if (emit_ir) {
        cout << codegen.emit_ir();
        return 0;
    }

    // Emit object file
    string obj_file = emit_obj ? output_file : (output_file + ".o");
    if (!codegen.emit_object_file(obj_file)) {
        cerr << "Error: failed to emit object file" << endl;
        return 1;
    }

    if (emit_obj) {
        return 0;
    }

    // Link with runtime library
    // Find the runtime library relative to the executable
    auto exe_dir = filesystem::path(argv[0]).parent_path();
    string rt_obj = (exe_dir / "compiled_runtime.o").string();

    // If runtime object doesn't exist next to executable, try building it
    if (!filesystem::exists(rt_obj)) {
        // Try relative paths
        for (auto& dir : {".", "..", "../.."}) {
            auto candidate = filesystem::path(dir) / "src" / "compiled_runtime.c";
            if (filesystem::exists(candidate)) {
                string compile_cmd = "cc -c " + candidate.string() + " -o " + rt_obj;
                system(compile_cmd.c_str());
                break;
            }
        }
    }

    // Link
    string link_cmd = "cc " + obj_file + " " + rt_obj + " -o " + output_file;
    int link_result = system(link_cmd.c_str());

    // Clean up temp object file
    if (!emit_obj) {
        filesystem::remove(obj_file);
    }

    if (link_result != 0) {
        cerr << "Error: linking failed" << endl;
        return 1;
    }

    return 0;
}
