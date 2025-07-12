#include <catch2/catch_test_macros.hpp>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include "common.h"
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

struct ModuleTest {
    parser::Parser parser;
    unique_ptr<Interpreter> interp;

    void SetUp() {
        // Set YONA_PATH to include test/code directory
        // Navigate from build directory to project root
        filesystem::path current = filesystem::current_path();
        filesystem::path test_code_path = current / "../../../test/code";
        string yona_path = filesystem::canonical(test_code_path).string();

#ifdef _WIN32
        _putenv_s("YONA_PATH", yona_path.c_str());
#else
        setenv("YONA_PATH", yona_path.c_str(), 1);
#endif

        // Create interpreter after setting YONA_PATH
        interp = make_unique<Interpreter>();
    }
};

TEST_CASE("SimpleModuleImport", "[ModuleTest]") {
    ModuleTest fixture;
    fixture.SetUp();
    // Test: import add from Test\Test in add(1, 2)
    stringstream code("import add from Test\\Test in add(1, 2)");

    auto parse_result = fixture.parser.parse_input(code);
    REQUIRE(parse_result.success); // Parse failed;
    REQUIRE(parse_result.node != nullptr); // Parse result is null;

    auto interpreter_result = fixture.interp->visit(parse_result.node.get());
      auto result = interpreter_result.value;

    CHECK(result->type == Int);
    CHECK(result->get<int>() == 3);
}

TEST_CASE("ImportWithAlias", "[ModuleTest]") {
    ModuleTest fixture;
    fixture.SetUp();
    // Test: import multiply as mult from Test\Test in mult(3, 4)
    stringstream code("import multiply as mult from Test\\Test in mult(3, 4)");

    auto parse_result = fixture.parser.parse_input(code);
    REQUIRE(parse_result.node != nullptr);

    auto interpreter_result = fixture.interp->visit(parse_result.node.get());
      auto result = interpreter_result.value;

    CHECK(result->type == Int);
    CHECK(result->get<int>() == 12);
}

TEST_CASE("ImportNonExportedFunction", "[ModuleTest]") {
    ModuleTest fixture;
    fixture.SetUp();
    // Test: import internal_func from Test\Test - should fail
    stringstream code("import internal_func from Test\\Test in internal_func(5)");

    auto parse_result = fixture.parser.parse_input(code);
    REQUIRE(parse_result.node != nullptr);

    CHECK_THROWS_AS(fixture.interp->visit(parse_result.node.get()), yona_error);
}

TEST_CASE("ModuleCaching", "[ModuleTest]") {
    ModuleTest fixture;
    fixture.SetUp();
    // Import same module twice, should use cache

    // First import
    stringstream code1("import add from Test\\Test in add(10, 20)");
    auto parse_result1 = fixture.parser.parse_input(code1);
    REQUIRE(parse_result1.node != nullptr);

    auto result1 = fixture.interp->visit(parse_result1.node.get()).value;
    CHECK(result1->type == Int);
    CHECK(result1->get<int>() == 30);

    // Second import - should use cached module
    stringstream code2("import multiply from Test\\Test in multiply(5, 6)");
    auto parse_result2 = fixture.parser.parse_input(code2);
    REQUIRE(parse_result2.node != nullptr);

    auto result2 = fixture.interp->visit(parse_result2.node.get()).value;
    CHECK(result2->type == Int);
    CHECK(result2->get<int>() == 30);
}
