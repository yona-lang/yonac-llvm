#include <doctest/doctest.h>
#include <sstream>
#include <memory>
#include <iostream>
#include "parser.h"
#include "Interpreter.h"
#include "runtime.h"
#include "common.h"

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;

TEST_CASE("Module system integration test") {
  // Test that the fixed ImportExpr allows imported functions to be used
  string code = R"(
    import add, multiply from Test\Test in
      let x = add 10 20 in
      let y = multiply x 2 in
        add x y
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Int);
  // x = 30, y = 60, result = 90
  CHECK(result.value->get<int>() == 90);
}

TEST_CASE("Native and Yona modules together") {
  string code = R"(
    import println from Std\IO in
    import map from Std\List in
    import add from Test\Test in
      let numbers = [1, 2, 3, 4, 5] in
      let incremented = map (\x -> add x 1) numbers in
        println incremented
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  // Capture stdout
  stringstream captured;
  streambuf* orig = cout.rdbuf(captured.rdbuf());

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  // Restore stdout
  cout.rdbuf(orig);

  CHECK(result.value->type == yona::interp::runtime::Unit);
  CHECK(captured.str() == "[2, 3, 4, 5, 6]\n");
}

TEST_CASE("Nested imports") {
  string code = R"(
    import add from Test\Test in
      let f x =
        import multiply from Test\Test in
          multiply (add x 1) 2
      in
        f 10
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Int);
  // (10 + 1) * 2 = 22
  CHECK(result.value->get<int>() == 22);
}

TEST_CASE("Module function in higher-order context") {
  string code = R"(
    import add from Test\Test in
    import map from Std\List in
      let add5 = add 5 in
        map add5 [1, 2, 3, 4]
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == yona::interp::runtime::Seq);
  auto seq = result.value->get<shared_ptr<SeqValue>>();
  REQUIRE(seq->fields.size() == 4);
  CHECK(seq->fields[0]->get<int>() == 6);
  CHECK(seq->fields[1]->get<int>() == 7);
  CHECK(seq->fields[2]->get<int>() == 8);
  CHECK(seq->fields[3]->get<int>() == 9);
}
