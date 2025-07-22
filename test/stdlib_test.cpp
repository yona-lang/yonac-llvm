#include <doctest/doctest.h>
#include <sstream>
#include <memory>
#include <iostream>
#include "parser.h"
#include "Interpreter.h"
#include "runtime.h"
#include "common.h"
#include <cstdlib>

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;

TEST_SUITE("Standard Library") {

struct StdlibTest {
  void SetUp() {
    // Set YONA_PATH to include stdlib directory
    const char* existing_path = getenv("YONA_PATH");
    if (!existing_path) {
      // Add stdlib to YONA_PATH
      #ifdef _WIN32
        _putenv_s("YONA_PATH", "../../stdlib;./stdlib");
      #else
        setenv("YONA_PATH", "../../stdlib:./stdlib", 1);
      #endif
    }
  }
};

TEST_CASE("List map function") {
  StdlibTest fixture;
  fixture.SetUp();

  string code = R"(
    import map from Std\List in
      map (\x -> x * 2) [1, 2, 3, 4, 5]
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == yona::interp::runtime::Seq);
  auto seq = result.value->get<shared_ptr<SeqValue>>();
  REQUIRE(seq->fields.size() == 5);
  CHECK(seq->fields[0]->get<int>() == 2);
  CHECK(seq->fields[1]->get<int>() == 4);
  CHECK(seq->fields[2]->get<int>() == 6);
  CHECK(seq->fields[3]->get<int>() == 8);
  CHECK(seq->fields[4]->get<int>() == 10);
}

TEST_CASE("List filter function") {
  StdlibTest fixture;
  fixture.SetUp();

  string code = R"(
    import filter from Std\List in
      filter (\x -> x % 2 == 0) [1, 2, 3, 4, 5, 6]
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == yona::interp::runtime::Seq);
  auto seq = result.value->get<shared_ptr<SeqValue>>();
  REQUIRE(seq->fields.size() == 3);
  CHECK(seq->fields[0]->get<int>() == 2);
  CHECK(seq->fields[1]->get<int>() == 4);
  CHECK(seq->fields[2]->get<int>() == 6);
}

TEST_CASE("List fold function") {
  StdlibTest fixture;
  fixture.SetUp();

  string code = R"(
    import fold from Std\List in
      fold (\acc x -> acc + x) 0 [1, 2, 3, 4, 5]
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Int);
  CHECK(result.value->get<int>() == 15);
}

TEST_CASE("Combine native IO with Yona List module") {
  StdlibTest fixture;
  fixture.SetUp();

  string code = R"(
    import println from Std\IO in
    import map from Std\List in
      let doubled = map (\x -> x * 2) [1, 2, 3] in
        println doubled
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
  CHECK(captured.str() == "[2, 4, 6]\n");
}

TEST_CASE("Math module functions") {
  StdlibTest fixture;
  fixture.SetUp();

  string code = R"(
    import abs, max, min, factorial from Std\Math in
      let a = abs (-42) in
      let b = max 10 20 in
      let c = min 10 20 in
      let d = factorial 5 in
        (a, b, c, d)
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Tuple);
  auto tuple = result.value->get<shared_ptr<TupleValue>>();
  REQUIRE(tuple->fields.size() == 4);
  CHECK(tuple->fields[0]->get<int>() == 42);
  CHECK(tuple->fields[1]->get<int>() == 20);
  CHECK(tuple->fields[2]->get<int>() == 10);
  CHECK(tuple->fields[3]->get<int>() == 120);
}

} // TEST_SUITE
