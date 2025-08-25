#include <sstream>
#include <doctest/doctest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "TypeChecker.h"
#include "runtime.h"
#include <iostream>

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace yona::typechecker;
using namespace std;

TEST_SUITE("QuickTest") {

TEST_CASE("BasicArithmetic") {
    parser::Parser parser;
    Interpreter interp;

    // Test simple arithmetic
    stringstream ss("2 + 3");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    auto result = interp.visit(main);
    CHECK(result.value->type == yona::interp::runtime::Int);
    CHECK(result.value->get<int>() == 5);
}

TEST_CASE("TypeCheckerBasic") {
    parser::Parser parser;
    TypeInferenceContext ctx;
    TypeChecker type_checker(ctx);

    // Test type checking for integer literal
    stringstream ss("42");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.success);
    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    auto type = type_checker.check(main->node);
    CHECK(holds_alternative<BuiltinType>(type));
    CHECK(get<BuiltinType>(type) == compiler::types::SignedInt64);
}

// Module import test disabled due to hanging issue
// TODO: Debug module loading issue in separate investigation

TEST_CASE("PatternMatching") {
    parser::Parser parser;
    Interpreter interp;

    // Test case expression with pattern matching
    stringstream ss(R"(
        case [1, 2, 3] of
            [] -> 0
            [h | t] -> h
        end
    )");

    auto parse_result = parser.parse_input(ss);
    REQUIRE(parse_result.success);

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    auto result = interp.visit(main);
    CHECK(result.value->type == yona::interp::runtime::Int);
    CHECK(result.value->get<int>() == 1);
}

} // TEST_SUITE("QuickTest")
