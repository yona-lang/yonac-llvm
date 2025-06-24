#include <gtest/gtest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <sstream>

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

class InterpreterByteTest : public ::testing::Test {
protected:
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

// Test basic byte literals
TEST_F(InterpreterByteTest, BasicByteLiteral) {
    auto result = evaluate("42b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Byte);
    EXPECT_EQ(static_cast<uint8_t>(result->get<std::byte>()), 42);
}

TEST_F(InterpreterByteTest, ByteLiteralUppercase) {
    auto result = evaluate("100B");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Byte);
    EXPECT_EQ(static_cast<uint8_t>(result->get<std::byte>()), 100);
}

TEST_F(InterpreterByteTest, ByteLiteralZero) {
    auto result = evaluate("0b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Byte);
    EXPECT_EQ(static_cast<uint8_t>(result->get<std::byte>()), 0);
}

TEST_F(InterpreterByteTest, ByteLiteralMax) {
    auto result = evaluate("255b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Byte);
    EXPECT_EQ(static_cast<uint8_t>(result->get<std::byte>()), 255);
}

// Test byte arithmetic
TEST_F(InterpreterByteTest, ByteAddition) {
    auto result = evaluate("10b + 20b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Int); // Arithmetic promotes to Int
    EXPECT_EQ(result->get<int>(), 30);
}

TEST_F(InterpreterByteTest, ByteSubtraction) {
    auto result = evaluate("100b - 50b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Int);
    EXPECT_EQ(result->get<int>(), 50);
}

TEST_F(InterpreterByteTest, ByteMultiplication) {
    auto result = evaluate("5b * 6b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Int);
    EXPECT_EQ(result->get<int>(), 30);
}

// Test byte comparisons
TEST_F(InterpreterByteTest, ByteEquality) {
    auto result = evaluate("42b == 42b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Bool);
    EXPECT_TRUE(result->get<bool>());
}

TEST_F(InterpreterByteTest, ByteInequality) {
    auto result = evaluate("10b != 20b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Bool);
    EXPECT_TRUE(result->get<bool>());
}

TEST_F(InterpreterByteTest, ByteLessThan) {
    auto result = evaluate("10b < 20b");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Bool);
    EXPECT_TRUE(result->get<bool>());
}

// Test byte in collections
TEST_F(InterpreterByteTest, ByteInList) {
    auto result = evaluate("[1b, 2b, 3b]");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 3);
    
    EXPECT_EQ(seq->fields[0]->type, RuntimeObjectType::Byte);
    EXPECT_EQ(static_cast<uint8_t>(seq->fields[0]->get<std::byte>()), 1);
    EXPECT_EQ(static_cast<uint8_t>(seq->fields[1]->get<std::byte>()), 2);
    EXPECT_EQ(static_cast<uint8_t>(seq->fields[2]->get<std::byte>()), 3);
}

TEST_F(InterpreterByteTest, ByteInTuple) {
    auto result = evaluate("(255b, 0b, 128b)");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Tuple);
    auto tuple = result->get<shared_ptr<TupleValue>>();
    ASSERT_EQ(tuple->fields.size(), 3);
    
    EXPECT_EQ(tuple->fields[0]->type, RuntimeObjectType::Byte);
    EXPECT_EQ(static_cast<uint8_t>(tuple->fields[0]->get<std::byte>()), 255);
    EXPECT_EQ(static_cast<uint8_t>(tuple->fields[1]->get<std::byte>()), 0);
    EXPECT_EQ(static_cast<uint8_t>(tuple->fields[2]->get<std::byte>()), 128);
}

// Test byte in pattern matching
TEST_F(InterpreterByteTest, ByteInPatternMatch) {
    auto result = evaluate(R"(
        case 42b of
            0b -> "zero"
            42b -> "forty-two"
            _ -> "other"
        end
    )");
    
    ASSERT_EQ(result->type, RuntimeObjectType::String);
    EXPECT_EQ(result->get<string>(), "forty-two");
}

TEST_F(InterpreterByteTest, BytePatternWithVariable) {
    auto result = evaluate(R"(
        case 100b of
            0b -> 0
            x -> x + 1b
        end
    )");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Int);
    EXPECT_EQ(result->get<int>(), 101);
}

// Test byte with functions
TEST_F(InterpreterByteTest, ByteAsParameter) {
    auto result = evaluate(R"(
        let inc = \(x) -> x + 1b in
        inc(10b)
    )");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Int);
    EXPECT_EQ(result->get<int>(), 11);
}

TEST_F(InterpreterByteTest, ByteInLetBinding) {
    auto result = evaluate(R"(
        let x = 50b in
        let y = 25b in
        x + y
    )");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Int);
    EXPECT_EQ(result->get<int>(), 75);
}