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

TEST(LetWithFunctionTest, LetWithLambda) {
    parser::Parser parser;
    Interpreter interp;
    
    // Let with function binding
    stringstream ss("let f = \\(x) -> x in f(42)");
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
    EXPECT_EQ(result->get<int>(), 42);
}

TEST(LetWithFunctionTest, NestedLets) {
    parser::Parser parser;
    Interpreter interp;
    
    // Nested lets
    stringstream ss("let x = 5 in let y = 3 in x + y");
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
    EXPECT_EQ(result->get<int>(), 8);
}

TEST(LetWithFunctionTest, SimpleCurryingWithLet) {
    parser::Parser parser;
    Interpreter interp;
    
    // The problematic case from currying test
    stringstream ss("let add = \\(x) -> \\(y) -> x + y in let add5 = add(5) in add5(3)");
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
    EXPECT_EQ(result->get<int>(), 8);
}