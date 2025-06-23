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

TEST(LambdaMinimalTest, JustLambda) {
    std::cerr << "TEST: Testing just lambda evaluation" << std::endl;
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("\\(x) -> x");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.success);
    ASSERT_NE(parse_result.node, nullptr);
    
    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    ASSERT_NE(main, nullptr);
    
    std::cerr << "TEST: About to evaluate lambda" << std::endl;
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(main));
    std::cerr << "TEST: Lambda evaluated" << std::endl;
    
    EXPECT_EQ(result->type, yona::interp::runtime::Function);
}