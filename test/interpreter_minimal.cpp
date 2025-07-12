#include <catch2/catch_test_macros.hpp>
#include "Interpreter.h"
#include "ast.h"
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

static const SourceContext TestSrcCtx = EMPTY_SOURCE_LOCATION;

TEST_CASE("DirectASTTest", "[InterpreterMinimalTest]") {
    // Create AST nodes directly without parsing
    auto left = new IntegerExpr(TestSrcCtx, 10);
    auto right = new IntegerExpr(TestSrcCtx, 20);
    auto add = new AddExpr(TestSrcCtx, left, right);
    auto main = make_unique<MainNode>(TestSrcCtx, add);

    // Create interpreter
    Interpreter interpreter;

    // Execute
    auto interpreter_result = interpreter.visit(main.get());
      auto result = interpreter_result.value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 30);
}

TEST_CASE("IntegerLiteralTest", "[InterpreterMinimalTest]") {
    auto integer = make_unique<IntegerExpr>(TestSrcCtx, 42);
    Interpreter interpreter;

    auto interpreter_result = interpreter.visit(integer.get());
      auto result = interpreter_result.value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 42);
}

TEST_CASE("BooleanLiteralTest", "[InterpreterMinimalTest]") {
    auto true_lit = make_unique<TrueLiteralExpr>(TestSrcCtx);
    Interpreter interpreter;

    auto interpreter_result = interpreter.visit(true_lit.get());
      auto result = interpreter_result.value;

    CHECK(result->type == yona::interp::runtime::Bool);
    CHECK(result->get<bool>());
}

TEST_CASE("ComparisonTest", "[InterpreterMinimalTest]") {
    auto left = new IntegerExpr(TestSrcCtx, 10);
    auto right = new IntegerExpr(TestSrcCtx, 10);
    auto eq = new EqExpr(TestSrcCtx, left, right);
    auto main = make_unique<MainNode>(TestSrcCtx, eq);

    Interpreter interpreter;
    auto interpreter_result = interpreter.visit(main.get());
      auto result = interpreter_result.value;

    CHECK(result->type == yona::interp::runtime::Bool);
    CHECK(result->get<bool>());
}

TEST_CASE("IfExpressionTest", "[InterpreterMinimalTest]") {
    auto condition = new TrueLiteralExpr(TestSrcCtx);
    auto then_expr = new IntegerExpr(TestSrcCtx, 100);
    auto else_expr = new IntegerExpr(TestSrcCtx, 200);
    auto if_expr = new IfExpr(TestSrcCtx, condition, then_expr, else_expr);
    auto main = make_unique<MainNode>(TestSrcCtx, if_expr);

    Interpreter interpreter;
    auto interpreter_result = interpreter.visit(main.get());
      auto result = interpreter_result.value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 100);
}
