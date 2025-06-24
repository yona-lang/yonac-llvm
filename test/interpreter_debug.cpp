#include <gtest/gtest.h>
#include <iostream>
#include "Parser.h"
#include "Interpreter.h"

using namespace yona;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST(InterpreterDebugTest, SimpleAddition) {
    try {
        // Create parser
        parser::Parser parser;

        // Parse simple addition
        stringstream ss("1 + 2");
        auto parse_result = parser.parse_input(ss);

        if (!parse_result.node) {
            FAIL() << "Failed to parse expression";
        }

        // Create interpreter
        Interpreter interpreter;

        // Execute
        auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(parse_result.node.get()));

        EXPECT_EQ(result->type, yona::interp::runtime::Int);
        EXPECT_EQ(result->get<int>(), 3);
    } catch (const exception& e) {
        FAIL() << "Exception thrown: " << e.what();
    }
}

TEST(InterpreterDebugTest, MinimalTest) {
    try {
        // Create and destroy interpreter
        Interpreter interpreter;
        SUCCEED();
    } catch (const exception& e) {
        FAIL() << "Exception in minimal test: " << e.what();
    }
}
