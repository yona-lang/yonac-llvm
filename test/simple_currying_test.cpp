#include <gtest/gtest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST(SimpleCurryingTest, SingleLineCurrying) {
    parser::Parser parser;
    Interpreter interp;
    
    // Test without let expression
    stringstream ss("(\\(x) -> \\(y) -> x + y)(5)(3)");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.success);
    ASSERT_NE(parse_result.node, nullptr);
    
    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    ASSERT_NE(main, nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(main));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 8);
}

TEST(SimpleCurryingTest, PartialApplication) {
    parser::Parser parser;
    Interpreter interp;
    
    // Test partial application
    stringstream ss("(\\(x) -> \\(y) -> x + y)(5)");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.success);
    ASSERT_NE(parse_result.node, nullptr);
    
    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    ASSERT_NE(main, nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(main));
    
    // Result should be a function
    EXPECT_EQ(result->type, yona::interp::runtime::Function);
}