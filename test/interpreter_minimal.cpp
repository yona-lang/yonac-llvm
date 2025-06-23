#include <gtest/gtest.h>
#include "Interpreter.h"
#include "ast.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

static const SourceContext TestSrcCtx = EMPTY_SOURCE_LOCATION;

TEST(InterpreterMinimalTest, DirectASTTest) {
    // Create AST nodes directly without parsing
    auto left = new IntegerExpr(TestSrcCtx, 10);
    auto right = new IntegerExpr(TestSrcCtx, 20);
    auto add = new AddExpr(TestSrcCtx, left, right);
    auto main = make_unique<MainNode>(TestSrcCtx, add);
    
    // Create interpreter
    Interpreter interpreter;
    
    // Execute
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(main.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 30);
}

TEST(InterpreterMinimalTest, IntegerLiteralTest) {
    auto integer = make_unique<IntegerExpr>(TestSrcCtx, 42);
    Interpreter interpreter;
    
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(integer.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 42);
}

TEST(InterpreterMinimalTest, BooleanLiteralTest) {
    auto true_lit = make_unique<TrueLiteralExpr>(TestSrcCtx);
    Interpreter interpreter;
    
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(true_lit.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Bool);
    EXPECT_TRUE(result->get<bool>());
}

TEST(InterpreterMinimalTest, ComparisonTest) {
    auto left = new IntegerExpr(TestSrcCtx, 10);
    auto right = new IntegerExpr(TestSrcCtx, 10);
    auto eq = new EqExpr(TestSrcCtx, left, right);
    auto main = make_unique<MainNode>(TestSrcCtx, eq);
    
    Interpreter interpreter;
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(main.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Bool);
    EXPECT_TRUE(result->get<bool>());
}

TEST(InterpreterMinimalTest, IfExpressionTest) {
    auto condition = new TrueLiteralExpr(TestSrcCtx);
    auto then_expr = new IntegerExpr(TestSrcCtx, 100);
    auto else_expr = new IntegerExpr(TestSrcCtx, 200);
    auto if_expr = new IfExpr(TestSrcCtx, condition, then_expr, else_expr);
    auto main = make_unique<MainNode>(TestSrcCtx, if_expr);
    
    Interpreter interpreter;
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(main.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 100);
}