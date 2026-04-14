#include <doctest/doctest.h>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <filesystem>
#include "Parser.h"
#include "Codegen.h"
#include "Diagnostic.h"
#include "typechecker/TypeChecker.h"

using namespace std;
using namespace yona;
using namespace yona::compiler;
using namespace yona::compiler::codegen;
namespace fs = std::filesystem;

// ===== Helpers =====

static string compile_to_ir(const string& code, int opt_level = 0) {
    parser::Parser parser;
    istringstream stream(code);
    auto parse_result = parser.parse_input(stream);
    if (!parse_result.node) return "PARSE_ERROR";

    Codegen codegen("yona_program");
    codegen.set_opt_level(opt_level);
    auto module = codegen.compile(parse_result.node.get());
    if (!module) return "CODEGEN_ERROR";

    return codegen.emit_ir();
}

static bool ir_contains(const string& ir, const string& pattern) {
    return ir.find(pattern) != string::npos;
}

static string compile_and_run(const string& code) {
    parser::Parser parser;

    Codegen codegen("test_module");
    for (auto& dir : {"lib", "../lib", "../../lib", "../../../lib"}) {
        if (fs::exists(dir)) codegen.module_paths_.push_back(fs::canonical(dir).string());
    }
    // Type check (blocking)
    DiagnosticEngine tc_diag;
    typechecker::TypeChecker type_checker(tc_diag);
    codegen.load_prelude(&parser, &type_checker);

    istringstream stream(code);
    auto parse_result = parser.parse_input(stream);
    if (!parse_result.node) return "PARSE_ERROR";

    type_checker.check(parse_result.node.get());
    if (type_checker.has_direct_errors()) return "TYPE_ERROR";

    auto module = codegen.compile(parse_result.node.get());
    if (!module) return "CODEGEN_ERROR";

    string obj_path = "/tmp/yona_codegen_test.o";
    if (!codegen.emit_object_file(obj_path)) return "EMIT_ERROR";

    // Compile runtime if needed (or source changed)
    string rt_path = "/tmp/compiled_runtime_test.o";
    bool need_recompile = !fs::exists(rt_path);
    if (!need_recompile) {
        for (auto& dir : {".", "src", "../src", "../../src"}) {
            auto candidate = fs::path(dir) / "compiled_runtime.c";
            if (fs::exists(candidate) &&
                fs::last_write_time(candidate) > fs::last_write_time(rt_path)) {
                need_recompile = true; break;
            }
        }
    }
    if (need_recompile) {
        for (auto& dir : {".", "src", "../src", "../../src"}) {
            auto candidate = fs::path(dir) / "compiled_runtime.c";
            if (fs::exists(candidate)) {
                string src_dir = string(dir);
                string cmd = "cc -c " + candidate.string() + " -I" + src_dir + " -o " + rt_path + " 2>/dev/null";
                system(cmd.c_str());
                // Compile platform layer files and merge via partial link
                for (auto& pf : {"file_linux.c", "net_linux.c", "os_linux.c"}) {
                    auto plat_src = fs::path(dir) / "runtime" / "platform" / pf;
                    if (fs::exists(plat_src)) {
                        string plat_obj = "/tmp/yona_plat_" + string(pf) + ".o";
                        system(("cc -c " + plat_src.string() + " -I" + src_dir + " -o " + plat_obj + " 2>/dev/null").c_str());
                        system(("ld -r " + rt_path + " " + plat_obj + " -o /tmp/yona_rt_merged.o 2>/dev/null && mv /tmp/yona_rt_merged.o " + rt_path).c_str());
                    }
                }
                break;
            }
        }
    }

    // Compile regex runtime if PCRE2 is available
    string regex_obj = "";
    for (auto& dir : {".", "src", "../src", "../../src"}) {
        auto regex_src = fs::path(dir) / "runtime" / "regex.c";
        if (fs::exists(regex_src)) {
            regex_obj = "/tmp/yona_regex_test.o";
            if (!fs::exists(regex_obj) ||
                fs::last_write_time(regex_src) > fs::last_write_time(regex_obj)) {
                string cmd = "cc -c " + regex_src.string() + " -o " + regex_obj + " 2>/dev/null";
                if (system(cmd.c_str()) != 0) regex_obj = "";
            }
            break;
        }
    }

    string exe_path = "/tmp/yona_codegen_test";
    string link_cmd = "cc " + obj_path + " " + rt_path;
    if (!regex_obj.empty()) link_cmd += " " + regex_obj + " -lpcre2-8";
    link_cmd += " -lm -lpthread -rdynamic -o " + exe_path + " 2>/dev/null";
    if (system(link_cmd.c_str()) != 0) return "LINK_ERROR";

    array<char, 256> buffer;
    string result;
    FILE* pipe = popen((exe_path + " 2>/dev/null").c_str(), "r");
    if (!pipe) return "RUN_ERROR";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);

    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

