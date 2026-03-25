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

static string compile_and_run_adt(const string& mod_source, const string& expr_source) {
    // Step 1: Compile the module
    parser::Parser p1;
    auto mod_result = p1.parse_module(mod_source, "adt_test.yona");
    if (!mod_result.has_value()) return "MOD_PARSE_ERROR";

    Codegen mod_codegen("adt_mod");
    auto mod = mod_codegen.compile_module(mod_result.value().get());
    if (!mod) return "MOD_CODEGEN_ERROR";

    string mod_obj = "/tmp/adt_mod_test.o";
    if (!mod_codegen.emit_object_file(mod_obj)) return "MOD_EMIT_ERROR";

    // Step 2: Compile the expression
    parser::Parser p2;
    istringstream stream(expr_source);
    auto expr_result = p2.parse_input(stream);
    if (!expr_result.node) return "EXPR_PARSE_ERROR";

    Codegen expr_codegen("adt_test");
    auto expr_mod = expr_codegen.compile(expr_result.node.get());
    if (!expr_mod) return "EXPR_CODEGEN_ERROR";

    string expr_obj = "/tmp/adt_expr_test.o";
    if (!expr_codegen.emit_object_file(expr_obj)) return "EXPR_EMIT_ERROR";

    // Step 3: Compile runtime
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

    // Step 4: Link and run
    string exe_path = "/tmp/adt_test_exe";
    string link_cmd = "cc " + mod_obj + " " + expr_obj + " " + rt_path +
                      " -lm -lpthread -o " + exe_path + " 2>/dev/null";
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

// ===== ADT Parsing Tests =====

TEST_SUITE("ADT") {

TEST_CASE("ADT declaration parses correctly") {
    parser::Parser parser;
    string source = R"(
module Test\Opt exports unwrap as
type Option a = Some a | None
unwrap x = x
end
)";
    auto result = parser.parse_module(source, "opt.yona");
    REQUIRE(result.has_value());
    auto* mod = result.value().get();
    REQUIRE(mod != nullptr);
    CHECK(mod->adt_declarations.size() == 1);
    CHECK(mod->adt_declarations[0]->name == "Option");
    CHECK(mod->adt_declarations[0]->type_params.size() == 1);
    CHECK(mod->adt_declarations[0]->type_params[0] == "a");
    CHECK(mod->adt_declarations[0]->variants.size() == 2);
    CHECK(mod->adt_declarations[0]->variants[0]->name == "Some");
    CHECK(mod->adt_declarations[0]->variants[0]->field_type_names.size() == 1);
    CHECK(mod->adt_declarations[0]->variants[1]->name == "None");
    CHECK(mod->adt_declarations[0]->variants[1]->field_type_names.size() == 0);
}

TEST_CASE("ADT with no type parameters parses") {
    parser::Parser parser;
    string source = R"(
module Test\Color exports id as
type Color = Red | Green | Blue
id x = x
end
)";
    auto result = parser.parse_module(source, "color.yona");
    REQUIRE(result.has_value());
    auto* mod = result.value().get();
    CHECK(mod->adt_declarations.size() == 1);
    CHECK(mod->adt_declarations[0]->name == "Color");
    CHECK(mod->adt_declarations[0]->type_params.empty());
    CHECK(mod->adt_declarations[0]->variants.size() == 3);
    CHECK(mod->adt_declarations[0]->variants[0]->name == "Red");
    CHECK(mod->adt_declarations[0]->variants[1]->name == "Green");
    CHECK(mod->adt_declarations[0]->variants[2]->name == "Blue");
}

TEST_CASE("Multiple ADT declarations in one module") {
    parser::Parser parser;
    string source = R"(
module Test\Types exports id as
type Option a = Some a | None
type Result a e = Ok a | Err e
id x = x
end
)";
    auto result = parser.parse_module(source, "types.yona");
    REQUIRE(result.has_value());
    auto* mod = result.value().get();
    CHECK(mod->adt_declarations.size() == 2);
    CHECK(mod->adt_declarations[0]->name == "Option");
    CHECK(mod->adt_declarations[1]->name == "Result");
    CHECK(mod->adt_declarations[1]->type_params.size() == 2);
    CHECK(mod->adt_declarations[1]->variants.size() == 2);
    CHECK(mod->adt_declarations[1]->variants[0]->name == "Ok");
    CHECK(mod->adt_declarations[1]->variants[0]->field_type_names.size() == 1);
    CHECK(mod->adt_declarations[1]->variants[1]->name == "Err");
    CHECK(mod->adt_declarations[1]->variants[1]->field_type_names.size() == 1);
}

TEST_CASE("ADT module compiles to IR") {
    parser::Parser parser;
    string source = R"(
module Test\Opt exports wrap, unwrap as
type Option a = Some a | None
wrap x = Some x
unwrap opt = case opt of
    Some v -> v
    None -> 0
end
end
)";
    auto result = parser.parse_module(source, "opt.yona");
    REQUIRE(result.has_value());

    Codegen codegen("adt_test");
    auto mod = codegen.compile_module(result.value().get());
    CHECK(mod != nullptr);
}

TEST_CASE("Zero-arity constructor compiles to constant") {
    parser::Parser parser;
    string source = R"(
module Test\Color exports red as
type Color = Red | Green | Blue
red = Red
end
)";
    auto result = parser.parse_module(source, "color.yona");
    REQUIRE(result.has_value());

    Codegen codegen("color_test");
    auto mod = codegen.compile_module(result.value().get());
    CHECK(mod != nullptr);

    string ir = codegen.emit_ir();
    // The IR should contain the mangled export
    CHECK(ir.find("yona_Test_Color__red") != string::npos);
}

TEST_CASE("Constructor pattern matching compiles") {
    parser::Parser parser;
    string source = R"(
module Test\Match exports check as
type Option a = Some a | None
check opt = case opt of
    Some v -> v
    None -> 0
end
end
)";
    auto result = parser.parse_module(source, "match.yona");
    REQUIRE(result.has_value());

    Codegen codegen("match_test");
    auto mod = codegen.compile_module(result.value().get());
    CHECK(mod != nullptr);
}

} // ADT
