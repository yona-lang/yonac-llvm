#include <gtest/gtest.h>
#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"

using namespace std;
using namespace yona::parser;
using namespace yona::interp;

TEST(RangeSequenceTest, SimpleRange) {
  Parser parser;
  Interpreter interp;
  
  // Test simple range: [1..5]
  stringstream ss("[1..5]");
  auto parse_result = parser.parse_input(ss);
  
  ASSERT_TRUE(parse_result.node != nullptr);
  
  auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
  
  EXPECT_EQ(result->type, runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  ASSERT_EQ(seq_value->fields.size(), 5);
  
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(seq_value->fields[i]->type, runtime::Int);
    EXPECT_EQ(seq_value->fields[i]->get<int>(), i + 1);
  }
}

TEST(RangeSequenceTest, RangeWithStep) {
  Parser parser;
  Interpreter interp;
  
  // Test range with step: [1..10..2] (1, 3, 5, 7, 9)
  stringstream ss("[1..10..2]");
  auto parse_result = parser.parse_input(ss);
  
  ASSERT_TRUE(parse_result.node != nullptr);
  
  auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
  
  EXPECT_EQ(result->type, runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  ASSERT_EQ(seq_value->fields.size(), 5);
  
  vector<int> expected = {1, 3, 5, 7, 9};
  for (size_t i = 0; i < expected.size(); i++) {
    EXPECT_EQ(seq_value->fields[i]->type, runtime::Int);
    EXPECT_EQ(seq_value->fields[i]->get<int>(), expected[i]);
  }
}

TEST(RangeSequenceTest, ReverseRange) {
  Parser parser;
  Interpreter interp;
  
  // Test reverse range: [5..1]
  stringstream ss("[5..1]");
  auto parse_result = parser.parse_input(ss);
  
  ASSERT_TRUE(parse_result.node != nullptr);
  
  auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
  
  EXPECT_EQ(result->type, runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  ASSERT_EQ(seq_value->fields.size(), 5);
  
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(seq_value->fields[i]->type, runtime::Int);
    EXPECT_EQ(seq_value->fields[i]->get<int>(), 5 - i);
  }
}

TEST(RangeSequenceTest, ReverseRangeWithNegativeStep) {
  Parser parser;
  Interpreter interp;
  
  // Test reverse range with step: [10..1..-2] (10, 8, 6, 4, 2)
  stringstream ss("[10..1..-2]");
  auto parse_result = parser.parse_input(ss);
  
  ASSERT_TRUE(parse_result.node != nullptr);
  
  auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
  
  EXPECT_EQ(result->type, runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  ASSERT_EQ(seq_value->fields.size(), 5);
  
  vector<int> expected = {10, 8, 6, 4, 2};
  for (size_t i = 0; i < expected.size(); i++) {
    EXPECT_EQ(seq_value->fields[i]->type, runtime::Int);
    EXPECT_EQ(seq_value->fields[i]->get<int>(), expected[i]);
  }
}

TEST(RangeSequenceTest, EmptyRange) {
  Parser parser;
  Interpreter interp;
  
  // Test empty range (start > end with positive step)
  stringstream ss("[5..1..1]");
  auto parse_result = parser.parse_input(ss);
  
  ASSERT_TRUE(parse_result.node != nullptr);
  
  auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
  
  EXPECT_EQ(result->type, runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  EXPECT_EQ(seq_value->fields.size(), 0);
}

TEST(RangeSequenceTest, SingleElementRange) {
  Parser parser;
  Interpreter interp;
  
  // Test single element range: [5..5]
  stringstream ss("[5..5]");
  auto parse_result = parser.parse_input(ss);
  
  ASSERT_TRUE(parse_result.node != nullptr);
  
  auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
  
  EXPECT_EQ(result->type, runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  ASSERT_EQ(seq_value->fields.size(), 1);
  EXPECT_EQ(seq_value->fields[0]->type, runtime::Int);
  EXPECT_EQ(seq_value->fields[0]->get<int>(), 5);
}

TEST(RangeSequenceTest, FloatRange) {
  Parser parser;
  Interpreter interp;
  
  // Test float range: [1.5..4.5]
  stringstream ss("[1.5..4.5]");
  auto parse_result = parser.parse_input(ss);
  
  ASSERT_TRUE(parse_result.node != nullptr);
  
  auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
  
  EXPECT_EQ(result->type, runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  ASSERT_EQ(seq_value->fields.size(), 4); // 1.5, 2.5, 3.5, 4.5
  
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(seq_value->fields[i]->type, runtime::Float);
    EXPECT_DOUBLE_EQ(seq_value->fields[i]->get<double>(), 1.5 + i);
  }
}

TEST(RangeSequenceTest, FloatRangeWithStep) {
  Parser parser;
  Interpreter interp;
  
  // Test float range with step: [0.0..1.0..0.25]
  stringstream ss("[0.0..1.0..0.25]");
  auto parse_result = parser.parse_input(ss);
  
  ASSERT_TRUE(parse_result.node != nullptr);
  
  auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
  
  EXPECT_EQ(result->type, runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  ASSERT_EQ(seq_value->fields.size(), 5); // 0.0, 0.25, 0.5, 0.75, 1.0
  
  for (int i = 0; i < 5; i++) {
    EXPECT_EQ(seq_value->fields[i]->type, runtime::Float);
    EXPECT_DOUBLE_EQ(seq_value->fields[i]->get<double>(), i * 0.25);
  }
}