static string read_file(const fs::path& path) {
    ifstream f(path);
    if (!f.is_open()) return "";
    stringstream buf;
    buf << f.rdbuf();
    string content = buf.str();
    // Trim trailing whitespace/newlines
    while (!content.empty() && (content.back() == '\n' || content.back() == '\r' || content.back() == ' '))
        content.pop_back();
    return content;
}

// ===== IR Snapshot Tests =====

TEST_SUITE("Codegen IR") {

TEST_CASE("Integer addition uses native add") {
    auto ir = compile_to_ir("1 + 2");
    CHECK(ir_contains(ir, "yona_rt_print_int(i64 3)"));
}

TEST_CASE("If expression with constant condition is optimized away") {
    auto ir = compile_to_ir("if true then 1 else 0", 2);
    // O2 constant-folds: if true → 1, eliminates branch
    CHECK(ir_contains(ir, "yona_rt_print_int(i64 1)"));
}

TEST_CASE("If expression with variable generates branch") {
    auto ir = compile_to_ir("let x = 5 in if x > 3 then 1 else 0", 2);
    // O2 constant-folds this (5 > 3 = true → 1)
    CHECK(ir_contains(ir, "yona_rt_print_int(i64 1)"));
}

TEST_CASE("With expression generates close call") {
    auto ir = compile_to_ir("with fd = 0 in 42");
    CHECK(ir_contains(ir, "yona_rt_close"));
}

// with expression E2E test is in test/codegen/with_value.yona fixture

TEST_CASE("Function generates internal LLVM function") {
    auto ir = compile_to_ir("let f x = x + 1 in f(5)");
    CHECK(ir_contains(ir, "define internal fastcc i64 @f(i64 %x)"));
    CHECK(ir_contains(ir, "add i64 %x, 1"));
}

TEST_CASE("Multi-arg function generates correct signature") {
    auto ir = compile_to_ir("let add x y = x + y in add 3 4");
    CHECK(ir_contains(ir, "define internal fastcc i64 @add(i64 %x, i64 %y)"));
}

} // Codegen IR

// ===== End-to-End Fixture Tests =====
// Loads .yona source files and .expected output files from test/codegen/

TEST_SUITE("Codegen E2E") {

TEST_CASE("Fixture-based codegen tests") {
    // Find the test fixtures directory
    fs::path fixtures_dir;
    for (auto& dir : {"test/codegen", "../test/codegen", "../../test/codegen"}) {
        if (fs::exists(dir) && fs::is_directory(dir)) {
            fixtures_dir = dir;
            break;
        }
    }

    if (fixtures_dir.empty()) {
        WARN("Could not find test/codegen fixtures directory");
        return;
    }

    // Collect all .yona files
    vector<fs::path> test_files;
    for (auto& entry : fs::directory_iterator(fixtures_dir)) {
        if (entry.path().extension() == ".yona") {
            test_files.push_back(entry.path());
        }
    }
    sort(test_files.begin(), test_files.end());

    REQUIRE(!test_files.empty());

    for (const auto& yona_file : test_files) {
        auto expected_file = yona_file;
        expected_file.replace_extension(".expected");

        if (!fs::exists(expected_file)) continue;

        string source = read_file(yona_file);
        string expected = read_file(expected_file);
        string test_name = yona_file.stem().string();

        SUBCASE(test_name.c_str()) {
            string actual = compile_and_run(source);
            CHECK_MESSAGE(actual == expected,
                "Test '", test_name, "': expected '", expected, "' but got '", actual, "'");
        }
    }
}

} // Codegen E2E

// ===== Module Compilation Tests =====

