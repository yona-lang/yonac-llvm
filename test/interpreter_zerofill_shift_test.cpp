#include <doctest/doctest.h>
#include <memory>
#include <sstream>
#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"
#include "common.h"

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;

// Test zero-fill right shift operator
TEST_CASE("Zero-fill right shift basic test") {
  string code = "-8 >>> 2";  // -8 as unsigned is 0xFFFFFFF8, >>> 2 should give 0x3FFFFFFE

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  auto ast = parse_result.node;
  REQUIRE(ast != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(ast.get());

  REQUIRE(result.value->type == Int);
  // -8 as unsigned 32-bit is 0xFFFFFFF8
  // >>> 2 gives 0x3FFFFFFE which is 1073741822 as signed int
  CHECK(result.value->get<int>() == 1073741822);
}

TEST_CASE("Zero-fill right shift with positive numbers") {
  string code = "16 >>> 2";  // Should behave like regular right shift for positive numbers

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  auto ast = parse_result.node;
  REQUIRE(ast != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(ast.get());

  REQUIRE(result.value->type == Int);
  CHECK(result.value->get<int>() == 4);  // 16 >>> 2 = 4
}

TEST_CASE("Zero-fill right shift with bytes") {
  string code = "255b >>> 4";  // 0xFF >>> 4 = 0x0F = 15

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  auto ast = parse_result.node;
  REQUIRE(ast != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(ast.get());

  REQUIRE(result.value->type == yona::interp::runtime::Byte);
  CHECK(static_cast<uint8_t>(result.value->get<std::byte>()) == 15);
}

TEST_CASE("Compare zero-fill shift with regular shift") {
  // For negative numbers, >>> and >> should give different results
  string code1 = "-16 >> 2";   // Sign-extending right shift
  string code2 = "-16 >>> 2";  // Zero-fill right shift

  parser::Parser parser1;
  istringstream stream1(code1);
  auto parse_result1 = parser1.parse_input(stream1);
  auto ast1 = parse_result1.node;
  REQUIRE(ast1 != nullptr);

  parser::Parser parser2;
  istringstream stream2(code2);
  auto parse_result2 = parser2.parse_input(stream2);
  auto ast2 = parse_result2.node;
  REQUIRE(ast2 != nullptr);

  Interpreter interpreter;
  auto result1 = interpreter.visit(ast1.get());
  auto result2 = interpreter.visit(ast2.get());

  REQUIRE(result1.value->type == Int);
  REQUIRE(result2.value->type == Int);

  // Regular right shift on -16 gives -4 (sign extension)
  CHECK(result1.value->get<int>() == -4);

  // Zero-fill right shift on -16 gives a large positive number
  // -16 as unsigned 32-bit is 0xFFFFFFF0
  // >>> 2 gives 0x3FFFFFFC which is 1073741820 as signed int
  CHECK(result2.value->get<int>() == 1073741820);
}

TEST_CASE("Zero-fill shift with mixed types") {
  // Test int >>> byte
  string code1 = "32 >>> 3b";

  parser::Parser parser1;
  istringstream stream1(code1);
  auto parse_result1 = parser1.parse_input(stream1);
  auto ast1 = parse_result1.node;
  REQUIRE(ast1 != nullptr);

  Interpreter interpreter;
  auto result1 = interpreter.visit(ast1.get());

  REQUIRE(result1.value->type == Int);
  CHECK(result1.value->get<int>() == 4);  // 32 >>> 3 = 4

  // Test byte >>> byte
  string code2 = "128b >>> 2b";

  parser::Parser parser2;
  istringstream stream2(code2);
  auto parse_result2 = parser2.parse_input(stream2);
  auto ast2 = parse_result2.node;
  REQUIRE(ast2 != nullptr);

  auto result2 = interpreter.visit(ast2.get());

  REQUIRE(result2.value->type == yona::interp::runtime::Byte);
  CHECK(static_cast<uint8_t>(result2.value->get<std::byte>()) == 32);  // 128 >>> 2 = 32
}

TEST_CASE("Zero-fill shift type error") {
  string code = "3.14 >>> 2";  // Float is not supported

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  auto ast = parse_result.node;
  REQUIRE(ast != nullptr);

  Interpreter interpreter;

  CHECK_THROWS_AS(
    interpreter.visit(ast.get()), ::yona::yona_error);
}
