// yonac — Yona compiler
//
// Compiles Yona source code to native executables or object files via LLVM.
//
// Usage:
//   yonac input.yona                  # compile expression to executable
//   yonac input.yona -o output        # compile to output
//   yonac -e "1 + 2"                  # compile expression
//   yonac --emit-ir -e "1 + 2"        # print LLVM IR
//   yonac --emit-obj -e "1 + 2"       # emit object file
//   yonac module.yona --emit-obj      # compile module to object file
//
// Module files (starting with 'module' keyword) compile to object files
// with externally-visible functions. Expression files compile to executables.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <algorithm>

#include <CLI/CLI.hpp>
#include "Parser.h"
#include "Codegen.h"

using namespace std;
using namespace yona;
using namespace yona::compiler::codegen;

// Check if source code is a module (starts with 'module' keyword)
static bool is_module_source(const string& source) {
    // Skip whitespace and find the first token
    auto it = source.begin();
    while (it != source.end() && (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r'))
        ++it;
    string prefix(it, min(it + 6, source.end()));
    return prefix == "module";
}

int main(int argc, char* argv[]) {
    CLI::App app{"yonac — Yona compiler"};

    string input_file;
    string expression;
    string output_file;
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

    bool is_module = is_module_source(source);

    // Set default output
    if (output_file.empty()) {
        if (is_module || emit_obj) {
            // Module or --emit-obj: default to .o
            if (!input_file.empty())
                output_file = filesystem::path(input_file).stem().string() + ".o";
            else
                output_file = "a.o";
        } else {
            output_file = "a.out";
        }
    }

    // Codegen
    string module_name = is_module ? "yona_module" : "yona_program";
    Codegen codegen(module_name);
    llvm::Module* llvm_mod = nullptr;

    if (is_module) {
        // Parse as module
        parser::Parser parser;
        auto result = parser.parse_module(source, input_file.empty() ? "<expression>" : input_file);
        if (!result.has_value()) {
            cerr << "Parse error";
            for (auto& e : result.error()) cerr << ": " << e.message;
            cerr << endl;
            return 1;
        }
        llvm_mod = codegen.compile_module(result.value().get());
    } else {
        // Parse as expression
        parser::Parser parser;
        istringstream stream(source);
        auto parse_result = parser.parse_input(stream);
        if (!parse_result.node) {
            cerr << "Parse error" << endl;
            return 1;
        }
        llvm_mod = codegen.compile(parse_result.node.get());
    }

    if (!llvm_mod) {
        cerr << "Codegen error" << endl;
        return 1;
    }

    // Emit IR
    if (emit_ir) {
        cout << codegen.emit_ir();
        return 0;
    }

    // Emit object file
    string obj_file = (is_module || emit_obj) ? output_file : (output_file + ".o");
    if (!codegen.emit_object_file(obj_file)) {
        cerr << "Error: failed to emit object file" << endl;
        return 1;
    }

    // For modules, we're done (modules produce .o files, not executables)
    if (is_module || emit_obj) {
        return 0;
    }

    // Link expression into executable
    auto exe_dir = filesystem::path(argv[0]).parent_path();
    string rt_obj = (exe_dir / "compiled_runtime.o").string();

    if (!filesystem::exists(rt_obj)) {
        for (auto& dir : {".", "..", "../.."}) {
            auto candidate = filesystem::path(dir) / "src" / "compiled_runtime.c";
            if (filesystem::exists(candidate)) {
                string cmd = "cc -c " + candidate.string() + " -o " + rt_obj;
                system(cmd.c_str());
                break;
            }
        }
    }

    string link_cmd = "cc " + obj_file + " " + rt_obj + " -lm -o " + output_file;
    int link_result = system(link_cmd.c_str());
    filesystem::remove(obj_file);

    if (link_result != 0) {
        cerr << "Error: linking failed" << endl;
        return 1;
    }

    return 0;
}
