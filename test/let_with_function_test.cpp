#include <catch2/catch_test_macros.hpp>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <iostream>
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST_CASE("LetWithLambda", "[LetWithFunctionTest]") {
    parser::Parser parser;
    Interpreter interp;

    // Let with function binding
    stringstream ss("let f = \\(x) -> x in f(42)");
    cout << "Parsing: " << ss.str() << endl;
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    cout << "AST: " << *parse_result.node << endl;

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    cout << "About to evaluate..." << endl;
    auto interpreter_result = interp.visit(main);
      auto result = interpreter_result.value;
    cout << "Evaluation complete" << endl;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 42);
}

TEST_CASE("NestedLets", "[LetWithFunctionTest]") {
    parser::Parser parser;
    Interpreter interp;

    // Nested lets
    stringstream ss("let x = 5 in let y = 3 in x + y");
    cout << "Parsing: " << ss.str() << endl;
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    cout << "AST: " << *parse_result.node << endl;

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    cout << "About to evaluate..." << endl;
    auto interpreter_result = interp.visit(main);
      auto result = interpreter_result.value;
    cout << "Evaluation complete" << endl;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 8);
}

TEST_CASE("SimpleCurryingWithLet", "[LetWithFunctionTest]") {
    parser::Parser parser;
    Interpreter interp;

    // The problematic case from currying test
    stringstream ss("let add = \\(x) -> \\(y) -> x + y in let add5 = add(5) in add5(3)");
    cout << "Parsing: " << ss.str() << endl;
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    cout << "AST: " << *parse_result.node << endl;

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    cout << "About to evaluate..." << endl;
    auto interpreter_result = interp.visit(main);
      auto result = interpreter_result.value;
    cout << "Evaluation complete" << endl;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 8);
}
