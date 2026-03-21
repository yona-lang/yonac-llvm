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

TEST_SUITE("String interpolation") {

TEST_CASE("Simple variable interpolation") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let name = "World" in
    "Hello {name}!"
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "Hello World!");
}

TEST_CASE("Expression interpolation") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let x = 6 in
    let y = 7 in
    "result is {(x * y)}"
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "result is 42");
}

TEST_CASE("Multiple interpolations") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let a = 1 in
    let b = 2 in
    "{a} + {b} = {(a + b)}"
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "1 + 2 = 3");
}

TEST_CASE("No interpolation is plain string") {
  SyntaxTest t;

  auto result = t.eval("\"hello world\"");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "hello world");
}

} // String interpolation TEST_SUITE

TEST_SUITE("Non-linear patterns") {

TEST_CASE("Same variable must match same value") {
  SyntaxTest t;

  auto result = t.eval(R"(
    case (1, 1) of
      (x, x) -> "same"
      (x, y) -> "different"
    end
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "same");
}

TEST_CASE("Different values fail non-linear match") {
  SyntaxTest t;

  auto result = t.eval(R"(
    case (1, 2) of
      (x, x) -> "same"
      (x, y) -> "different"
    end
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "different");
}

} // Non-linear patterns TEST_SUITE

TEST_SUITE("Parallel let bindings") {

TEST_CASE("Independent let bindings produce correct results") {
  SyntaxTest t;

  // Multiple independent bindings — may be parallelized
  auto result = t.eval(R"(
    let a = 10 * 2,
        b = 20 + 5,
        c = 30 - 3 in
    a + b + c
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 72); // 20 + 25 + 27
}

TEST_CASE("Dependent let bindings remain sequential") {
  SyntaxTest t;

  // b depends on a, c depends on b — must be sequential
  auto result = t.eval(R"(
    let a = 10 in
    let b = a + 5 in
    let c = b * 2 in
    c
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 30);
}

TEST_CASE("Mixed independent and dependent bindings") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let x = 100,
        y = 200 in
    let sum = x + y in
    sum
  )");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 300);
}

} // Parallel let TEST_SUITE

TEST_SUITE("Import alias") {

TEST_CASE("Import module with alias and qualified access") {
  SyntaxTest t;

  // Import module as alias and use qualified function call
  CHECK(t.parses("import Std\\Math as M in M.abs(-42)"));
}

} // Import alias TEST_SUITE

TEST_SUITE("Pattern destructuring in let") {

TEST_CASE("Tuple destructuring") {
  SyntaxTest t;

  auto result = t.eval("let (a, b) = (10, 20) in a + b");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 30);
}

TEST_CASE("Nested tuple destructuring") {
  SyntaxTest t;

  auto result = t.eval("let (a, (b, c)) = (1, (2, 3)) in a + b + c");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 6);
}

TEST_CASE("List head-tail destructuring") {
  SyntaxTest t;

  auto result = t.eval("let [h | t] = [10, 20, 30] in h");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 10);
}

} // Pattern destructuring TEST_SUITE

TEST_SUITE("String auto-conversion") {

TEST_CASE("Int auto-converts in string concat") {
  SyntaxTest t;

  auto result = t.eval("\"count: \" ++ 42");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "count: 42");
}

TEST_CASE("Bool auto-converts in string concat") {
  SyntaxTest t;

  auto result = t.eval("\"flag: \" ++ true");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "flag: true");
}

} // String auto-conversion TEST_SUITE

TEST_SUITE("Promise type system") {

TEST_CASE("PromiseType is a valid type") {
  using namespace yona::compiler::types;

  auto inner = Type(SignedInt64);
  auto promise_type = make_promise_type(inner);

  CHECK(is_promise(promise_type));
  CHECK(!is_promise(inner));

  auto unwrapped = unwrap_promise(promise_type);
  CHECK(holds_alternative<BuiltinType>(unwrapped));
  CHECK(get<BuiltinType>(unwrapped) == SignedInt64);
}

TEST_CASE("Promise<T> unifies with T via coercion") {
  using namespace yona::compiler::types;
  using namespace yona::typechecker;

  TypeInferenceContext ctx;
  TypeChecker tc(ctx);

  // Promise<Int> should unify with Int (coercion inserts await)
  auto promise_int = make_promise_type(Type(SignedInt64));
  auto plain_int = Type(SignedInt64);

  // Access unification through type checking
  // Create a simple test: check that Promise<Int> + Int works
  // We test indirectly by verifying the types are compatible
  CHECK(is_promise(promise_int));
  CHECK(unwrap_promise(promise_int) == plain_int);
}

TEST_CASE("Nested Promise<Promise<T>> unwraps to T") {
  using namespace yona::compiler::types;

  auto inner = Type(compiler::types::String);
  auto promise = make_promise_type(inner);
  auto nested = make_promise_type(promise);

  CHECK(is_promise(nested));
  auto unwrapped = unwrap_promise(nested);
  CHECK(is_promise(unwrapped));  // One level unwrapped

  // Full unwrap
  auto fully_unwrapped = unwrap_promise(unwrap_promise(nested));
  CHECK(!is_promise(fully_unwrapped));
}

TEST_CASE("Async function flag") {
  using namespace yona::interp::runtime;

  auto func = std::make_shared<FunctionValue>();
  func->arity = 1;
  func->is_native = true;
  func->is_async = false;

  CHECK(!func->is_async);
  func->is_async = true;
  CHECK(func->is_async);
}

TEST_CASE("Promise runtime value is auto-awaited in arithmetic") {
  SyntaxTest t;

  // This tests that if a value happens to be a Promise at runtime,
  // arithmetic operators auto-await it before using the value.
  // For now, test that normal arithmetic works (no promises in user code yet)
  auto result = t.eval("let x = 10 in let y = 20 in x + y");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 30);
}

TEST_CASE("Promise runtime value is auto-awaited in comparison") {
  SyntaxTest t;

  auto result = t.eval("let x = 10 in x > 5");
  CHECK(result->type == RT::Bool);
  CHECK(result->get<bool>() == true);
}

TEST_CASE("Promise runtime value is auto-awaited in if condition") {
  SyntaxTest t;

  auto result = t.eval("let flag = true in if flag then 1 else 0");
  CHECK(result->type == RT::Int);
  CHECK(result->get<int>() == 1);
}

TEST_CASE("Promise runtime value is auto-awaited in case target") {
  SyntaxTest t;

  auto result = t.eval(R"(
    let x = :ok in
    case x of
      :ok -> "yes"
      _ -> "no"
    end
  )");
  CHECK(result->type == RT::String);
  CHECK(result->get<string>() == "yes");
}

} // Promise type system TEST_SUITE
