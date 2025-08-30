#include <doctest/doctest.h>
#include <sstream>
#include <memory>
#include <filesystem>
#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"
#include "common.h"

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;

TEST_SUITE("Extended Standard Library") {

TEST_CASE("Option module") {
  string code = R"(
    import some, none, isSome, isNone, unwrapOr, map from Std\Option in
      let opt1 = some 42 in
      let opt2 = none in
      let result1 = isSome opt1 in
      let result2 = isNone opt2 in
      let result3 = unwrapOr 0 opt2 in
      let result4 = unwrapOr 0 opt1 in
      let result5 = map (\x -> x * 2) opt1 in
        (result1, result2, result3, result4, result5)
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Tuple);
  auto tuple = result.value->get<shared_ptr<TupleValue>>();
  REQUIRE(tuple->fields.size() == 5);

  CHECK(tuple->fields[0]->get<bool>() == true);  // isSome opt1
  CHECK(tuple->fields[1]->get<bool>() == true);  // isNone opt2
  CHECK(tuple->fields[2]->get<int>() == 0);      // unwrapOr 0 opt2
  CHECK(tuple->fields[3]->get<int>() == 42);     // unwrapOr 0 opt1

  // result5 is some 84
  REQUIRE(tuple->fields[4]->type == Tuple);
  auto some_result = tuple->fields[4]->get<shared_ptr<TupleValue>>();
  REQUIRE(some_result->fields.size() == 2);
  CHECK(some_result->fields[0]->type == yona::interp::runtime::Symbol);
  CHECK(some_result->fields[1]->get<int>() == 84);
}

TEST_CASE("Result module") {
  string code = R"(
    import ok, err, isOk, isErr, unwrapOr, map, mapErr from Std\Result in
      let res1 = ok 42 in
      let res2 = err "error message" in
      let check1 = isOk res1 in
      let check2 = isErr res2 in
      let val1 = unwrapOr 0 res1 in
      let val2 = unwrapOr 0 res2 in
      let mapped = map (\x -> x * 2) res1 in
      let mappedErr = mapErr (\e -> "Error: " ++ e) res2 in
        (check1, check2, val1, val2, mapped, mappedErr)
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Tuple);
  auto tuple = result.value->get<shared_ptr<TupleValue>>();
  REQUIRE(tuple->fields.size() == 6);

  CHECK(tuple->fields[0]->get<bool>() == true);   // isOk res1
  CHECK(tuple->fields[1]->get<bool>() == true);   // isErr res2
  CHECK(tuple->fields[2]->get<int>() == 42);      // unwrapOr 0 res1
  CHECK(tuple->fields[3]->get<int>() == 0);       // unwrapOr 0 res2
}

TEST_CASE("Tuple module") {
  string code = R"(
    import fst, snd, swap, mapBoth, zip, unzip from Std\Tuple in
      let t1 = (10, 20) in
      let t2 = swap t1 in
      let t3 = mapBoth (\x -> x + 1) (\x -> x * 2) t1 in
      let list1 = [1, 2, 3] in
      let list2 = ["a", "b", "c"] in
      let zipped = zip list1 list2 in
      let (nums, strs) = unzip zipped in
        (fst t1, snd t1, t2, t3, zipped, nums, strs)
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Tuple);
  auto tuple = result.value->get<shared_ptr<TupleValue>>();
  REQUIRE(tuple->fields.size() == 7);

  CHECK(tuple->fields[0]->get<int>() == 10);      // fst t1
  CHECK(tuple->fields[1]->get<int>() == 20);      // snd t1

  // t2 = swap t1 = (20, 10)
  REQUIRE(tuple->fields[2]->type == Tuple);
  auto swapped = tuple->fields[2]->get<shared_ptr<TupleValue>>();
  CHECK(swapped->fields[0]->get<int>() == 20);
  CHECK(swapped->fields[1]->get<int>() == 10);

  // t3 = mapBoth (+1) (*2) t1 = (11, 40)
  REQUIRE(tuple->fields[3]->type == Tuple);
  auto mapped = tuple->fields[3]->get<shared_ptr<TupleValue>>();
  CHECK(mapped->fields[0]->get<int>() == 11);
  CHECK(mapped->fields[1]->get<int>() == 40);
}

TEST_CASE("Range module") {
  string code = R"(
    import range, toList, contains, length, take, drop from Std\Range in
      let r = range 1 10 in
      let list = toList (take 5 r) in
      let check1 = contains 5 r in
      let check2 = contains 15 r in
      let len = length r in
      let dropped = toList (drop 7 r) in
        (list, check1, check2, len, dropped)
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Tuple);
  auto tuple = result.value->get<shared_ptr<TupleValue>>();
  REQUIRE(tuple->fields.size() == 5);

  // list = [1, 2, 3, 4, 5]
  REQUIRE(tuple->fields[0]->type == yona::interp::runtime::Seq);
  auto list = tuple->fields[0]->get<shared_ptr<SeqValue>>();
  REQUIRE(list->fields.size() == 5);
  for (int i = 0; i < 5; i++) {
    CHECK(list->fields[i]->get<int>() == i + 1);
  }

  CHECK(tuple->fields[1]->get<bool>() == true);   // contains 5 r
  CHECK(tuple->fields[2]->get<bool>() == false);  // contains 15 r
  CHECK(tuple->fields[3]->get<int>() == 10);      // length r

  // dropped = [8, 9, 10]
  REQUIRE(tuple->fields[4]->type == yona::interp::runtime::Seq);
  auto dropped = tuple->fields[4]->get<shared_ptr<SeqValue>>();
  REQUIRE(dropped->fields.size() == 3);
  CHECK(dropped->fields[0]->get<int>() == 8);
  CHECK(dropped->fields[1]->get<int>() == 9);
  CHECK(dropped->fields[2]->get<int>() == 10);
}

TEST_CASE("File I/O operations") {
  string code = R"(
    import writeFile, readFile, fileExists from Std\IO in
      let filename = "test_file.txt" in
      let content = "Hello, Yona!" in
      let write_result = writeFile filename content in
      let exists = fileExists filename in
      let read_result = readFile filename in
        (write_result, exists, read_result)
  )";

  parser::Parser parser;
  istringstream stream(code);
  auto parse_result = parser.parse_input(stream);
  REQUIRE(parse_result.node != nullptr);

  Interpreter interpreter;
  auto result = interpreter.visit(parse_result.node.get());

  REQUIRE(result.value->type == Tuple);
  auto tuple = result.value->get<shared_ptr<TupleValue>>();
  REQUIRE(tuple->fields.size() == 3);

  // Check write result
  REQUIRE(tuple->fields[0]->type == Tuple);
  auto write_res = tuple->fields[0]->get<shared_ptr<TupleValue>>();
  CHECK(write_res->fields[0]->type == yona::interp::runtime::Symbol);

  // Check file exists
  CHECK(tuple->fields[1]->get<bool>() == true);

  // Check read result
  REQUIRE(tuple->fields[2]->type == Tuple);
  auto read_res = tuple->fields[2]->get<shared_ptr<TupleValue>>();
  CHECK(read_res->fields[0]->type == yona::interp::runtime::Symbol);
  if (read_res->fields.size() > 1 && read_res->fields[1]->type == yona::interp::runtime::String) {
    CHECK(read_res->fields[1]->get<string>() == "Hello, Yona!");
  }

  // Clean up
  std::filesystem::remove("test_file.txt");
}

} // TEST_SUITE
