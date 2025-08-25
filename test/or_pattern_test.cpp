#include <doctest/doctest.h>
#include <sstream>
#include <memory>
#include "parser.h"
#include "Interpreter.h"
#include "runtime.h"
#include "common.h"

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;

TEST_SUITE("OrPatterns") {

TEST_CASE("OR pattern with literals") {
  string code = R"(
    let x = 1 in
    case x of
      0 | 1 | 2 -> "small"
      _ -> "large"
    end
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == RuntimeObjectType::String);
  CHECK(result.value->get<string>() == "small");
}

TEST_CASE("OR pattern with different value") {
  string code = R"(
    let x = 5 in
    case x of
      0 | 1 | 2 -> "small"
      _ -> "large"
    end
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == RuntimeObjectType::String);
  CHECK(result.value->get<string>() == "large");
}

TEST_CASE("OR pattern with symbols") {
  string code = R"(
    let status = :ok in
    case status of
      :ok | :success -> "good"
      :error | :failure -> "bad"
      _ -> "unknown"
    end
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == RuntimeObjectType::String);
  CHECK(result.value->get<string>() == "good");
}

TEST_CASE("OR pattern with mixed types") {
  string code = R"(
    let value = "hello" in
    case value of
      "hi" | "hello" | "hey" -> "greeting"
      _ -> "not a greeting"
    end
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == RuntimeObjectType::String);
  CHECK(result.value->get<string>() == "greeting");
}

} // TEST_SUITE
