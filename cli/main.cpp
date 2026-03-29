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
//   yonac module.yona                 # compile module to .o + .yonai
//   yonac -I lib main.yona            # compile with module search path
//   yonac -Wall -Werror main.yona     # enable warnings, treat as errors

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
#include "Diagnostic.h"

using namespace std;
using namespace yona;
using namespace yona::compiler;
using namespace yona::compiler::codegen;

static bool is_module_source(const string& source) {
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
    bool flag_wall = false;
    bool flag_wextra = false;
    bool flag_werror = false;
    bool flag_w = false;
    bool flag_debug = false;
    vector<string> include_paths;

    app.add_option("input", input_file, "Input .yona file");
    app.add_option("-e,--expression", expression, "Compile expression");
    app.add_option("-o,--output", output_file, "Output file");
    app.add_option("-I,--include", include_paths, "Module search paths (for .yonai files)");
    app.add_flag("--emit-ir", emit_ir, "Print LLVM IR instead of compiling");
    app.add_flag("--emit-obj", emit_obj, "Emit object file only (don't link)");
    app.add_flag("--Wall", flag_wall, "Enable common warnings");
    app.add_flag("--Wextra", flag_wextra, "Enable all warnings");
    app.add_flag("--Werror", flag_werror, "Treat warnings as errors");
    app.add_flag("-w", flag_w, "Suppress all warnings");
    app.add_flag("-g,--debug", flag_debug, "Emit DWARF debug information");

    CLI11_PARSE(app, argc, argv);

    // Get source code
    string source;
    string filename;
    if (!expression.empty()) {
        source = expression;
        filename = "<expression>";
    } else if (!input_file.empty()) {
        ifstream file(input_file);
        if (!file.is_open()) {
            cerr << "Error: cannot open " << input_file << endl;
            return 1;
        }
        stringstream buf;
        buf << file.rdbuf();
        source = buf.str();
        filename = input_file;
    } else {
        cerr << "Error: no input. Use 'yonac file.yona' or 'yonac -e \"expr\"'" << endl;
        return 1;
    }

    bool is_module = is_module_source(source);

    // Set default output
    if (output_file.empty()) {
        if (is_module || emit_obj) {
            if (!input_file.empty())
                output_file = filesystem::path(input_file).stem().string() + ".o";
            else
                output_file = "a.o";
        } else {
            output_file = "a.out";
        }
    }

    // Set up diagnostics
    DiagnosticEngine diag;
    diag.set_source(source, filename);
    if (flag_w)      diag.suppress_all_warnings();
    if (flag_wall)   diag.enable_wall();
    if (flag_wextra) diag.enable_wextra();
    if (flag_werror) diag.set_warnings_as_errors(true);

    // Codegen
    string module_name = is_module ? "yona_module" : "yona_program";
    Codegen codegen(module_name, &diag);

    if (flag_debug) codegen.set_debug_info(true, filename);

    // Set module search paths for import resolution
    codegen.module_paths_ = include_paths;
    if (!input_file.empty()) {
        auto parent = filesystem::path(input_file).parent_path();
        if (!parent.empty())
            codegen.module_paths_.push_back(parent.string());
    }
    codegen.module_paths_.push_back(".");

    llvm::Module* llvm_mod = nullptr;

    if (is_module) {
        parser::Parser parser;
        auto result = parser.parse_module(source, filename);
        if (!result.has_value()) {
            for (auto& e : result.error())
                diag.error(e.location, e.message);
            return 1;
        }
        llvm_mod = codegen.compile_module(result.value().get());
    } else {
        parser::Parser parser;
        istringstream stream(source);
        auto parse_result = parser.parse_input(stream);
        if (!parse_result.node) {
            // Try the modern interface for better error messages
            auto result = parser.parse_expression(source, filename);
            if (!result.has_value()) {
                for (auto& e : result.error())
                    diag.error(e.location, e.message);
            } else {
                diag.error(SourceLocation::unknown(), "parse error");
            }
            return 1;
        }
        llvm_mod = codegen.compile(parse_result.node.get());
    }

    if (!llvm_mod) {
        // Errors already printed by DiagnosticEngine
        return 1;
    }

    // Print summary if there were warnings
    if (diag.warning_count() > 0) {
        cerr << diag.warning_count() << " warning"
             << (diag.warning_count() != 1 ? "s" : "") << " generated." << endl;
    }

    if (emit_ir) {
        cout << codegen.emit_ir();
        return 0;
    }

    // Emit object file
    string obj_file = (is_module || emit_obj) ? output_file : (output_file + ".o");
    if (!codegen.emit_object_file(obj_file)) {
        diag.error(SourceLocation::unknown(), "failed to emit object file");
        return 1;
    }

    // For modules, also emit interface file (.yonai)
    if (is_module) {
        auto yonai_path = filesystem::path(output_file).replace_extension(".yonai");
        codegen.emit_interface_file(yonai_path.string());
        return 0;
    }

    if (emit_obj) return 0;

    // Link expression into executable
    auto exe_dir = filesystem::path(argv[0]).parent_path();
    string rt_obj = (exe_dir / "compiled_runtime.o").string();

    if (!filesystem::exists(rt_obj)) {
        for (auto& dir : {".", "..", "../.."}) {
            auto candidate = filesystem::path(dir) / "src" / "compiled_runtime.c";
            if (filesystem::exists(candidate)) {
                string src_dir = (filesystem::path(dir) / "src").string();
                string cmd = "cc -c " + candidate.string() + " -I" + src_dir + " -o " + rt_obj;
                system(cmd.c_str());
                break;
            }
        }
    }

    // -rdynamic exports symbols for backtrace_symbols() stack traces
    string link_cmd = "cc " + obj_file + " " + rt_obj + " -lm -lpthread -rdynamic -o " + output_file;
    int link_result = system(link_cmd.c_str());
    filesystem::remove(obj_file);

    if (link_result != 0) {
        diag.error(SourceLocation::unknown(), "linking failed");
        return 1;
    }

    return 0;
}
