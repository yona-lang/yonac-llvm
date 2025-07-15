#include <sstream>
#include <doctest/doctest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <sstream>
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

struct InterpreterByteTest {
    parser::Parser parser;
    Interpreter interp;

    RuntimeObjectPtr evaluate(const string& code) {
        stringstream input(code);
        auto parse_result = parser.parse_input(input);
        if (!parse_result.success || !parse_result.node) {
            throw runtime_error("Parse failed");
        }
        return interp.visit(parse_result.node.get()).value;
    }
};

// Test basic byte literals
TEST_CASE("BasicByteLiteral") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("42b");

    REQUIRE(result->type == RuntimeObjectType::Byte);
    CHECK(static_cast<uint8_t>(result->get<std::byte>()) == 42);
}

TEST_CASE("ByteLiteralUppercase") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("100B");

    REQUIRE(result->type == RuntimeObjectType::Byte);
    CHECK(static_cast<uint8_t>(result->get<std::byte>()) == 100);
}

TEST_CASE("ByteLiteralZero") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("0b");

    REQUIRE(result->type == RuntimeObjectType::Byte);
    CHECK(static_cast<uint8_t>(result->get<std::byte>()) == 0);
}

TEST_CASE("ByteLiteralMax") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("255b");

    REQUIRE(result->type == RuntimeObjectType::Byte);
    CHECK(static_cast<uint8_t>(result->get<std::byte>()) == 255);
}

// Test byte arithmetic
TEST_CASE("ByteAddition") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("10b + 20b");

    REQUIRE(result->type == RuntimeObjectType::Int); // Arithmetic promotes to Int
    CHECK(result->get<int>() == 30);
}

TEST_CASE("ByteSubtraction") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("100b - 50b");

    REQUIRE(result->type == RuntimeObjectType::Int);
    CHECK(result->get<int>() == 50);
}

TEST_CASE("ByteMultiplication") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("5b * 6b");

    REQUIRE(result->type == RuntimeObjectType::Int);
    CHECK(result->get<int>() == 30);
}

// Test byte comparisons
TEST_CASE("ByteEquality") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("42b == 42b");

    REQUIRE(result->type == RuntimeObjectType::Bool);
    CHECK(result->get<bool>());
}

TEST_CASE("ByteInequality") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("10b != 20b");

    REQUIRE(result->type == RuntimeObjectType::Bool);
    CHECK(result->get<bool>());
}

TEST_CASE("ByteLessThan") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("10b < 20b");

    REQUIRE(result->type == RuntimeObjectType::Bool);
    CHECK(result->get<bool>());
}

// Test byte in collections
TEST_CASE("ByteInList") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("[1b, 2b, 3b]");

    REQUIRE(result->type == RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 3);

    CHECK(seq->fields[0]->type == RuntimeObjectType::Byte);
    CHECK(static_cast<uint8_t>(seq->fields[0]->get<std::byte>()) == 1);
    CHECK(static_cast<uint8_t>(seq->fields[1]->get<std::byte>()) == 2);
    CHECK(static_cast<uint8_t>(seq->fields[2]->get<std::byte>()) == 3);
}

TEST_CASE("ByteInTuple") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate("(255b, 0b, 128b)");

    REQUIRE(result->type == RuntimeObjectType::Tuple);
    auto tuple = result->get<shared_ptr<TupleValue>>();
    REQUIRE(tuple->fields.size() == 3);

    CHECK(tuple->fields[0]->type == RuntimeObjectType::Byte);
    CHECK(static_cast<uint8_t>(tuple->fields[0]->get<std::byte>()) == 255);
    CHECK(static_cast<uint8_t>(tuple->fields[1]->get<std::byte>()) == 0);
    CHECK(static_cast<uint8_t>(tuple->fields[2]->get<std::byte>()) == 128);
}

// Test byte in pattern matching
TEST_CASE("ByteInPatternMatch") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate(R"(
        case 42b of
            0b -> "zero"
            42b -> "forty-two"
            _ -> "other"
        end
    )");

    REQUIRE(result->type == RuntimeObjectType::String);
    CHECK(result->get<string>() == "forty-two");
}

TEST_CASE("BytePatternWithVariable") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate(R"(
        case 100b of
            0b -> 0
            x -> x + 1b
        end
    )");

    REQUIRE(result->type == RuntimeObjectType::Int);
    CHECK(result->get<int>() == 101);
}

// Test byte with functions
TEST_CASE("ByteAsParameter") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate(R"(
        let inc = \(x) -> x + 1b in
        inc(10b)
    )");

    REQUIRE(result->type == RuntimeObjectType::Int);
    CHECK(result->get<int>() == 11);
}

TEST_CASE("ByteInLetBinding") /* FIXTURE */ {
    InterpreterByteTest fixture;
    auto result = fixture.evaluate(R"(
        let x = 50b in
        let y = 25b in
        x + y
    )");

    REQUIRE(result->type == RuntimeObjectType::Int);
    CHECK(result->get<int>() == 75);
}
