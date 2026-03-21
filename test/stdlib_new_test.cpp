#include <doctest/doctest.h>
#include <sstream>
#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;

using RT = yona::interp::runtime::RuntimeObjectType;

struct StdlibNewTest {
  parser::Parser parser;
  Interpreter interp;

  yona::interp::runtime::RuntimeObjectPtr eval(const string& code) {
    istringstream stream(code);
    auto result = parser.parse_input(stream);
    REQUIRE(result.node != nullptr);
    return interp.visit(result.node.get()).value;
  }
};

TEST_SUITE("Std.String") {

TEST_CASE("String length") {
  StdlibNewTest t;
  auto result = t.eval(R"(import length from Std\String in length "hello")");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 5);
}

TEST_CASE("String toUpperCase") {
  StdlibNewTest t;
  auto result = t.eval(R"(import toUpperCase from Std\String in toUpperCase "hello")");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "HELLO");
}

TEST_CASE("String toLowerCase") {
  StdlibNewTest t;
  auto result = t.eval(R"(import toLowerCase from Std\String in toLowerCase "HELLO")");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "hello");
}

TEST_CASE("String trim") {
  StdlibNewTest t;
  auto result = t.eval(R"(import trim from Std\String in trim "  hello  ")");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "hello");
}

TEST_CASE("String split") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import split from Std\String in
    import length from Std\List in
    length (split "," "a,b,c")
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3);
}

TEST_CASE("String join") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import join from Std\String in
    join ", " ["a", "b", "c"]
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "a, b, c");
}

TEST_CASE("String contains") {
  StdlibNewTest t;
  auto result = t.eval(R"(import contains from Std\String in contains "ell" "hello")");
  CHECK(result->type == RT::Bool);
  CHECK(result->get<bool>() == true);
}

TEST_CASE("String indexOf") {
  StdlibNewTest t;
  auto result = t.eval(R"(import indexOf from Std\String in indexOf "lo" "hello")");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3);
}

TEST_CASE("String replace") {
  StdlibNewTest t;
  auto result = t.eval(R"(import replace from Std\String in replace "world" "Yona" "hello world")");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "hello Yona");
}

TEST_CASE("String startsWith and endsWith") {
  StdlibNewTest t;
  auto r1 = t.eval(R"(import startsWith from Std\String in startsWith "hel" "hello")");
  CHECK(r1->get<bool>() == true);

  StdlibNewTest t2;
  auto r2 = t2.eval(R"(import endsWith from Std\String in endsWith "llo" "hello")");
  CHECK(r2->get<bool>() == true);
}

TEST_CASE("String toString") {
  StdlibNewTest t;
  auto result = t.eval(R"(import toString from Std\String in toString 42)");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "42");
}

TEST_CASE("String chars") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import chars from Std\String in
    import length from Std\List in
    length (chars "abc")
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3);
}

} // Std.String TEST_SUITE

TEST_SUITE("Std.Set") {

TEST_CASE("Set contains") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import contains from Std\Set in
    contains 2 {1, 2, 3}
  )");
  CHECK(result->type == RT::Bool);
  CHECK(result->get<bool>() == true);
}

TEST_CASE("Set add") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import add, size from Std\Set in
    size (add 4 {1, 2, 3})
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 4);
}

TEST_CASE("Set add duplicate") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import add, size from Std\Set in
    size (add 2 {1, 2, 3})
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3); // no change
}

TEST_CASE("Set fromList removes duplicates") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import fromList, size from Std\Set in
    size (fromList [1, 2, 2, 3, 3, 3])
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3);
}

} // Std.Set TEST_SUITE

TEST_SUITE("Std.Dict") {

TEST_CASE("Dict put and get") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import put, get from Std\Dict in
    import unwrapOr from Std\Option in
    let d = put "key" 42 (put "init" 0 {"x": 1}) in
    unwrapOr 0 (get "key" d)
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 42);
}

TEST_CASE("Dict keys and values") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import keys from Std\Dict in
    import length from Std\List in
    length (keys {"a": 1, "b": 2, "c": 3})
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3);
}

TEST_CASE("Dict containsKey") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import containsKey from Std\Dict in
    containsKey "b" {"a": 1, "b": 2}
  )");
  CHECK(result->type == RT::Bool);
  CHECK(result->get<bool>() == true);
}

TEST_CASE("Dict merge") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import merge, size from Std\Dict in
    size (merge {"a": 1} {"b": 2, "c": 3})
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3);
}

} // Std.Dict TEST_SUITE

TEST_SUITE("Std.Timer") {

TEST_CASE("Timer millis returns int") {
  StdlibNewTest t;
  auto result = t.eval(R"(import millis from Std\Timer in millis)");
  CHECK(result->type == RT::Int);
  // Just check it's a positive number
  CHECK(result->get<int>() > 0);
}

TEST_CASE("Timer measure") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import measure from Std\Timer in
    let (nanos, value) = measure (\-> 42) in
    value
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 42);
}

} // Std.Timer TEST_SUITE

TEST_SUITE("File IO") {

TEST_CASE("File write and read") {
  StdlibNewTest t;
  auto result = t.eval(R"(
    import writeFile, readFile, deleteFile from Std\IO in
    let _ = writeFile "test_async_io.txt" "async content" in
    let content = readFile "test_async_io.txt" in
    let _ = deleteFile "test_async_io.txt" in
    content
  )");
  // readFile returns (:ok, content) tuple
  CHECK(result->type == RT::Tuple);
  auto tuple = result->get<shared_ptr<yona::interp::runtime::TupleValue>>();
  CHECK(tuple->fields.size() == 2);
  CHECK(tuple->fields[1]->get<string>() == "async content");
}

} // Async IO TEST_SUITE
