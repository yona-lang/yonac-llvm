#include <memory>
#include <sstream>
#include <catch2/catch_test_macros.hpp>

#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"

using namespace std;
using namespace yona::ast;
using namespace yona::parser;
using namespace yona::interp;
using namespace yona::interp::runtime;

struct CurryingTest {
    Parser parser;
    Interpreter interpreter;

    RuntimeObjectPtr eval(const string& code) {
        istringstream stream(code);
        auto parse_result = parser.parse_input(stream);

        if (!parse_result.success) {
            string error_msg = "Parse error";
            if (parse_result.ast_ctx.hasErrors()) {
                auto errors = parse_result.ast_ctx.getErrors();
                if (!errors.empty()) {
                    error_msg += ": ";
                    error_msg += errors.begin()->second->what();
                }
            }
            throw runtime_error(error_msg);
        }

        return any_cast<RuntimeObjectPtr>(parse_result.node->accept(interpreter));
    }
};

// Test basic currying with 2 arguments
TEST_CASE("BasicCurrying", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let add = \(x) -> \(y) -> x + y in
        let add5 = add(5) in
        add5(3)
    )");

    REQUIRE(result->type == runtime::Int);
    CHECK(result->get<int>() == 8);
}

// Test currying with 3 arguments
TEST_CASE("MultipleArgumentCurrying", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let sum3 = \(x) -> \(y) -> \(z) -> x + y + z in
        let sum3_10 = sum3(10) in
        let sum3_10_20 = sum3_10(20) in
        sum3_10_20(30)
    )");

    REQUIRE(result->type == runtime::Int);
    CHECK(result->get<int>() == 60);
}

// Test partial application with multiple arguments at once
TEST_CASE("PartialApplicationMultipleArgs", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let sum3 = \(x) -> \(y) -> \(z) -> x + y + z in
        let sum3_10_20 = sum3(10)(20) in
        sum3_10_20(30)
    )");

    REQUIRE(result->type == runtime::Int);
    CHECK(result->get<int>() == 60);
}

// Test full application
TEST_CASE("FullApplication", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let sum3 = \(x) -> \(y) -> \(z) -> x + y + z in
        sum3(10)(20)(30)
    )");

    REQUIRE(result->type == runtime::Int);
    CHECK(result->get<int>() == 60);
}

// Test over-application (too many arguments)
TEST_CASE("OverApplication", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let add = \(x) -> \(y) -> x + y in
        let sum = add(5)(3) in
        let double = \(x) -> x * 2 in
        double(sum)
    )");

    // add(5)(3) returns 8, then double(8) returns 16
    REQUIRE(result->type == runtime::Int);
    CHECK(result->get<int>() == 16);
}

// Test currying with higher-order functions
TEST_CASE("HigherOrderFunctionCurrying", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let map = \(f) -> \(list) ->
            case list of
            [] -> []
            [head | tail] -> f(head) :: map(f)(tail)
            end
        in
        let double = \(x) -> x * 2 in
        let doubleList = map(double) in
        doubleList([1, 2, 3])
    )");

    REQUIRE(result->type == runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 3);
    CHECK(seq->fields[0]->get<int>() == 2);
    CHECK(seq->fields[1]->get<int>() == 4);
    CHECK(seq->fields[2]->get<int>() == 6);
}

// Test currying with nested functions
TEST_CASE("NestedFunctionCurrying", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let makeAdder = \(x) ->
            \(y) -> x + y
        in
        let add10 = makeAdder(10) in
        add10(5)
    )");

    REQUIRE(result->type == runtime::Int);
    CHECK(result->get<int>() == 15);
}

// Test currying preserves function identity
TEST_CASE("CurryingPreservesFunctionIdentity", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let add = \(x) -> \(y) -> x + y in
        let add5 = add(5) in
        let add5_again = add(5) in
        # Both should produce the same result
        [add5(3), add5_again(3)]
    )");

    REQUIRE(result->type == runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 2);
    CHECK(seq->fields[0]->get<int>() == 8);
    CHECK(seq->fields[1]->get<int>() == 8);
}

// Test currying with pattern matching
TEST_CASE("CurryingWithPatternMatching", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let processList = \(default) -> \(list) ->
            case list of
            [] -> default
            [x | _] -> x
            end
        in
        let getFirstOrDefault = processList(99) in
        [getFirstOrDefault([]), getFirstOrDefault([42, 1, 2])]
    )");

    REQUIRE(result->type == runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 2);
    CHECK(seq->fields[0]->get<int>() == 99);  // [] matches, returns default
    CHECK(seq->fields[1]->get<int>() == 42);  // [x | _] matches, returns x=42
}

// Test zero-argument functions (should not be curried)
TEST_CASE("ZeroArgumentFunction", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let getConstant = 42 in
        getConstant
    )");

    REQUIRE(result->type == runtime::Int);
    CHECK(result->get<int>() == 42);
}

// Test single-argument functions (effectively no currying but should work)
TEST_CASE("SingleArgumentFunction", "[CurryingTest]") /* FIXTURE */ {
    CurryingTest fixture;
    auto result = fixture.eval(R"(
        let double = \(x) -> x * 2 in
        double(21)
    )");

    REQUIRE(result->type == runtime::Int);
    CHECK(result->get<int>() == 42);
}
