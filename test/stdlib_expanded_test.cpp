#include <doctest/doctest.h>
#include <sstream>
#include <filesystem>
#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"

using namespace std;
using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using RT = yona::interp::runtime::RuntimeObjectType;

struct EvalFixture {
  parser::Parser parser;
  Interpreter interp;

  yona::interp::runtime::RuntimeObjectPtr eval(const string& code) {
    istringstream stream(code);
    auto result = parser.parse_input(stream);
    REQUIRE(result.node != nullptr);
    return interp.visit(result.node.get()).value;
  }
};

// ===== JSON Module =====

TEST_SUITE("Std.Json") {

TEST_CASE("Parse JSON array and access") {
  EvalFixture t;
  auto result = t.eval(R"(
    import parse from Std\Json in
    import lookup from Std\List in
    let arr = parse "[10, 20, 30]" in
    lookup 1 arr
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 20);
}

TEST_CASE("Parse JSON array") {
  EvalFixture t;
  auto result = t.eval(R"(
    import parse from Std\Json in
    import length from Std\List in
    length (parse "[1, 2, 3]")
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3);
}

TEST_CASE("Parse JSON primitives") {
  EvalFixture t;
  CHECK(t.eval(R"(import parse from Std\Json in parse "42")")->get<int>() == 42);
  CHECK(t.eval(R"(import parse from Std\Json in parse "true")")->get<bool>() == true);
  CHECK(t.eval(R"(import parse from Std\Json in parse "\"hello\"")")->get<string>() == "hello");
}

TEST_CASE("Stringify value") {
  EvalFixture t;
  auto result = t.eval(R"(import stringify from Std\Json in stringify 42)");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "42");
}

TEST_CASE("Stringify string") {
  EvalFixture t;
  auto result = t.eval(R"(import stringify from Std\Json in stringify "hello")");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "\"hello\"");
}

} // Std.Json

// ===== Regexp Module =====

TEST_SUITE("Std.Regexp") {

TEST_CASE("Regexp test") {
  EvalFixture t;
  auto result = t.eval(R"(import test from Std\Regexp in test "[0-9]+" "abc123def")");
  CHECK(result->type == RT::Bool);
  CHECK(result->get<bool>() == true);
}

TEST_CASE("Regexp test no match") {
  EvalFixture t;
  auto result = t.eval(R"(import test from Std\Regexp in test "[0-9]+" "abcdef")");
  CHECK(result->get<bool>() == false);
}

TEST_CASE("Regexp match") {
  EvalFixture t;
  auto result = t.eval(R"(
    import match from Std\Regexp in
    import isSome from Std\Option in
    isSome (match "[0-9]+" "abc123def")
  )");
  CHECK(result->get<bool>() == true);
}

TEST_CASE("Regexp matchAll") {
  EvalFixture t;
  auto result = t.eval(R"(
    import matchAll from Std\Regexp in
    import length from Std\List in
    length (matchAll "[0-9]+" "a1b2c3")
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 3);
}

TEST_CASE("Regexp replace") {
  EvalFixture t;
  auto result = t.eval(R"(import replace from Std\Regexp in replace "[0-9]+" "X" "a1b2c3")");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "aXbXcX");
}

TEST_CASE("Regexp split") {
  EvalFixture t;
  auto result = t.eval(R"(
    import split from Std\Regexp in
    import length from Std\List in
    length (split "[,;]" "a,b;c,d")
  )");
  CHECK(result->get<int>() == 4);
}

} // Std.Regexp

// ===== File Module =====

TEST_SUITE("Std.File") {

TEST_CASE("File exists") {
  EvalFixture t;
  auto result = t.eval(R"(import exists from Std\File in exists ".")");
  CHECK(result->type == RT::Bool);
  CHECK(result->get<bool>() == true);
}

TEST_CASE("File isDir") {
  EvalFixture t;
  auto result = t.eval(R"(import isDir from Std\File in isDir ".")");
  CHECK(result->get<bool>() == true);
}

TEST_CASE("File basename and dirname") {
  EvalFixture t;
  auto r1 = t.eval(R"(import basename from Std\File in basename "/foo/bar/baz.txt")");
  CHECK(r1->get<string>() == "baz.txt");

  EvalFixture t2;
  auto r2 = t2.eval(R"(import dirname from Std\File in dirname "/foo/bar/baz.txt")");
  CHECK(r2->get<string>() == "/foo/bar");
}

TEST_CASE("File extension") {
  EvalFixture t;
  auto result = t.eval(R"(import extension from Std\File in extension "file.txt")");
  CHECK(result->get<string>() == ".txt");
}

TEST_CASE("File join") {
  EvalFixture t;
  auto result = t.eval(R"(import join from Std\File in join "/foo" "bar.txt")");
  CHECK(result->type == RT::String);
  // Result should contain both parts joined
  CHECK(result->get<string>().find("bar.txt") != string::npos);
}

} // Std.File

