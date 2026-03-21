#include <doctest/doctest.h>
#include <sstream>
#include <memory>
#include <iostream>
#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"
#include "common.h"

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;

// RAII guard to capture and restore cout
struct CoutCapture {
  stringstream captured;
  streambuf* orig;
  CoutCapture() : orig(cout.rdbuf(captured.rdbuf())) {}
  ~CoutCapture() { cout.rdbuf(orig); }
  string str() const { return captured.str(); }
};

TEST_SUITE("Native Modules") {

TEST_CASE("Import native IO module") {
  string code = R"(
    import println from Std\IO in
      println "Hello from native module!"
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  CoutCapture capture;
  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  CHECK(result.value->type == yona::interp::runtime::Unit);
  CHECK(capture.str() == "Hello from native module!\n");
}

TEST_CASE("Import multiple native functions") {
  string code = R"(
    import print, println from Std\IO in
      let _ = print "Hello" in
        println " World!"
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  CoutCapture capture;
  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  CHECK(result.value->type == yona::interp::runtime::Unit);
  CHECK(capture.str() == "Hello World!\n");
}

TEST_CASE("Import entire native module") {
  string code = R"(
    import Std\IO in
      let _ = print "Test: " in
        println 42
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  CoutCapture capture;
  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  CHECK(result.value->type == yona::interp::runtime::Unit);
  CHECK(capture.str() == "Test: 42\n");
}

TEST_CASE("Import native module with alias") {
  string code = R"(
    import Std\IO as IO in
      IO.println "Using module alias"
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  CoutCapture capture;
  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  CHECK(result.value->type == yona::interp::runtime::Unit);
  CHECK(capture.str() == "Using module alias\n");
}

TEST_CASE("Native function with different types") {
  string code = R"(
    import println from Std\IO in
      let _ = println 42 in
      let _ = println 3.14 in
      let _ = println true in
      let _ = println "string" in
        println [1, 2, 3]
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  CoutCapture capture;
  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  string expected = "42\n3.14\ntrue\nstring\n[1, 2, 3]\n";
  CHECK(result.value->type == yona::interp::runtime::Unit);
  CHECK(capture.str() == expected);
}

} // TEST_SUITE
