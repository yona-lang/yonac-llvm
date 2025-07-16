#include <doctest/doctest.h>
#include "Interpreter.h"
#include "ast.h"
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

static const SourceContext TestSrcCtx = EMPTY_SOURCE_LOCATION;

TEST_CASE("RaiseExprTest") {
    auto symbol = new SymbolExpr(TestSrcCtx, "TestError");
    auto message = new StringExpr(TestSrcCtx, "This is a test error");
    auto raise = new RaiseExpr(TestSrcCtx, symbol, message);
    auto main = make_unique<MainNode>(TestSrcCtx, raise);

    Interpreter interpreter;

    // Execute the raise expression
    auto interpreter_result = interpreter.visit(main.get());
    auto test_result = interpreter_result.value;

    // The result should be Unit since an exception was raised
    CHECK(test_result->type == runtime::Unit);
}

TEST_CASE("TryCatchExprTest") {
    // Create a try expression that raises an exception
    auto symbol = new SymbolExpr(TestSrcCtx, "TestError");
    auto message = new StringExpr(TestSrcCtx, "This is a test error");
    auto raise = new RaiseExpr(TestSrcCtx, symbol, message);

    // Create a catch expression with underscore pattern (catch-all)
    // The pattern is: _ -> 42
    auto catch_value = new IntegerExpr(TestSrcCtx, 42);
    auto underscore_pattern = new UnderscoreNode(TestSrcCtx);
    auto pattern_without_guards = new PatternWithoutGuards(TestSrcCtx, catch_value);
    auto catch_pattern = new CatchPatternExpr(TestSrcCtx, underscore_pattern, pattern_without_guards);
    vector<CatchPatternExpr*> patterns = { catch_pattern };
    auto catch_expr = new CatchExpr(TestSrcCtx, patterns);

    // Create try-catch expression
    auto try_catch = new TryCatchExpr(TestSrcCtx, raise, catch_expr);
    auto main = make_unique<MainNode>(TestSrcCtx, try_catch);

    Interpreter interpreter;
    auto interpreter_result = interpreter.visit(main.get());
    auto test_result = interpreter_result.value;

    CHECK(test_result->type == runtime::Int);
    CHECK(test_result->get<int>() == 42);
}

TEST_CASE("TryCatchNoExceptionTest") {
    // Create a try expression that doesn't raise an exception
    auto try_value = new IntegerExpr(TestSrcCtx, 100);

    // Create a catch expression with underscore pattern
    auto catch_value = new IntegerExpr(TestSrcCtx, 42);
    auto underscore_pattern = new UnderscoreNode(TestSrcCtx);
    auto pattern_without_guards = new PatternWithoutGuards(TestSrcCtx, catch_value);
    auto catch_pattern = new CatchPatternExpr(TestSrcCtx, underscore_pattern, pattern_without_guards);
    vector<CatchPatternExpr*> patterns = { catch_pattern };
    auto catch_expr = new CatchExpr(TestSrcCtx, patterns);

    // Create try-catch expression
    auto try_catch = new TryCatchExpr(TestSrcCtx, try_value, catch_expr);
    auto main = make_unique<MainNode>(TestSrcCtx, try_catch);

    Interpreter interpreter;
    auto interpreter_result = interpreter.visit(main.get());
    auto test_result = interpreter_result.value;

    // Should return the try value, not the catch value
    CHECK(test_result->type == runtime::Int);
    CHECK(test_result->get<int>() == 100);
}