// ===== Random Module =====

TEST_SUITE("Std.Random") {

TEST_CASE("Random int in range") {
  EvalFixture t;
  auto result = t.eval(R"(import int from Std\Random in int 1 10)");
  CHECK(result->type == RT::Int);
  int val = result->get<int>();
  CHECK(val >= 1);
  CHECK(val <= 10);
}

TEST_CASE("Random float in [0,1)") {
  EvalFixture t;
  auto result = t.eval(R"(import float from Std\Random in float)");
  CHECK(result->type == RT::Float);
  double val = result->get<double>();
  CHECK(val >= 0.0);
  CHECK(val < 1.0);
}

TEST_CASE("Random choice") {
  EvalFixture t;
  auto result = t.eval(R"(import choice from Std\Random in choice [10, 20, 30])");
  CHECK(result->type == RT::Int);
  int val = result->get<int>();
  CHECK((val == 10 || val == 20 || val == 30));
}

TEST_CASE("Random shuffle preserves length") {
  EvalFixture t;
  auto result = t.eval(R"(
    import shuffle from Std\Random in
    import length from Std\List in
    length (shuffle [1, 2, 3, 4, 5])
  )");
  CHECK(result->get<int>() == 5);
}

} // Std.Random

// ===== Time Module =====

TEST_SUITE("Std.Time") {

TEST_CASE("Time now returns positive int") {
  EvalFixture t;
  auto result = t.eval(R"(import now from Std\Time in now)");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() > 0);
}

TEST_CASE("Time nowMillis") {
  EvalFixture t;
  auto result = t.eval(R"(import nowMillis from Std\Time in nowMillis)");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() > 0);
}

} // Std.Time

// ===== Types Module =====

TEST_SUITE("Std.Types") {

TEST_CASE("typeOf returns correct symbols") {
  EvalFixture t;
  auto r1 = t.eval(R"(import typeOf from Std\Types in typeOf 42)");
  CHECK(r1->type == RT::Symbol);
  CHECK(r1->get<shared_ptr<yona::interp::runtime::SymbolValue>>()->name == "int");

  EvalFixture t2;
  auto r2 = t2.eval(R"(import typeOf from Std\Types in typeOf "hello")");
  CHECK(r2->get<shared_ptr<yona::interp::runtime::SymbolValue>>()->name == "string");

  EvalFixture t3;
  auto r3 = t3.eval(R"(import typeOf from Std\Types in typeOf true)");
  CHECK(r3->get<shared_ptr<yona::interp::runtime::SymbolValue>>()->name == "bool");
}

TEST_CASE("Type check functions") {
  EvalFixture t;
  CHECK(t.eval(R"(import isInt from Std\Types in isInt 42)")->get<bool>() == true);

  EvalFixture t2;
  CHECK(t2.eval(R"(import isString from Std\Types in isString "hi")")->get<bool>() == true);

  EvalFixture t3;
  CHECK(t3.eval(R"(import isInt from Std\Types in isInt "hi")")->get<bool>() == false);
}

TEST_CASE("Type conversion toInt") {
  EvalFixture t;
  CHECK(t.eval(R"(import toInt from Std\Types in toInt "42")")->get<int>() == 42);

  EvalFixture t2;
  CHECK(t2.eval(R"(import toInt from Std\Types in toInt 3.7)")->get<int>() == 3);
}

TEST_CASE("Type conversion toFloat") {
  EvalFixture t;
  auto result = t.eval(R"(import toFloat from Std\Types in toFloat 42)");
  CHECK(result->type == RT::Float);
  CHECK(result->get<double>() == doctest::Approx(42.0));
}

TEST_CASE("Type conversion toStr") {
  EvalFixture t;
  CHECK(t.eval(R"(import toStr from Std\Types in toStr 42)")->get<string>() == "42");
}

} // Std.Types

// ===== Extended List functions =====

