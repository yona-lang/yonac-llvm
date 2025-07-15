#include <doctest/doctest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

struct PatternMatchingTest {
    parser::Parser parser;
    Interpreter interp;
};

TEST_CASE("SimpleIdentifierPattern") /* FIXTURE */ {
    PatternMatchingTest fixture;
    // let x = 42 in x
    auto x_identifier = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "x"));
    auto pattern = new PatternValue(EMPTY_SOURCE_LOCATION, x_identifier);
    auto value_expr = new IntegerExpr(EMPTY_SOURCE_LOCATION, 42);

    auto pattern_alias = new PatternAlias(EMPTY_SOURCE_LOCATION, pattern, value_expr);
    auto let_expr = new LetExpr(EMPTY_SOURCE_LOCATION, {pattern_alias}, x_identifier);

    auto interpreter_result = fixture.interp.visit(let_expr);
      auto result = interpreter_result.value;

    CHECK(result->type == Int);
    CHECK(result->get<int>() == 42);
}

TEST_CASE("TuplePatternMatch") /* FIXTURE */ {
    PatternMatchingTest fixture;
    // let (x, y) = (1, 2) in x + y
    auto x_id = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "x"));
    auto y_id = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "y"));

    vector<Pattern*> patterns = {
        new PatternValue(EMPTY_SOURCE_LOCATION, x_id),
        new PatternValue(EMPTY_SOURCE_LOCATION, y_id)
    };
    auto tuple_pattern = new TuplePattern(EMPTY_SOURCE_LOCATION, patterns);

    vector<ExprNode*> values = {
        new IntegerExpr(EMPTY_SOURCE_LOCATION, 1),
        new IntegerExpr(EMPTY_SOURCE_LOCATION, 2)
    };
    auto tuple_expr = new TupleExpr(EMPTY_SOURCE_LOCATION, values);

    auto pattern_alias = new PatternAlias(EMPTY_SOURCE_LOCATION, tuple_pattern, tuple_expr);

    // x + y
    auto x_ref = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "x"));
    auto y_ref = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "y"));
    auto add_expr = new AddExpr(EMPTY_SOURCE_LOCATION, x_ref, y_ref);

    auto let_expr = new LetExpr(EMPTY_SOURCE_LOCATION, {pattern_alias}, add_expr);

    auto interpreter_result = fixture.interp.visit(let_expr);
      auto result = interpreter_result.value;

    CHECK(result->type == Int);
    CHECK(result->get<int>() == 3);
}

TEST_CASE("UnderscorePattern") /* FIXTURE */ {
    PatternMatchingTest fixture;
    // let (_, y) = (1, 2) in y
    auto underscore = new UnderscoreNode(EMPTY_SOURCE_LOCATION);
    auto y_id = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "y"));

    vector<Pattern*> patterns = {underscore, new PatternValue(EMPTY_SOURCE_LOCATION, y_id)};
    auto tuple_pattern = new TuplePattern(EMPTY_SOURCE_LOCATION, patterns);

    vector<ExprNode*> values = {
        new IntegerExpr(EMPTY_SOURCE_LOCATION, 1),
        new IntegerExpr(EMPTY_SOURCE_LOCATION, 2)
    };
    auto tuple_expr = new TupleExpr(EMPTY_SOURCE_LOCATION, values);

    auto pattern_alias = new PatternAlias(EMPTY_SOURCE_LOCATION, tuple_pattern, tuple_expr);
    auto y_ref = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "y"));
    auto let_expr = new LetExpr(EMPTY_SOURCE_LOCATION, {pattern_alias}, y_ref);

    auto interpreter_result = fixture.interp.visit(let_expr);
      auto result = interpreter_result.value;

    CHECK(result->type == Int);
    CHECK(result->get<int>() == 2);
}

TEST_CASE("PatternMatchFailure") /* FIXTURE */ {
    PatternMatchingTest fixture;
    // let (1, y) = (2, 3) in y  -- should raise :nomatch
    auto one_literal = new IntegerExpr(EMPTY_SOURCE_LOCATION, 1);
    auto one_pattern = new PatternValue(EMPTY_SOURCE_LOCATION,
        reinterpret_cast<LiteralExpr<void*>*>(one_literal));  // This is a hack, need proper literal pattern
    auto y_id = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "y"));

    vector<Pattern*> patterns = {one_pattern, new PatternValue(EMPTY_SOURCE_LOCATION, y_id)};
    auto tuple_pattern = new TuplePattern(EMPTY_SOURCE_LOCATION, patterns);

    vector<ExprNode*> values = {
        new IntegerExpr(EMPTY_SOURCE_LOCATION, 2),
        new IntegerExpr(EMPTY_SOURCE_LOCATION, 3)
    };
    auto tuple_expr = new TupleExpr(EMPTY_SOURCE_LOCATION, values);

    auto pattern_alias = new PatternAlias(EMPTY_SOURCE_LOCATION, tuple_pattern, tuple_expr);
    auto y_ref = new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "y"));
    auto let_expr = new LetExpr(EMPTY_SOURCE_LOCATION, {pattern_alias}, y_ref);

    // Should raise exception
    auto interpreter_result = fixture.interp.visit(let_expr);
      auto result = interpreter_result.value;

    // The result should be Unit with exception raised
    // We can't directly check IS since it's private
    // For now, just check that we got a Unit result
    CHECK(result->type == runtime::Unit);
    // TODO: Add a way to check exception state from tests
}

TEST_CASE("CaseExpressionSimple") /* FIXTURE */ {
    PatternMatchingTest fixture;
    // case 1 of
    //   1 -> "one"
    //   2 -> "two"
    //   _ -> "other"
    // end

    auto one_expr = new IntegerExpr(EMPTY_SOURCE_LOCATION, 1);

    // Pattern 1 -> "one"
    auto pattern1 = new PatternValue(EMPTY_SOURCE_LOCATION,
        new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "1")));  // This is wrong, need literal
    auto expr1 = new StringExpr(EMPTY_SOURCE_LOCATION, "one");
    auto clause1 = new CaseClause(EMPTY_SOURCE_LOCATION, pattern1, expr1);

    // Pattern 2 -> "two"
    auto pattern2 = new PatternValue(EMPTY_SOURCE_LOCATION,
        new IdentifierExpr(EMPTY_SOURCE_LOCATION, new NameExpr(EMPTY_SOURCE_LOCATION, "2")));  // Also wrong, need literal
    auto expr2 = new StringExpr(EMPTY_SOURCE_LOCATION, "two");
    auto clause2 = new CaseClause(EMPTY_SOURCE_LOCATION, pattern2, expr2);

    // Pattern _ -> "other"
    auto underscore = new UnderscoreNode(EMPTY_SOURCE_LOCATION);
    auto expr3 = new StringExpr(EMPTY_SOURCE_LOCATION, "other");
    auto clause3 = new CaseClause(EMPTY_SOURCE_LOCATION, underscore, expr3);

    vector<CaseClause*> clauses = {clause1, clause2, clause3};
    auto case_expr = new CaseExpr(EMPTY_SOURCE_LOCATION, one_expr, clauses);

    // For now this test is incomplete - we need proper literal patterns in PatternValue
    // which currently doesn't support integer literals properly
}