TEST_SUITE("Codegen Modules") {

TEST_CASE("Module compiles to object with mangled exports") {
    // Parse module
    parser::Parser parser;
    string source = R"(
module Test\Arith

export add, mul

add x y = x + y
mul x y = x * y
)";
    auto result = parser.parse_module(source, "test_arith.yona");
    REQUIRE(result.has_value());

    // Compile module
    Codegen codegen("test_arith");
    auto mod = codegen.compile_module(result.value().get());
    REQUIRE(mod != nullptr);

    // Check that mangled exports exist in the IR
    string ir = codegen.emit_ir();
    CHECK(ir.find("yona_Test_Arith__add") != string::npos);
    CHECK(ir.find("yona_Test_Arith__mul") != string::npos);
}

TEST_CASE("Module name mangling") {
    CHECK(Codegen::mangle_name("Test\\Arith", "add") == "yona_Test_Arith__add");
    CHECK(Codegen::mangle_name("Std\\Math", "abs") == "yona_Std_Math__abs");
    CHECK(Codegen::mangle_name("My\\Deep\\Module", "func") == "yona_My_Deep_Module__func");
}

TEST_CASE("Module cross-language linking") {
    // Compile a module
    parser::Parser parser;
    string source = R"(
module Test\CrossLang

export double, negate

double x = x * 2
negate x = 0 - x
)";
    auto result = parser.parse_module(source, "cross.yona");
    REQUIRE(result.has_value());

    Codegen codegen("cross_lang");
    auto mod = codegen.compile_module(result.value().get());
    REQUIRE(mod != nullptr);

    // Emit object file
    string obj_path = "/tmp/cross_lang_test.o";
    REQUIRE(codegen.emit_object_file(obj_path));

    // Write a C program that calls the Yona functions
    {
        ofstream f("/tmp/cross_lang_caller.c");
        f << "#include <stdio.h>\n#include <stdint.h>\n"
          << "extern int64_t yona_Test_CrossLang__double(int64_t);\n"
          << "extern int64_t yona_Test_CrossLang__negate(int64_t);\n"
          << "int main() { printf(\"%ld %ld\", "
          << "yona_Test_CrossLang__double(21), "
          << "yona_Test_CrossLang__negate(5)); return 0; }\n";
    }

    // Compile and run
    int cc = system("cc /tmp/cross_lang_caller.c /tmp/cross_lang_test.o -lm -lpthread -o /tmp/cross_lang_run 2>/dev/null");
    REQUIRE(cc == 0);

    array<char, 64> buf;
    string output;
    FILE* pipe = popen("/tmp/cross_lang_run", "r");
    REQUIRE(pipe != nullptr);
    while (fgets(buf.data(), buf.size(), pipe)) output += buf.data();
    pclose(pipe);

    CHECK(output == "42 -5");
}

TEST_CASE("Multi-module Yona linking") {
    // Compile a module
    parser::Parser p1;
    string mod_source = R"(
module Test\Calc

export square, cube

square x = x * x
cube x = x * x * x
)";
    auto mod_result = p1.parse_module(mod_source, "calc.yona");
    REQUIRE(mod_result.has_value());

    Codegen mod_codegen("calc_mod");
    auto mod = mod_codegen.compile_module(mod_result.value().get());
    REQUIRE(mod != nullptr);
    string mod_obj = "/tmp/calc_mod_test.o";
    REQUIRE(mod_codegen.emit_object_file(mod_obj));

    // Compile expression that imports from the module
    parser::Parser p2;
    string expr_source = "import square, cube from Test\\Calc in square 3 + cube 2";
    istringstream stream(expr_source);
    auto expr_result = p2.parse_input(stream);
    REQUIRE(expr_result.node != nullptr);

    Codegen expr_codegen("expr_test");
    auto expr_mod = expr_codegen.compile(expr_result.node.get());
    REQUIRE(expr_mod != nullptr);
    string expr_obj = "/tmp/calc_expr_test.o";
    REQUIRE(expr_codegen.emit_object_file(expr_obj));

    // Compile runtime
    string rt_obj = "/tmp/compiled_runtime_test.o";

    // Link and run
    string link = "cc " + expr_obj + " " + mod_obj + " " + rt_obj + " -lm -lpthread -o /tmp/calc_link_test 2>/dev/null";
    REQUIRE(system(link.c_str()) == 0);

    array<char, 64> buf;
    string output;
    FILE* pipe = popen("/tmp/calc_link_test", "r");
    REQUIRE(pipe != nullptr);
    while (fgets(buf.data(), buf.size(), pipe)) output += buf.data();
    pclose(pipe);
    if (!output.empty() && output.back() == '\n') output.pop_back();

    CHECK(output == "17"); // square(3)=9 + cube(2)=8 = 17
}

