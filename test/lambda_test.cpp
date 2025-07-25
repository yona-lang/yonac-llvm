#include <sstream>
#include <doctest/doctest.h>
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

TEST_SUITE("Interpreter.Lambda") {

TEST_CASE("SimpleLambda") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("\\(x) -> x + 1");
    auto parse_result = parser.parse_input(ss);

    if (!parse_result.success || parse_result.node == nullptr) {
        if (parse_result.ast_ctx.hasErrors()) {
            for (const auto& [type, error] : parse_result.ast_ctx.getErrors()) {
                std::cerr << "Parse error: " << error->what() << std::endl;
            }
        }
        FAIL("Parse failed");
    }

    // MainNode wraps the expression
    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    auto interpreter_result = interp.visit(main);
      auto result = interpreter_result.value;

    CHECK(result->type == yona::interp::runtime::Function);
}

TEST_CASE("LambdaApplication") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("(\\(x) -> x + 1)(5)");
    auto parse_result = parser.parse_input(ss);

    if (!parse_result.success || parse_result.node == nullptr) {
        if (parse_result.ast_ctx.hasErrors()) {
            for (const auto& [type, error] : parse_result.ast_ctx.getErrors()) {
                std::cerr << "Parse error: " << error->what() << std::endl;
            }
        }
        FAIL("Parse failed");
    }

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    auto interpreter_result = interp.visit(main);
      auto result = interpreter_result.value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 6);
}

} // TEST_SUITE("Interpreter.Lambda")
