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
//   yonac --explain E0100             # explain error code E0100

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
#include "typechecker/TypeChecker.h"
#include "typechecker/RefinementChecker.h"
#include "typechecker/LinearityChecker.h"

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
    int opt_level = 2;
    vector<string> include_paths;
    string explain_code;

    app.add_option("input", input_file, "Input .yona file");
    app.add_option("-e,--expression", expression, "Compile expression");
    app.add_option("-o,--output", output_file, "Output file");
    app.add_option("-I,--include", include_paths, "Module search paths (for .yonai files)");
    app.add_option("-O", opt_level, "Optimization level (0-3, default 2)")
       ->check(CLI::Range(0, 3));
    app.add_flag("--emit-ir", emit_ir, "Print LLVM IR instead of compiling");
    app.add_flag("--emit-obj", emit_obj, "Emit object file only (don't link)");
    app.add_flag("--Wall", flag_wall, "Enable common warnings");
    app.add_flag("--Wextra", flag_wextra, "Enable all warnings");
    app.add_flag("--Werror", flag_werror, "Treat warnings as errors");
    app.add_flag("-w", flag_w, "Suppress all warnings");
    app.add_flag("-g,--debug", flag_debug, "Emit DWARF debug information");
    app.add_option("--explain", explain_code, "Show detailed explanation for an error code (e.g., E0100)");

    CLI11_PARSE(app, argc, argv);

    // --explain: print explanation and exit
    if (!explain_code.empty()) {
        auto code = compiler::parse_error_code(explain_code);
        if (code) {
            string explanation = compiler::error_explanation(*code);
            if (!explanation.empty()) {
                cout << explanation << endl;
                return 0;
            }
        }
        cerr << "Unknown error code: " << explain_code << endl;
        return 1;
    }

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
    codegen.set_opt_level(opt_level);

    // Set module search paths for import resolution
    codegen.module_paths_ = include_paths;
    if (!input_file.empty()) {
        auto parent = filesystem::path(input_file).parent_path();
        if (!parent.empty())
            codegen.module_paths_.push_back(parent.string());
    }
    codegen.module_paths_.push_back(".");
    // Auto-discover lib/ for Prelude and stdlib (relative to cwd, exe, or common locations)
    for (auto& candidate : {"lib", "../lib", "../../lib", "../../../lib"}) {
        if (filesystem::exists(filesystem::path(candidate) / "Prelude.yonai")) {
            codegen.module_paths_.push_back(filesystem::canonical(candidate).string());
            break;
        }
    }
    // Also check relative to executable
    if (argc > 0) {
        auto exe_dir = filesystem::path(argv[0]).parent_path();
        for (auto& rel : {"../lib", "../../lib", "../../../lib"}) {
            auto candidate = exe_dir / rel;
            if (filesystem::exists(candidate / "Prelude.yonai")) {
                codegen.module_paths_.push_back(filesystem::canonical(candidate).string());
                break;
            }
        }
    }

    llvm::Module* llvm_mod = nullptr;

    if (is_module) {
        parser::Parser parser;
        codegen.load_prelude(&parser);  // registers constructors in parser
        auto result = parser.parse_module(source, filename);
        if (!result.has_value()) {
            for (auto& e : result.error())
                diag.error(e.location, compiler::ErrorCode::E0301, e.message);
            return 1;
        }
        llvm_mod = codegen.compile_module(result.value().get());
    } else {
        parser::Parser parser;
        typechecker::TypeChecker type_checker(diag);
        codegen.load_prelude(&parser, &type_checker);  // registers everything

        istringstream stream(source);
        auto parse_result = parser.parse_input(stream);
        if (!parse_result.node) {
            auto result = parser.parse_expression(source, filename);
            if (!result.has_value()) {
                for (auto& e : result.error())
                    diag.error(e.location, compiler::ErrorCode::E0301, e.message);
            } else {
                diag.error(SourceLocation::unknown(), compiler::ErrorCode::E0301, "parse error");
            }
            return 1;
        }

        type_checker.check(parse_result.node.get());
        if (type_checker.has_direct_errors()) {
            return 1;
        }
        codegen.set_type_checker(&type_checker);

        // Refinement checking (non-blocking)
        typechecker::RefinementChecker refinement_checker(diag, &type_checker);
        refinement_checker.check(parse_result.node.get());

        // Linearity checking (non-blocking)
        typechecker::LinearityChecker linearity_checker(diag);
        linearity_checker.check(parse_result.node.get());

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
        diag.error(SourceLocation::unknown(), compiler::ErrorCode::E0400, "failed to emit object file");
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
    string rt_bc = (exe_dir / "compiled_runtime.bc").string();

    // Find runtime source and compile to both .o (for linking) and .bc (for LTO)
    for (auto& dir : {".", "..", "../.."}) {
        auto candidate = filesystem::path(dir) / "src" / "compiled_runtime.c";
        if (filesystem::exists(candidate)) {
            string src_dir = (filesystem::path(dir) / "src").string();

            if (!filesystem::exists(rt_obj)) {
                string cmd = "cc -c " + candidate.string() + " -I" + src_dir + " -o " + rt_obj;
                system(cmd.c_str());
                // Compile platform layer
                for (auto& pf : {"file_linux.c", "net_linux.c", "os_linux.c"}) {
                    auto plat_src = filesystem::path(dir) / "src" / "runtime" / "platform" / pf;
                    if (filesystem::exists(plat_src)) {
                        string plat_obj = rt_obj + "." + string(pf) + ".o";
                        system(("cc -c " + plat_src.string() + " -I" + src_dir + " -o " + plat_obj).c_str());
                        system(("ld -r " + rt_obj + " " + plat_obj + " -o " + rt_obj + ".merged && mv " + rt_obj + ".merged " + rt_obj).c_str());
                        filesystem::remove(plat_obj);
                    }
                }
            }

            // Compile to LLVM bitcode for LTO (enables runtime function inlining).
            // Merge all runtime sources (main + platform) into one bitcode.
            // Regenerate if .bc doesn't exist or source is newer
            bool need_bc = !filesystem::exists(rt_bc);
            if (!need_bc && filesystem::exists(candidate)) {
                auto bc_time = filesystem::last_write_time(rt_bc);
                auto src_time = filesystem::last_write_time(candidate);
                if (src_time > bc_time) need_bc = true;
            }
            if (need_bc) {
                string bc_main = rt_bc + ".main";
                string bc_cmd = "clang -emit-llvm -O2 -c " + candidate.string() +
                    " -I" + src_dir + " -o " + bc_main + " 2>/dev/null";
                system(bc_cmd.c_str());

                // Compile platform files and link with llvm-link
                vector<string> bc_files = {bc_main};
                for (auto& pf : {"file_linux.c", "net_linux.c", "os_linux.c"}) {
                    auto plat_src = filesystem::path(dir) / "src" / "runtime" / "platform" / pf;
                    if (filesystem::exists(plat_src)) {
                        string plat_bc = rt_bc + "." + string(pf) + ".bc";
                        system(("clang -emit-llvm -O2 -c " + plat_src.string() +
                            " -I" + src_dir + " -o " + plat_bc + " 2>/dev/null").c_str());
                        bc_files.push_back(plat_bc);
                    }
                }
                // Link all bitcode files into one
                string link_bc = "llvm-link";
                for (auto& f : bc_files) link_bc += " " + f;
                link_bc += " -o " + rt_bc + " 2>/dev/null";
                system(link_bc.c_str());
                // Clean up intermediate files
                for (auto& f : bc_files) filesystem::remove(f);
            }
            break;
        }
    }

    // LTO: link runtime bitcode into the module before emitting object code.
    // This enables LLVM to inline seq_head, seq_tail, etc.
    bool lto_active = false;
    if (filesystem::exists(rt_bc)) {
        lto_active = codegen.link_runtime_bitcode(rt_bc);
        if (lto_active) {
            codegen.optimize();
            if (!codegen.emit_object_file(obj_file)) {
                diag.error(SourceLocation::unknown(), "failed to emit LTO object file");
                return 1;
            }
        }
    }

    // Find Prelude.o for linking
    string prelude_obj;
    for (auto& dir : codegen.module_paths_) {
        auto candidate = filesystem::path(dir) / "Prelude.o";
        if (filesystem::exists(candidate)) {
            prelude_obj = candidate.string();
            break;
        }
    }

    // When LTO merged the runtime, don't link rt_obj separately (avoid dups).
    // -rdynamic exports symbols for backtrace_symbols() stack traces.
    string link_cmd = lto_active
        ? "cc " + obj_file
        : "cc " + obj_file + " " + rt_obj;
    if (!prelude_obj.empty()) link_cmd += " " + prelude_obj;
    link_cmd += " -lm -lpthread -rdynamic -o " + output_file;
    int link_result = system(link_cmd.c_str());
    filesystem::remove(obj_file);

    if (link_result != 0) {
        diag.error(SourceLocation::unknown(), compiler::ErrorCode::E0401, "linking failed");
        return 1;
    }

    return 0;
}