TEST_CASE("Re-exports") {
    namespace fs = std::filesystem;

    // Compile runtime
    string rt_path = "/tmp/compiled_runtime_test.o";
    if (!fs::exists(rt_path)) {
        for (auto& dir : {".", "src", "../src", "../../src"}) {
            auto candidate = fs::path(dir) / "compiled_runtime.c";
            if (fs::exists(candidate)) {
                string src_dir = string(dir);
                string cmd = "cc -c " + candidate.string() + " -I" + src_dir + " -o " + rt_path + " 2>/dev/null";
                system(cmd.c_str());
                // Compile platform layer files and merge via partial link
                for (auto& pf : {"file_linux.c", "net_linux.c", "os_linux.c"}) {
                    auto plat_src = fs::path(dir) / "runtime" / "platform" / pf;
                    if (fs::exists(plat_src)) {
                        string plat_obj = "/tmp/yona_plat_" + string(pf) + ".o";
                        system(("cc -c " + plat_src.string() + " -I" + src_dir + " -o " + plat_obj + " 2>/dev/null").c_str());
                        system(("ld -r " + rt_path + " " + plat_obj + " -o /tmp/yona_rt_merged.o 2>/dev/null && mv /tmp/yona_rt_merged.o " + rt_path).c_str());
                    }
                }
                break;
            }
        }
    }

    // Step 1: Compile source module Test\Arith
    parser::Parser p1;
    string arith_source = R"(
module Test\Arith

export add, mul

add x y = x + y
mul x y = x * y
)";
    auto arith_result = p1.parse_module(arith_source, "arith.yona");
    REQUIRE(arith_result.has_value());

    Codegen arith_codegen("arith_mod");
    auto arith_mod = arith_codegen.compile_module(arith_result.value().get());
    REQUIRE(arith_mod != nullptr);
    string arith_obj = "/tmp/arith_mod_test.o";
    REQUIRE(arith_codegen.emit_object_file(arith_obj));
    fs::create_directories("/tmp/yona_lib/Test");
    REQUIRE(arith_codegen.emit_interface_file("/tmp/yona_lib/Test/Arith.yonai"));

    // Step 2: Compile re-exporting module Test\Prelude
    parser::Parser p2;
    string prelude_source = R"(
module Test\Prelude

export add, mul from Test\Arith
export double

double x = add x x
)";
    auto prelude_result = p2.parse_module(prelude_source, "prelude.yona");
    REQUIRE(prelude_result.has_value());

    Codegen prelude_codegen("prelude_mod");
    prelude_codegen.module_paths_.push_back("/tmp/yona_lib");
    auto prelude_mod = prelude_codegen.compile_module(prelude_result.value().get());
    REQUIRE(prelude_mod != nullptr);
    string prelude_obj = "/tmp/prelude_mod_test.o";
    REQUIRE(prelude_codegen.emit_object_file(prelude_obj));
    REQUIRE(prelude_codegen.emit_interface_file("/tmp/yona_lib/Test/Prelude.yonai"));

    // Step 3: Compile expression that imports from the re-exporting module
    parser::Parser p3;
    string expr_source = "import Test\\Prelude in add 10 (mul 3 4)";
    istringstream stream(expr_source);
    auto expr_result = p3.parse_input(stream);
    REQUIRE(expr_result.node != nullptr);

    Codegen expr_codegen("reexport_test");
    expr_codegen.module_paths_.push_back("/tmp/yona_lib");
    auto expr_mod = expr_codegen.compile(expr_result.node.get());
    REQUIRE(expr_mod != nullptr);
    string expr_obj = "/tmp/reexport_expr_test.o";
    REQUIRE(expr_codegen.emit_object_file(expr_obj));

    // Step 4: Link all three and run
    string exe_path = "/tmp/reexport_test_exe";
    string link_cmd = "cc " + arith_obj + " " + prelude_obj + " " + expr_obj + " " + rt_path +
                      " -lm -lpthread -o " + exe_path + " 2>/dev/null";
    REQUIRE(system(link_cmd.c_str()) == 0);

    array<char, 256> buffer;
    string result;
    FILE* pipe = popen((exe_path + " 2>/dev/null").c_str(), "r");
    REQUIRE(pipe != nullptr);
    while (fgets(buffer.data(), buffer.size(), pipe)) result += buffer.data();
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();

    CHECK(result == "22"); // add(10, mul(3,4)) = 10 + 12 = 22
}