TEST_SUITE("Std.List extended") {

TEST_CASE("List take") {
  EvalFixture t;
  auto result = t.eval(R"(
    import take, length from Std\List in
    length (take 3 [1, 2, 3, 4, 5])
  )");
  CHECK(result->get<int>() == 3);
}

TEST_CASE("List drop") {
  EvalFixture t;
  auto result = t.eval(R"(
    import drop, length from Std\List in
    length (drop 2 [1, 2, 3, 4, 5])
  )");
  CHECK(result->get<int>() == 3);
}

TEST_CASE("List flatten") {
  EvalFixture t;
  auto result = t.eval(R"(
    import flatten, length from Std\List in
    length (flatten [[1, 2], [3, 4], [5]])
  )");
  CHECK(result->get<int>() == 5);
}

TEST_CASE("List splitAt") {
  EvalFixture t;
  auto result = t.eval(R"(
    import splitAt from Std\List in
    import length from Std\List in
    let (left, right) = splitAt 2 [1, 2, 3, 4, 5] in
    (length left, length right)
  )");
  CHECK(result->type == RT::Tuple);
  auto tuple = result->get<shared_ptr<yona::interp::runtime::TupleValue>>();
  CHECK(tuple->fields[0]->get<int>() == 2);
  CHECK(tuple->fields[1]->get<int>() == 3);
}

TEST_CASE("List any and all") {
  EvalFixture t;
  CHECK(t.eval(R"(import any from Std\List in any (\x -> x > 3) [1, 2, 3, 4, 5])")->get<bool>() == true);

  EvalFixture t2;
  CHECK(t2.eval(R"(import all from Std\List in all (\x -> x > 0) [1, 2, 3])")->get<bool>() == true);

  EvalFixture t3;
  CHECK(t3.eval(R"(import all from Std\List in all (\x -> x > 2) [1, 2, 3])")->get<bool>() == false);
}

TEST_CASE("List contains") {
  EvalFixture t;
  CHECK(t.eval(R"(import contains from Std\List in contains 3 [1, 2, 3, 4])")->get<bool>() == true);

  EvalFixture t2;
  CHECK(t2.eval(R"(import contains from Std\List in contains 9 [1, 2, 3])")->get<bool>() == false);
}

TEST_CASE("List isEmpty") {
  EvalFixture t;
  CHECK(t.eval(R"(import isEmpty from Std\List in isEmpty [])")->get<bool>() == true);

  EvalFixture t2;
  CHECK(t2.eval(R"(import isEmpty from Std\List in isEmpty [1])")->get<bool>() == false);
}

TEST_CASE("List foldl and foldr") {
  EvalFixture t;
  // foldl with string concat: "abc"
  auto r1 = t.eval(R"(import foldl from Std\List in foldl (\acc x -> acc ++ x) "" ["a", "b", "c"])");
  CHECK(r1->get<string>() == "abc");

  EvalFixture t2;
  // foldr with string concat: "abc"
  auto r2 = t2.eval(R"(import foldr from Std\List in foldr (\x acc -> x ++ acc) "" ["a", "b", "c"])");
  CHECK(r2->get<string>() == "abc");
}

TEST_CASE("List lookup") {
  EvalFixture t;
  CHECK(t.eval(R"(import lookup from Std\List in lookup 2 [10, 20, 30, 40])")->get<int>() == 30);
}

} // Std.List extended

// ===== Extended Set functions =====

TEST_SUITE("Std.Set extended") {

TEST_CASE("Set fold") {
  EvalFixture t;
  auto result = t.eval(R"(
    import fold from Std\Set in
    fold (\acc x -> acc + x) 0 {1, 2, 3}
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

TEST_CASE("Set map") {
  EvalFixture t;
  auto result = t.eval(R"(
    import map, size from Std\Set in
    size (map (\x -> x * 2) {1, 2, 3})
  )");
  CHECK(result->get<int>() == 3);
}

TEST_CASE("Set filter") {
  EvalFixture t;
  auto result = t.eval(R"(
    import filter, size from Std\Set in
    size (filter (\x -> x > 2) {1, 2, 3, 4, 5})
  )");
  CHECK(result->get<int>() == 3);
}

TEST_CASE("Set isEmpty") {
  EvalFixture t;
  CHECK(t.eval(R"(import isEmpty from Std\Set in isEmpty {})")->get<bool>() == true);
}

} // Std.Set extended

// ===== Extended Dict functions =====

TEST_SUITE("Std.Dict extended") {

TEST_CASE("Dict fold") {
  EvalFixture t;
  auto result = t.eval(R"(
    import fold from Std\Dict in
    fold (\acc kv -> acc + 1) 0 {"a": 1, "b": 2, "c": 3}
  )");
  CHECK(result->get<int>() == 3);
}

TEST_CASE("Dict isEmpty") {
  EvalFixture t;
  // Use a dict with content (no empty dict literal)
  auto result = t.eval(R"(
    import isEmpty, remove from Std\Dict in
    isEmpty (remove "a" {"a": 1})
  )");
  CHECK(result->get<bool>() == true);
}

TEST_CASE("Dict lookup returns unit on miss") {
  EvalFixture t;
  auto result = t.eval(R"(
    import lookup from Std\Dict in
    import typeOf from Std\Types in
    typeOf (lookup "missing" {"a": 1})
  )");
  CHECK(result->get<shared_ptr<yona::interp::runtime::SymbolValue>>()->name == "unit");
}

} // Std.Dict extended
