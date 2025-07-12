#include <catch2/catch_test_macros.hpp>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST_CASE("SingleLineCurrying", "[SimpleCurryingTest]") {
    parser::Parser parser;
    Interpreter interp;

    // Test without let expression
    stringstream ss("(\\(x) -> \\(y) -> x + y)(5)(3)");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    auto interpreter_result = interp.visit(main);
      auto result = interpreter_result.value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 8);
}

TEST_CASE("PartialApplication", "[SimpleCurryingTest]") {
    parser::Parser parser;
    Interpreter interp;

    // Test partial application
    stringstream ss("(\\(x) -> \\(y) -> x + y)(5)");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    auto interpreter_result = interp.visit(main);
      auto result = interpreter_result.value;

    // Result should be a function
    CHECK(result->type == yona::interp::runtime::Function);
}
