#include <gtest/gtest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <iostream>

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST(SimpleLetTest, BasicLet) {
    parser::Parser parser;
    Interpreter interp;

    // Simple let binding
    stringstream ss("let x = 5 in x");
    cout << "Parsing: " << ss.str() << endl;
    auto parse_result = parser.parse_input(ss);

    ASSERT_TRUE(parse_result.success);
    ASSERT_NE(parse_result.node, nullptr);

    cout << "AST: " << *parse_result.node << endl;

    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    ASSERT_NE(main, nullptr);

    cout << "About to evaluate..." << endl;
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(main));
    cout << "Evaluation complete" << endl;

    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 5);
}
