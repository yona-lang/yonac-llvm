#include <catch2/catch_test_macros.hpp>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <sstream>

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

struct InterpreterConsTest {
    parser::Parser parser;
    Interpreter interp;

    RuntimeObjectPtr evaluate(const string& code) {
        stringstream input(code);
        auto parse_result = parser.parse_input(input);
        if (!parse_result.success || !parse_result.node) {
            throw runtime_error("Parse failed");
        }
        return any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    }
};

// Test cons left operator (::)
TEST_CASE("ConsLeftBasic", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate("1 :: [2, 3, 4]");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 4);

    CHECK(seq->fields[0]->type == RuntimeObjectType::Int);
    CHECK(seq->fields[0]->get<int>() == 1);
    CHECK(seq->fields[1]->get<int>() == 2);
    CHECK(seq->fields[2]->get<int>() == 3);
    CHECK(seq->fields[3]->get<int>() == 4);
}

TEST_CASE("ConsLeftEmpty", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate("42 :: []");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 1);

    CHECK(seq->fields[0]->type == RuntimeObjectType::Int);
    CHECK(seq->fields[0]->get<int>() == 42);
}

TEST_CASE("ConsLeftChained", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    // Test right associativity: 1 :: 2 :: 3 :: [] should be 1 :: (2 :: (3 :: []))
    auto result = fixture.evaluate("1 :: 2 :: 3 :: []");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 3);

    CHECK(seq->fields[0]->get<int>() == 1);
    CHECK(seq->fields[1]->get<int>() == 2);
    CHECK(seq->fields[2]->get<int>() == 3);
}

// Test cons right operator (:>)
TEST_CASE("ConsRightBasic", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate("[1, 2, 3] :> 4");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 4);

    CHECK(seq->fields[0]->get<int>() == 1);
    CHECK(seq->fields[1]->get<int>() == 2);
    CHECK(seq->fields[2]->get<int>() == 3);
    CHECK(seq->fields[3]->get<int>() == 4);
}

TEST_CASE("ConsRightEmpty", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate("[] :> 42");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 1);

    CHECK(seq->fields[0]->type == RuntimeObjectType::Int);
    CHECK(seq->fields[0]->get<int>() == 42);
}

TEST_CASE("ConsRightChained", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    // Test associativity: [] :> 1 :> 2 :> 3
    auto result = fixture.evaluate("[] :> 1 :> 2 :> 3");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 3);

    CHECK(seq->fields[0]->get<int>() == 1);
    CHECK(seq->fields[1]->get<int>() == 2);
    CHECK(seq->fields[2]->get<int>() == 3);
}

// Test mixing cons operators
TEST_CASE("MixedConsOperators", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate("0 :: [1, 2] :> 3");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 4);

    CHECK(seq->fields[0]->get<int>() == 0);
    CHECK(seq->fields[1]->get<int>() == 1);
    CHECK(seq->fields[2]->get<int>() == 2);
    CHECK(seq->fields[3]->get<int>() == 3);
}

// Test cons with different types
TEST_CASE("ConsWithStrings", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate(R"("hello" :: ["world"])");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 2);

    CHECK(seq->fields[0]->type == RuntimeObjectType::String);
    CHECK(seq->fields[0]->get<string>() == "hello");
    CHECK(seq->fields[1]->get<string>() == "world");
}

// Test type errors
TEST_CASE("ConsLeftTypeError", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    CHECK_THROWS_AS(fixture.evaluate("1 :: 2"), yona_error); // Right side must be a sequence
}

TEST_CASE("ConsRightTypeError", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    CHECK_THROWS_AS(fixture.evaluate("1 :> 2"), yona_error); // Left side must be a sequence
}

// Test cons in pattern matching
TEST_CASE("ConsInPatternMatch", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate(R"(
        case [1, 2, 3] of
            h :: t -> h
            [] -> 0
        end
    )");

    REQUIRE(result->type == RuntimeObjectType::Int);
    CHECK(result->get<int>() == 1);
}

TEST_CASE("ConsPatternWithTail", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate(R"(
        case [1, 2, 3] of
            h :: t -> case t of
                h2 :: _ -> h + h2
                [] -> h
            end
            [] -> 0
        end
    )");

    REQUIRE(result->type == RuntimeObjectType::Int);
    CHECK(result->get<int>() == 3); // 1 + 2
}

// Test cons with byte literals
TEST_CASE("ConsWithBytes", "[InterpreterConsTest]") /* FIXTURE */ {
    InterpreterConsTest fixture;
    auto result = fixture.evaluate("10b :: [20b, 30b]");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 3);

    CHECK(seq->fields[0]->type == RuntimeObjectType::Byte);
    CHECK(static_cast<uint8_t>(seq->fields[0]->get<std::byte>()) == 10);
    CHECK(static_cast<uint8_t>(seq->fields[1]->get<std::byte>()) == 20);
    CHECK(static_cast<uint8_t>(seq->fields[2]->get<std::byte>()) == 30);
}
