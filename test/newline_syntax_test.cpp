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

struct SyntaxTest {
  parser::Parser parser;
  Interpreter interp;

  RuntimeObjectPtr eval(const string& code) {
    istringstream stream(code);
    auto result = parser.parse_input(stream);
    REQUIRE(result.node != nullptr);
    return interp.visit(result.node.get()).value;
  }

  bool parses(const string& code) {
    parser::Parser p;
    istringstream stream(code);
    auto result = p.parse_input(stream);
    return result.node != nullptr;
  }
};

TEST_SUITE("Newline Syntax") {

TEST_CASE("Newlines delimit case arms") {
  SyntaxTest t;

  // Multi-line case expression
  auto result = t.eval(R"(
    case 1 of
      1 -> "one"
      2 -> "two"
      _ -> "other"
    end
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "one");
}

TEST_CASE("Semicolons delimit case arms") {
  SyntaxTest t;

  // Single-line case with semicolons
  auto result = t.eval("case 2 of 1 -> \"one\"; 2 -> \"two\"; _ -> \"other\" end");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "two");
}

TEST_CASE("Newlines in do blocks") {
  SyntaxTest t;

  auto result = t.eval(R"(
    do
      1 + 1
      2 + 2
      3 + 3
    end
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

TEST_CASE("Semicolons in do blocks") {
  SyntaxTest t;

  auto result = t.eval("do 1 + 1; 2 + 2; 3 + 3 end");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

TEST_CASE("Newlines suppressed inside brackets") {
  SyntaxTest t;

  // Multi-line list
  auto result = t.eval(R"(
    [
      1,
      2,
      3
    ]
  )");
  CHECK(result->type == RT::Seq);

  // Multi-line tuple
  auto result2 = t.eval(R"(
    (
      1,
      2,
      3
    )
  )");
  CHECK(result2->type == RT::Tuple);
}

TEST_CASE("Newlines suppressed after binary operators") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let x = 1 +
      2 +
      3 in
    x
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

TEST_CASE("Newlines suppressed after arrow") {
  SyntaxTest t;

  auto result = t.eval(R"(
    case 1 of
      1 ->
        42
      _ ->
        0
    end
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 42);
}

TEST_CASE("Multiline if-then-else") {
  SyntaxTest t;

  auto result = t.eval(R"(
    if true
    then
      42
    else
      0
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 42);
}

TEST_CASE("Multiline let expression") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let
      x = 10,
      y = 20
    in
      x + y
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 30);
}

TEST_CASE("Multiline try-catch") {
  SyntaxTest t;

  auto result = t.eval(R"(
    try
      raise :test_error "oops"
    catch
      _ -> 42
    end
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 42);
}

} // TEST_SUITE

TEST_SUITE("Juxtaposition") {

TEST_CASE("Simple juxtaposition") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let f = \(x) -> x + 1 in
    f 5
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

TEST_CASE("Multiple arguments via juxtaposition") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let f = \(x) -> \(y) -> x + y in
    f 3 4
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 7);
}

TEST_CASE("Juxtaposition with list argument") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let head = \(lst) -> case lst of [h | t] -> h; _ -> 0 end in
    head [42, 1, 2]
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 42);
}

TEST_CASE("Juxtaposition with parenthesized argument") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let f = \(x) -> x * 2 in
    f (3 + 4)
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 14);
}

TEST_CASE("Juxtaposition has higher precedence than operators") {
  SyntaxTest t;

  // f x + g y = (f x) + (g y)
  auto result = t.eval(R"(
    let f = \(x) -> x * 10 in
    let g = \(x) -> x * 100 in
    f 2 + g 3
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 320);
}

TEST_CASE("Juxtaposition stops at newline") {
  SyntaxTest t;

  // Each line is a separate expression in a do block
  auto result = t.eval(R"(
    let f = \(x) -> x + 1 in
    do
      f 1
      f 2
      f 3
    end
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 4); // last expression: f 3 = 4
}

} // TEST_SUITE

TEST_SUITE("Function-style let bindings") {

TEST_CASE("Simple function binding") {
  SyntaxTest t;

  auto result = t.eval("let double x = x * 2 in double 5");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 10);
}

TEST_CASE("Multi-param function binding") {
  SyntaxTest t;

  auto result = t.eval("let add x y = x + y in add 3 4");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 7);
}

TEST_CASE("Underscore in let binding") {
  SyntaxTest t;

  auto result = t.eval("let _ = 42 in 7");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 7);
}

TEST_CASE("Multiple comma-separated let bindings") {
  SyntaxTest t;

  auto result = t.eval("let x = 10, y = 20 in x + y");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 30);
}

} // TEST_SUITE

TEST_SUITE("Lambda syntax") {

TEST_CASE("Lambda with parenthesized params") {
  SyntaxTest t;

  auto result = t.eval("(\\(x) -> x + 1)(5)");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

TEST_CASE("Lambda with bare params") {
  SyntaxTest t;

  auto result = t.eval("(\\x -> x + 1)(5)");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

TEST_CASE("Lambda with multiple bare params") {
  SyntaxTest t;

  auto result = t.eval("(\\x y -> x + y)(3, 4)");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 7);
}

} // TEST_SUITE

TEST_SUITE("Symbol equality") {

TEST_CASE("Symbol pattern matching") {
  SyntaxTest t;

  auto result = t.eval(R"(
    case :ok of
      :ok -> "matched"
      _ -> "no match"
    end
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "matched");
}

TEST_CASE("Symbol equality comparison") {
  SyntaxTest t;

  auto result = t.eval(":ok == :ok");
  CHECK(result->type == RT::Bool);
  CHECK(result->get<bool>() == true);
}

TEST_CASE("Symbol inequality") {
  SyntaxTest t;

  auto result = t.eval(":ok == :error");
  CHECK(result->type == RT::Bool);
  CHECK(result->get<bool>() == false);
}

TEST_CASE("OR pattern with symbols") {
  SyntaxTest t;

  auto result = t.eval(R"(
    case :ok of
      :ok | :success -> "good"
      :error | :failure -> "bad"
      _ -> "unknown"
    end
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "good");
}

} // Symbol equality TEST_SUITE

TEST_SUITE("Zero-arity functions") {

TEST_CASE("Zero-arity function auto-evaluates") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let make_val = \-> 42 in
    make_val
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 42);
}

TEST_CASE("Thunk syntax") {
  SyntaxTest t;

  auto result = t.eval("(\\-> 42)()");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 42);
}

TEST_CASE("Non-zero arity function is not auto-called") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let f = \(x) -> x + 1 in
    let g = f in
    g 5
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

} // Zero-arity TEST_SUITE