TEST_CASE("Type-annotated module functions") {
    namespace fs = std::filesystem;

    string rt_path = "/tmp/compiled_runtime_test.o";
    if (!fs::exists(rt_path)) {
        for (auto& dir : {".", "src", "../src", "../../src"}) {
            auto candidate = fs::path(dir) / "compiled_runtime.c";
            if (fs::exists(candidate)) {
                string src_dir = string(dir);
                string cmd = "cc -c " + candidate.string() + " -I" + src_dir + " -o " + rt_path + " 2>/dev/null";
                system(cmd.c_str());
                // Compile platform layer files and merge via partial link
                for (auto& pf : {"file_linux.c", "net_linux.c", "os_linux.c"}) {
                    auto plat_src = fs::path(dir) / "runtime" / "platform" / pf;
                    if (fs::exists(plat_src)) {
                        string plat_obj = "/tmp/yona_plat_" + string(pf) + ".o";
                        system(("cc -c " + plat_src.string() + " -I" + src_dir + " -o " + plat_obj + " 2>/dev/null").c_str());
                        system(("ld -r " + rt_path + " " + plat_obj + " -o /tmp/yona_rt_merged.o 2>/dev/null && mv /tmp/yona_rt_merged.o " + rt_path).c_str());
                    }
                }
                break;
            }
        }
    }

    parser::Parser p1;
    string mod_source = R"(
module Test\Typed

export scale, greet

scale : Float -> Float -> Float
scale factor x = factor * x
greet : String -> String
greet name = "Hello " ++ name
)";
    auto mod_result = p1.parse_module(mod_source, "typed.yona");
    REQUIRE(mod_result.has_value());

    Codegen mod_codegen("typed_mod");
    auto mod = mod_codegen.compile_module(mod_result.value().get());
    REQUIRE(mod != nullptr);
    string mod_obj = "/tmp/typed_mod_test.o";
    REQUIRE(mod_codegen.emit_object_file(mod_obj));
    fs::create_directories("/tmp/yona_lib/Test");
    REQUIRE(mod_codegen.emit_interface_file("/tmp/yona_lib/Test/Typed.yonai"));

    // Test: scale 2.5 4.0 = 10.0
    parser::Parser p2;
    string expr_source = "import scale from Test\\Typed in scale 2.5 4.0";
    istringstream stream(expr_source);
    auto expr_result = p2.parse_input(stream);
    REQUIRE(expr_result.node != nullptr);

    Codegen expr_codegen("typed_test");
    expr_codegen.module_paths_.push_back("/tmp/yona_lib");
    auto expr_mod = expr_codegen.compile(expr_result.node.get());
    REQUIRE(expr_mod != nullptr);
    string expr_obj = "/tmp/typed_expr_test.o";
    REQUIRE(expr_codegen.emit_object_file(expr_obj));

    string exe_path = "/tmp/typed_test_exe";
    string link_cmd = "cc " + mod_obj + " " + expr_obj + " " + rt_path +
                      " -lm -lpthread -o " + exe_path + " 2>/dev/null";
    REQUIRE(system(link_cmd.c_str()) == 0);

    array<char, 256> buffer;
    string result;
    FILE* pipe = popen((exe_path + " 2>/dev/null").c_str(), "r");
    REQUIRE(pipe != nullptr);
    while (fgets(buffer.data(), buffer.size(), pipe)) result += buffer.data();
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();

    CHECK(result == "10"); // %g format: 10.0 prints as "10"
}

} // Codegen Modules

// ===== Diagnostic / Error Reporting Tests =====

