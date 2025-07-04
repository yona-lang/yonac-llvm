#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "Interpreter.h"
#include "ast.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

static const SourceContext TestSrcCtx = EMPTY_SOURCE_LOCATION;

TEST_CASE("SeqGeneratorTest", "[InterpreterGeneratorTest]") {
    // Create a source sequence [1, 2, 3]
    vector<ExprNode*> seq_values;
    seq_values.push_back(new IntegerExpr(TestSrcCtx, 1));
    seq_values.push_back(new IntegerExpr(TestSrcCtx, 2));
    seq_values.push_back(new IntegerExpr(TestSrcCtx, 3));
    auto source_seq = new ValuesSequenceExpr(TestSrcCtx, seq_values);

    // Create a collection extractor with an identifier 'x'
    auto name = new NameExpr(TestSrcCtx, "x");
    auto identifier = new IdentifierExpr(TestSrcCtx, name);
    auto extractor = new ValueCollectionExtractorExpr(TestSrcCtx, identifier);

    // Create a reducer expression that doubles the value: x * 2
    auto x_ref = new IdentifierExpr(TestSrcCtx, new NameExpr(TestSrcCtx, "x"));
    auto two = new IntegerExpr(TestSrcCtx, 2);
    auto reducer = new MultiplyExpr(TestSrcCtx, x_ref, two);

    // Create the sequence generator
    auto generator = new SeqGeneratorExpr(TestSrcCtx, reducer, extractor, source_seq);
    auto main = make_unique<MainNode>(TestSrcCtx, generator);

    Interpreter interpreter;
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(main.get()));

    CHECK(result->type == yona::interp::runtime::Seq);
    auto result_seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(result_seq->fields.size() == 3);
    CHECK(result_seq->fields[0]->get<int>() == 2);
    CHECK(result_seq->fields[1]->get<int>() == 4);
    CHECK(result_seq->fields[2]->get<int>() == 6);
}

TEST_CASE("SetGeneratorTest", "[InterpreterGeneratorTest]") {
    // Create a source set {1, 2, 3}
    vector<ExprNode*> set_values;
    set_values.push_back(new IntegerExpr(TestSrcCtx, 1));
    set_values.push_back(new IntegerExpr(TestSrcCtx, 2));
    set_values.push_back(new IntegerExpr(TestSrcCtx, 3));
    auto source_set = new SetExpr(TestSrcCtx, set_values);

    // Create a collection extractor with an identifier 'x'
    auto name = new NameExpr(TestSrcCtx, "x");
    auto identifier = new IdentifierExpr(TestSrcCtx, name);
    auto extractor = new ValueCollectionExtractorExpr(TestSrcCtx, identifier);

    // Create a reducer expression that adds 10: x + 10
    auto x_ref = new IdentifierExpr(TestSrcCtx, new NameExpr(TestSrcCtx, "x"));
    auto ten = new IntegerExpr(TestSrcCtx, 10);
    auto reducer = new AddExpr(TestSrcCtx, x_ref, ten);

    // Create the set generator
    auto generator = new SetGeneratorExpr(TestSrcCtx, reducer, extractor, source_set);
    auto main = make_unique<MainNode>(TestSrcCtx, generator);

    Interpreter interpreter;
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(main.get()));

    CHECK(result->type == yona::interp::runtime::Set);
    auto result_set = result->get<shared_ptr<SetValue>>();
    REQUIRE(result_set->fields.size() == 3);

    // Check that the set contains the expected values (order doesn't matter in sets)
    vector<int> values;
    for (const auto& field : result_set->fields) {
        values.push_back(field->get<int>());
    }
    sort(values.begin(), values.end());
    CHECK(values[0] == 11);
    CHECK(values[1] == 12);
    CHECK(values[2] == 13);
}
