#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include "Parser.h"
#include "Interpreter.h"
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST_CASE("SimpleAddition", "[InterpreterDebugTest]") {
    try {
        // Create parser
        parser::Parser parser;

        // Parse simple addition
        stringstream ss("1 + 2");
        auto parse_result = parser.parse_input(ss);

        if (!parse_result.node) {
            FAIL("Failed to parse expression");
        }

        // Create interpreter
        Interpreter interpreter;

        // Execute
        auto interpreter_result = interpreter.visit(parse_result.node.get());
      auto result = interpreter_result.value;

        CHECK(result->type == yona::interp::runtime::Int);
        CHECK(result->get<int>() == 3);
    } catch (const exception& e) {
        FAIL("Exception thrown: " << e.what());
    }
}

TEST_CASE("MinimalTest", "[InterpreterDebugTest]") {
    try {
        // Create and destroy interpreter
        Interpreter interpreter;
        SUCCEED();
    } catch (const exception& e) {
        FAIL("Exception in minimal test: " << e.what());
    }
}