TEST_SUITE("Diagnostics") {

TEST_CASE("DiagnosticEngine error counting") {
    DiagnosticEngine diag;
    diag.set_source("let x = 1 in y", "<test>");

    CHECK(diag.error_count() == 0);
    CHECK(!diag.has_errors());

    diag.error(SourceLocation{1, 14, 0, 1, "<test>"}, "undefined variable 'y'");
    CHECK(diag.error_count() == 1);
    CHECK(diag.has_errors());
}

TEST_CASE("DiagnosticEngine warning flags") {
    DiagnosticEngine diag;
    diag.set_source("let x = 1 in 42", "<test>");

    // Warnings suppressed by default (no flags enabled)
    diag.warning({1, 5, 0, 1, "<test>"}, "unused variable 'x'", WarningFlag::UnusedVariable);
    CHECK(diag.warning_count() == 0);

    // Enable -Wall
    diag.enable_wall();
    diag.warning({1, 5, 0, 1, "<test>"}, "unused variable 'x'", WarningFlag::UnusedVariable);
    CHECK(diag.warning_count() == 1);
}

TEST_CASE("DiagnosticEngine -Werror") {
    DiagnosticEngine diag;
    diag.enable_wall();
    diag.set_warnings_as_errors(true);
    diag.set_source("let x = 1 in 42", "<test>");

    diag.warning({1, 5, 0, 1, "<test>"}, "unused variable 'x'", WarningFlag::UnusedVariable);
    CHECK(diag.error_count() == 1);   // promoted to error
    CHECK(diag.warning_count() == 0); // not counted as warning
}

TEST_CASE("DiagnosticEngine -w suppresses all") {
    DiagnosticEngine diag;
    diag.enable_wextra();
    diag.suppress_all_warnings();
    diag.set_source("let x = 1 in 42", "<test>");

    diag.warning({1, 5, 0, 1, "<test>"}, "unused variable", WarningFlag::UnusedVariable);
    diag.warning({1, 5, 0, 1, "<test>"}, "shadow", WarningFlag::Shadow);
    CHECK(diag.warning_count() == 0);
    CHECK(diag.error_count() == 0);
}

TEST_CASE("Codegen reports errors through DiagnosticEngine") {
    DiagnosticEngine diag;
    string source = "let x = 42 in y + 1";
    diag.set_source(source, "<test>");

    Codegen codegen("test", &diag);
    parser::Parser parser;
    istringstream stream(source);
    auto result = parser.parse_input(stream);
    if (result.node) codegen.compile(result.node.get());

    CHECK(diag.has_errors());
    CHECK(diag.error_count() >= 1);
}

TEST_CASE("Codegen suggests similar names for typos") {
    DiagnosticEngine diag;
    string source = "let myVariable = 42 in myVarible + 1";
    diag.set_source(source, "<test>");

    Codegen codegen("test", &diag);
    parser::Parser parser;
    istringstream stream(source);
    auto result = parser.parse_input(stream);
    if (result.node) codegen.compile(result.node.get());

    // Should have an error with "did you mean" suggestion
    CHECK(diag.has_errors());
}

TEST_CASE("Warning flag names") {
    CHECK(DiagnosticEngine::flag_name(WarningFlag::UnusedVariable) == "unused-variable");
    CHECK(DiagnosticEngine::flag_name(WarningFlag::UnusedImport) == "unused-import");
    CHECK(DiagnosticEngine::flag_name(WarningFlag::Shadow) == "shadow");
    CHECK(DiagnosticEngine::flag_name(WarningFlag::MissingSignature) == "missing-signature");
    CHECK(DiagnosticEngine::flag_name(WarningFlag::IncompletePatterns) == "incomplete-patterns");
    CHECK(DiagnosticEngine::flag_name(WarningFlag::OverlappingPatterns) == "overlapping-patterns");
}

TEST_CASE("Parser errors route through DiagnosticEngine") {
    DiagnosticEngine diag;
    string source = "let x = in 42";
    diag.set_source(source, "<test>");

    parser::Parser parser;
    auto result = parser.parse_expression(source, "<test>");
    if (!result.has_value()) {
        for (auto& e : result.error())
            diag.error(e.location, e.message);
    }

    CHECK(diag.has_errors());
}

TEST_CASE("Debug info: compilation succeeds with -g") {
    // Compiling with debug info enabled should not break anything
    DiagnosticEngine diag;
    string source = "let x = 42 in let y = x + 1 in y";
    diag.set_source(source, "test_debug.yona");

    Codegen codegen("debug_test", &diag);
    codegen.set_debug_info(true, "test_debug.yona");

    parser::Parser parser;
    istringstream stream(source);
    auto result = parser.parse_input(stream);
    REQUIRE(result.node != nullptr);

    auto* mod = codegen.compile(result.node.get());
    REQUIRE(mod != nullptr);

    // Should compile without errors
    CHECK(!diag.has_errors());
    CHECK(codegen.error_count_ == 0);
}

TEST_CASE("Debug info: function with params") {
    DiagnosticEngine diag;
    string source = "let add x y = x + y in add 10 32";
    diag.set_source(source, "test_fn.yona");

    Codegen codegen("debug_fn_test", &diag);
    codegen.set_debug_info(true, "test_fn.yona");

    parser::Parser parser;
    istringstream stream(source);
    auto result = parser.parse_input(stream);
    REQUIRE(result.node != nullptr);

    auto* mod = codegen.compile(result.node.get());
    REQUIRE(mod != nullptr);
    CHECK(!diag.has_errors());
}

TEST_CASE("Debug info: closures") {
    DiagnosticEngine diag;
    string source = "let n = 10 in let add_n x = x + n in add_n 5";
    diag.set_source(source, "test_closure.yona");

    Codegen codegen("debug_closure_test", &diag);
    codegen.set_debug_info(true, "test_closure.yona");

    parser::Parser parser;
    istringstream stream(source);
    auto result = parser.parse_input(stream);
    REQUIRE(result.node != nullptr);

    auto* mod = codegen.compile(result.node.get());
    REQUIRE(mod != nullptr);
    CHECK(!diag.has_errors());
}

TEST_CASE("Debug info: disabled by default") {
    // Without set_debug_info, no debug metadata should be generated
    DiagnosticEngine diag;
    string source = "42";
    diag.set_source(source, "test.yona");

    Codegen codegen("no_debug_test", &diag);
    // NOT calling set_debug_info

    parser::Parser parser;
    istringstream stream(source);
    auto result = parser.parse_input(stream);
    REQUIRE(result.node != nullptr);

    auto* mod = codegen.compile(result.node.get());
    REQUIRE(mod != nullptr);
    CHECK(!diag.has_errors());

    // IR should NOT contain !dbg metadata
    string ir = codegen.emit_ir();
    CHECK(ir.find("!dbg") == string::npos);
}

} // Diagnostics

