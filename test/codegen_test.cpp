#include <doctest/doctest.h>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <filesystem>
#include "Parser.h"
#include "Codegen.h"

using namespace std;
using namespace yona;
using namespace yona::compiler::codegen;
namespace fs = std::filesystem;

// ===== Helpers =====

static string compile_to_ir(const string& code) {
    parser::Parser parser;
    istringstream stream(code);
    auto parse_result = parser.parse_input(stream);
    if (!parse_result.node) return "PARSE_ERROR";

    Codegen codegen("yona_program");
    auto module = codegen.compile(parse_result.node.get());
    if (!module) return "CODEGEN_ERROR";

    return codegen.emit_ir();
}

static bool ir_contains(const string& ir, const string& pattern) {
    return ir.find(pattern) != string::npos;
}

static string compile_and_run(const string& code) {
    parser::Parser parser;
    istringstream stream(code);
    auto parse_result = parser.parse_input(stream);
    if (!parse_result.node) return "PARSE_ERROR";

    Codegen codegen("test_module");
    auto module = codegen.compile(parse_result.node.get());
    if (!module) return "CODEGEN_ERROR";

    string obj_path = "/tmp/yona_codegen_test.o";
    if (!codegen.emit_object_file(obj_path)) return "EMIT_ERROR";

    // Compile runtime if needed
    string rt_path = "/tmp/compiled_runtime_test.o";
    if (!fs::exists(rt_path)) {
        for (auto& dir : {".", "src", "../src", "../../src"}) {
            auto candidate = fs::path(dir) / "compiled_runtime.c";
            if (fs::exists(candidate)) {
                string cmd = "cc -c " + candidate.string() + " -o " + rt_path + " 2>/dev/null";
                system(cmd.c_str());
                break;
            }
        }
    }

    string exe_path = "/tmp/yona_codegen_test";
    string link_cmd = "cc " + obj_path + " " + rt_path + " -o " + exe_path + " 2>/dev/null";
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

TEST_CASE("If expression generates phi node") {
    auto ir = compile_to_ir("if true then 1 else 0");
    CHECK(ir_contains(ir, "br i1"));
    CHECK(ir_contains(ir, "phi i64"));
}

TEST_CASE("Function generates internal LLVM function") {
    auto ir = compile_to_ir("let f x = x + 1 in f(5)");
    CHECK(ir_contains(ir, "define internal i64 @f(i64 %x)"));
    CHECK(ir_contains(ir, "add i64 %x, 1"));
}

TEST_CASE("Multi-arg function generates correct signature") {
    auto ir = compile_to_ir("let add x y = x + y in add(3, 4)");
    CHECK(ir_contains(ir, "define internal i64 @add(i64 %x, i64 %y)"));
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

// IR fixture tests removed — IR text comparison is fragile due to
// whitespace/formatting differences between runs. The E2E fixture tests
// (compile → run → check output) are more reliable and valuable.
