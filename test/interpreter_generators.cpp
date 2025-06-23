#include <gtest/gtest.h>
#include <algorithm>
#include "Interpreter.h"
#include "ast.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

static const SourceContext TestSrcCtx = EMPTY_SOURCE_LOCATION;

TEST(InterpreterGeneratorTest, SeqGeneratorTest) {
    // Create a source sequence [1, 2, 3]
    vector<ExprNode*> seq_values;
    seq_values.push_back(new IntegerExpr(TestSrcCtx, 1));
    seq_values.push_back(new IntegerExpr(TestSrcCtx, 2));
    seq_values.push_back(new IntegerExpr(TestSrcCtx, 3));
    auto source_seq = make_unique<ValuesSequenceExpr>(TestSrcCtx, seq_values);
    
    // Create a collection extractor with an identifier 'x'
    auto name = make_unique<NameExpr>(TestSrcCtx, "x");
    auto identifier = make_unique<IdentifierExpr>(TestSrcCtx, name.get());
    auto extractor = make_unique<ValueCollectionExtractorExpr>(TestSrcCtx, identifier.get());
    
    // Create a reducer expression that doubles the value: x * 2
    auto x_ref = make_unique<IdentifierExpr>(TestSrcCtx, name.get());
    auto two = make_unique<IntegerExpr>(TestSrcCtx, 2);
    auto reducer = make_unique<MultiplyExpr>(TestSrcCtx, x_ref.get(), two.get());
    
    // Create the sequence generator
    auto generator = make_unique<SeqGeneratorExpr>(TestSrcCtx, reducer.get(), extractor.get(), source_seq.get());
    
    Interpreter interpreter;
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(generator.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Seq);
    auto result_seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(result_seq->fields.size(), 3);
    EXPECT_EQ(result_seq->fields[0]->get<int>(), 2);
    EXPECT_EQ(result_seq->fields[1]->get<int>(), 4);
    EXPECT_EQ(result_seq->fields[2]->get<int>(), 6);
}

TEST(InterpreterGeneratorTest, SetGeneratorTest) {
    // Create a source set {1, 2, 3}
    vector<ExprNode*> set_values;
    set_values.push_back(new IntegerExpr(TestSrcCtx, 1));
    set_values.push_back(new IntegerExpr(TestSrcCtx, 2));
    set_values.push_back(new IntegerExpr(TestSrcCtx, 3));
    auto source_set = make_unique<SetExpr>(TestSrcCtx, set_values);
    
    // Create a collection extractor with an identifier 'x'
    auto name = make_unique<NameExpr>(TestSrcCtx, "x");
    auto identifier = make_unique<IdentifierExpr>(TestSrcCtx, name.get());
    auto extractor = make_unique<ValueCollectionExtractorExpr>(TestSrcCtx, identifier.get());
    
    // Create a reducer expression that adds 10: x + 10
    auto x_ref = make_unique<IdentifierExpr>(TestSrcCtx, name.get());
    auto ten = make_unique<IntegerExpr>(TestSrcCtx, 10);
    auto reducer = make_unique<AddExpr>(TestSrcCtx, x_ref.get(), ten.get());
    
    // Create the set generator
    auto generator = make_unique<SetGeneratorExpr>(TestSrcCtx, reducer.get(), extractor.get(), source_set.get());
    
    Interpreter interpreter;
    auto result = any_cast<RuntimeObjectPtr>(interpreter.visit(generator.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Set);
    auto result_set = result->get<shared_ptr<SetValue>>();
    ASSERT_EQ(result_set->fields.size(), 3);
    
    // Check that the set contains the expected values (order doesn't matter in sets)
    vector<int> values;
    for (const auto& field : result_set->fields) {
        values.push_back(field->get<int>());
    }
    sort(values.begin(), values.end());
    EXPECT_EQ(values[0], 11);
    EXPECT_EQ(values[1], 12);
    EXPECT_EQ(values[2], 13);
}