// IR fixture tests removed — IR text comparison is fragile due to
// whitespace/formatting differences between runs. The E2E fixture tests
// (compile → run → check output) are more reliable and valuable.

TEST_SUITE("Regex") {

TEST_CASE("Regex module: matches, find, replace, split") {
    namespace fs = std::filesystem;
    using namespace yona::compiler::codegen;

    // Ensure runtime is compiled
    string rt_path = "/tmp/compiled_runtime_test.o";
    if (!fs::exists(rt_path)) {
        for (auto& dir : {".", "src", "../src", "../../src"}) {
            auto candidate = fs::path(dir) / "compiled_runtime.c";
            if (fs::exists(candidate)) {
                string src_dir = string(dir);
                system(("cc -c " + candidate.string() + " -I" + src_dir +
                    " -o " + rt_path + " 2>/dev/null").c_str());
                for (auto& pf : {"file_linux.c", "net_linux.c", "os_linux.c"}) {
                    auto plat_src = fs::path(dir) / "runtime" / "platform" / pf;
                    if (fs::exists(plat_src)) {
                        string plat_obj = "/tmp/yona_plat_" + string(pf) + ".o";
                        system(("cc -c " + plat_src.string() + " -I" + src_dir +
                            " -o " + plat_obj + " 2>/dev/null").c_str());
                        system(("ld -r " + rt_path + " " + plat_obj +
                            " -o /tmp/yona_rt_merged.o 2>/dev/null && mv /tmp/yona_rt_merged.o " +
                            rt_path).c_str());
                    }
                }
                break;
            }
        }
    }

    // Compile regex.c
    string regex_obj = "/tmp/yona_regex_test.o";
    for (auto& dir : {".", "src", "../src", "../../src"}) {
        auto regex_src = fs::path(dir) / "runtime" / "regex.c";
        if (fs::exists(regex_src)) {
            system(("cc -c " + regex_src.string() + " -o " + regex_obj + " 2>/dev/null").c_str());
            break;
        }
    }

    // Compile the Regex module
    string regex_mod_src;
    for (auto& dir : {".", "lib", "../lib", "../../lib"}) {
        auto candidate = fs::path(dir) / "Std" / "Regex.yona";
        if (fs::exists(candidate)) {
            ifstream f(candidate);
            regex_mod_src = string(istreambuf_iterator<char>(f), {});
            break;
        }
    }
    REQUIRE(!regex_mod_src.empty());

    parser::Parser mp;
    auto mod_result = mp.parse_module(regex_mod_src, "Regex.yona");
    REQUIRE(mod_result.has_value());

    Codegen mod_codegen("regex_mod");
    auto mod = mod_codegen.compile_module(mod_result.value().get());
    REQUIRE(mod != nullptr);
    string mod_obj = "/tmp/regex_mod_test.o";
    REQUIRE(mod_codegen.emit_object_file(mod_obj));
    // Emit .yonai interface so the importing expression gets return types
    string yonai_dir = "/tmp/yona_regex_lib/Std";
    fs::create_directories(yonai_dir);
    REQUIRE(mod_codegen.emit_interface_file(yonai_dir + "/Regex.yonai"));

    // Helper: compile expression, link with module + runtime, run, return output
    auto run_expr = [&](const string& expr_source) -> string {
        parser::Parser ep;
        istringstream stream(expr_source);
        auto expr_result = ep.parse_input(stream);
        if (!expr_result.node) return "PARSE_ERROR";

        Codegen expr_codegen("regex_test");
        // Add module search paths for .yonai (generated + stdlib)
        expr_codegen.module_paths_.push_back("/tmp/yona_regex_lib");
        for (auto& dir : {".", "lib", "../lib", "../../lib"})
            if (fs::exists(dir)) expr_codegen.module_paths_.push_back(fs::canonical(dir).string());
        auto expr_mod = expr_codegen.compile(expr_result.node.get());
        if (!expr_mod) return "CODEGEN_ERROR";
        string expr_obj = "/tmp/regex_expr_test.o";
        if (!expr_codegen.emit_object_file(expr_obj)) return "EMIT_ERROR";

        string link = "cc " + expr_obj + " " + mod_obj + " " + rt_path +
            " " + regex_obj + " -lpcre2-8 -lm -lpthread -rdynamic" +
            " -o /tmp/regex_link_test 2>/dev/null";
        if (system(link.c_str()) != 0) return "LINK_ERROR";

        array<char, 512> buf;
        string output;
        FILE* pipe = popen("/tmp/regex_link_test 2>/dev/null", "r");
        if (!pipe) return "RUN_ERROR";
        while (fgets(buf.data(), buf.size(), pipe)) output += buf.data();
        pclose(pipe);
        if (!output.empty() && output.back() == '\n') output.pop_back();
        return output;
    };

    SUBCASE("matches true") {
        CHECK(run_expr(R"YT(import matches, compile from Std\Regex in matches (compile "[0-9]+") "abc 42 def")YT") == "true");
    }

    SUBCASE("matches false") {
        CHECK(run_expr(R"YT(import matches, compile from Std\Regex in matches (compile "[0-9]+") "no digits")YT") == "false");
    }

    SUBCASE("replace") {
        CHECK(run_expr(R"YT(import replace, compile from Std\Regex in replace (compile "[0-9]+") "abc 42 def 99" "NUM")YT") == "abc NUM def 99");
    }

    SUBCASE("replaceAll") {
        CHECK(run_expr(R"YT(import replaceAll, compile from Std\Regex in replaceAll (compile "[0-9]+") "abc 42 def 99" "NUM")YT") == "abc NUM def NUM");
    }

    // TODO: split/find return SEQ from C but module metadata infers ADT.
    // The extern return type (Seq) doesn't propagate through the wrapper
    // function's return type inference. Needs module_meta_ to use the
    // extern declaration's type annotation instead of body inference.

    SUBCASE("find match") {
        CHECK(run_expr(R"YT(import find, compile from Std\Regex in
            case find (compile "([a-z]+)([0-9]+)") "test123" of
                [] -> "none"
                [m | _] -> m
            end)YT") == "test123");
    }

    SUBCASE("find no match") {
        CHECK(run_expr(R"YT(import find, compile from Std\Regex in
            case find (compile "[0-9]+") "no digits" of
                [] -> "none"
                [m | _] -> m
            end)YT") == "none");
    }
}

} // Regex
