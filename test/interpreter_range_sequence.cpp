#include <sstream>
#include <doctest/doctest.h>
#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"
#include "ast_visitor_impl.h"

using namespace std;
using namespace yona::parser;
using namespace yona::interp;

TEST_CASE("SimpleRange") {
  Parser parser;
  Interpreter interp;

  // Test simple range: [1..5]
  stringstream ss("[1..5]");
  auto parse_result = parser.parse_input(ss);

  REQUIRE(parse_result.node != nullptr);

  auto interpreter_result = interp.visit(parse_result.node.get());
      auto result = interpreter_result.value;

  CHECK(result->type == runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  REQUIRE(seq_value->fields.size() == 5);

  for (int i = 0; i < 5; i++) {
    CHECK(seq_value->fields[i]->type == runtime::Int);
    CHECK(seq_value->fields[i]->get<int>() == i + 1);
  }
}

TEST_CASE("RangeWithStep") {
  Parser parser;
  Interpreter interp;

  // Test range with step: [1..10..2] (1, 3, 5, 7, 9)
  stringstream ss("[1..10..2]");
  auto parse_result = parser.parse_input(ss);

  REQUIRE(parse_result.node != nullptr);

  auto interpreter_result = interp.visit(parse_result.node.get());
      auto result = interpreter_result.value;

  CHECK(result->type == runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  REQUIRE(seq_value->fields.size() == 5);

  vector<int> expected = {1, 3, 5, 7, 9};
  for (size_t i = 0; i < expected.size(); i++) {
    CHECK(seq_value->fields[i]->type == runtime::Int);
    CHECK(seq_value->fields[i]->get<int>() == expected[i]);
  }
}

TEST_CASE("ReverseRange") {
  Parser parser;
  Interpreter interp;

  // Test reverse range: [5..1]
  stringstream ss("[5..1]");
  auto parse_result = parser.parse_input(ss);

  REQUIRE(parse_result.node != nullptr);

  auto interpreter_result = interp.visit(parse_result.node.get());
      auto result = interpreter_result.value;

  CHECK(result->type == runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  REQUIRE(seq_value->fields.size() == 5);

  for (int i = 0; i < 5; i++) {
    CHECK(seq_value->fields[i]->type == runtime::Int);
    CHECK(seq_value->fields[i]->get<int>() == 5 - i);
  }
}

TEST_CASE("ReverseRangeWithNegativeStep") {
  Parser parser;
  Interpreter interp;

  // Test reverse range with step: [10..1..-2] (10, 8, 6, 4, 2)
  stringstream ss("[10..1..-2]");
  auto parse_result = parser.parse_input(ss);

  REQUIRE(parse_result.node != nullptr);

  auto interpreter_result = interp.visit(parse_result.node.get());
      auto result = interpreter_result.value;

  CHECK(result->type == runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  REQUIRE(seq_value->fields.size() == 5);

  vector<int> expected = {10, 8, 6, 4, 2};
  for (size_t i = 0; i < expected.size(); i++) {
    CHECK(seq_value->fields[i]->type == runtime::Int);
    CHECK(seq_value->fields[i]->get<int>() == expected[i]);
  }
}

TEST_CASE("EmptyRange") {
  Parser parser;
  Interpreter interp;

  // Test empty range (start > end with positive step)
  stringstream ss("[5..1..1]");
  auto parse_result = parser.parse_input(ss);

  REQUIRE(parse_result.node != nullptr);

  auto interpreter_result = interp.visit(parse_result.node.get());
      auto result = interpreter_result.value;

  CHECK(result->type == runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  CHECK(seq_value->fields.size() == 0);
}

TEST_CASE("SingleElementRange") {
  Parser parser;
  Interpreter interp;

  // Test single element range: [5..5]
  stringstream ss("[5..5]");
  auto parse_result = parser.parse_input(ss);

  REQUIRE(parse_result.node != nullptr);

  auto interpreter_result = interp.visit(parse_result.node.get());
      auto result = interpreter_result.value;

  CHECK(result->type == runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  REQUIRE(seq_value->fields.size() == 1);
  CHECK(seq_value->fields[0]->type == runtime::Int);
  CHECK(seq_value->fields[0]->get<int>() == 5);
}

TEST_CASE("FloatRange") {
  Parser parser;
  Interpreter interp;

  // Test float range: [1.5..4.5]
  stringstream ss("[1.5..4.5]");
  auto parse_result = parser.parse_input(ss);

  REQUIRE(parse_result.node != nullptr);

  auto interpreter_result = interp.visit(parse_result.node.get());
      auto result = interpreter_result.value;

  CHECK(result->type == runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  REQUIRE(seq_value->fields.size() == 4); // 1.5, 2.5, 3.5, 4.5

  for (int i = 0; i < 4; i++) {
    CHECK(seq_value->fields[i]->type == runtime::Float);
    CHECK(seq_value->fields[i]->get<double>() == doctest::Approx(1.5 + i));
  }
}

TEST_CASE("FloatRangeWithStep") {
  Parser parser;
  Interpreter interp;

  // Test float range with step: [0.0..1.0..0.25]
  stringstream ss("[0.0..1.0..0.25]");
  auto parse_result = parser.parse_input(ss);

  REQUIRE(parse_result.node != nullptr);

  auto interpreter_result = interp.visit(parse_result.node.get());
      auto result = interpreter_result.value;

  CHECK(result->type == runtime::Seq);
  auto seq_value = result->get<shared_ptr<SeqValue>>();
  REQUIRE(seq_value->fields.size() == 5); // 0.0, 0.25, 0.5, 0.75, 1.0

  for (int i = 0; i < 5; i++) {
    CHECK(seq_value->fields[i]->type == runtime::Float);
    CHECK(seq_value->fields[i]->get<double>() == doctest::Approx(i * 0.25));
  }
}
