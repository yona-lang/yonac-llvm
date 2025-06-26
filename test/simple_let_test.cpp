#include <catch2/catch_test_macros.hpp>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <iostream>

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST_CASE("BasicLet", "[SimpleLetTest]") {
    parser::Parser parser;
    Interpreter interp;

    // Simple let binding
    stringstream ss("let x = 5 in x");
    cout << "Parsing: " << ss.str() << endl;
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    cout << "AST: " << *parse_result.node << endl;

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    cout << "About to evaluate..." << endl;
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(main));
    cout << "Evaluation complete" << endl;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 5);
}
