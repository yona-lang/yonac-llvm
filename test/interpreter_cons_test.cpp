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

class InterpreterConsTest : public ::testing::Test {
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

// Test cons left operator (::)
TEST_F(InterpreterConsTest, ConsLeftBasic) {
    auto result = evaluate("1 :: [2, 3, 4]");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 4);
    
    EXPECT_EQ(seq->fields[0]->type, RuntimeObjectType::Int);
    EXPECT_EQ(seq->fields[0]->get<int>(), 1);
    EXPECT_EQ(seq->fields[1]->get<int>(), 2);
    EXPECT_EQ(seq->fields[2]->get<int>(), 3);
    EXPECT_EQ(seq->fields[3]->get<int>(), 4);
}

TEST_F(InterpreterConsTest, ConsLeftEmpty) {
    auto result = evaluate("42 :: []");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 1);
    
    EXPECT_EQ(seq->fields[0]->type, RuntimeObjectType::Int);
    EXPECT_EQ(seq->fields[0]->get<int>(), 42);
}

TEST_F(InterpreterConsTest, ConsLeftChained) {
    // Test right associativity: 1 :: 2 :: 3 :: [] should be 1 :: (2 :: (3 :: []))
    auto result = evaluate("1 :: 2 :: 3 :: []");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 3);
    
    EXPECT_EQ(seq->fields[0]->get<int>(), 1);
    EXPECT_EQ(seq->fields[1]->get<int>(), 2);
    EXPECT_EQ(seq->fields[2]->get<int>(), 3);
}

// Test cons right operator (:>)
TEST_F(InterpreterConsTest, ConsRightBasic) {
    auto result = evaluate("[1, 2, 3] :> 4");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 4);
    
    EXPECT_EQ(seq->fields[0]->get<int>(), 1);
    EXPECT_EQ(seq->fields[1]->get<int>(), 2);
    EXPECT_EQ(seq->fields[2]->get<int>(), 3);
    EXPECT_EQ(seq->fields[3]->get<int>(), 4);
}

TEST_F(InterpreterConsTest, ConsRightEmpty) {
    auto result = evaluate("[] :> 42");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 1);
    
    EXPECT_EQ(seq->fields[0]->type, RuntimeObjectType::Int);
    EXPECT_EQ(seq->fields[0]->get<int>(), 42);
}

TEST_F(InterpreterConsTest, ConsRightChained) {
    // Test associativity: [] :> 1 :> 2 :> 3
    auto result = evaluate("[] :> 1 :> 2 :> 3");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 3);
    
    EXPECT_EQ(seq->fields[0]->get<int>(), 1);
    EXPECT_EQ(seq->fields[1]->get<int>(), 2);
    EXPECT_EQ(seq->fields[2]->get<int>(), 3);
}

// Test mixing cons operators
TEST_F(InterpreterConsTest, MixedConsOperators) {
    auto result = evaluate("0 :: [1, 2] :> 3");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 4);
    
    EXPECT_EQ(seq->fields[0]->get<int>(), 0);
    EXPECT_EQ(seq->fields[1]->get<int>(), 1);
    EXPECT_EQ(seq->fields[2]->get<int>(), 2);
    EXPECT_EQ(seq->fields[3]->get<int>(), 3);
}

// Test cons with different types
TEST_F(InterpreterConsTest, ConsWithStrings) {
    auto result = evaluate(R"("hello" :: ["world"])");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 2);
    
    EXPECT_EQ(seq->fields[0]->type, RuntimeObjectType::String);
    EXPECT_EQ(seq->fields[0]->get<string>(), "hello");
    EXPECT_EQ(seq->fields[1]->get<string>(), "world");
}

// Test type errors
TEST_F(InterpreterConsTest, ConsLeftTypeError) {
    EXPECT_THROW({
        evaluate("1 :: 2"); // Right side must be a sequence
    }, yona_error);
}

TEST_F(InterpreterConsTest, ConsRightTypeError) {
    EXPECT_THROW({
        evaluate("1 :> 2"); // Left side must be a sequence
    }, yona_error);
}

// Test cons in pattern matching
TEST_F(InterpreterConsTest, ConsInPatternMatch) {
    auto result = evaluate(R"(
        case [1, 2, 3] of
            h :: t -> h
            [] -> 0
        end
    )");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Int);
    EXPECT_EQ(result->get<int>(), 1);
}

TEST_F(InterpreterConsTest, ConsPatternWithTail) {
    auto result = evaluate(R"(
        case [1, 2, 3] of
            h :: t -> case t of
                h2 :: _ -> h + h2
                [] -> h
            end
            [] -> 0
        end
    )");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Int);
    EXPECT_EQ(result->get<int>(), 3); // 1 + 2
}

// Test cons with byte literals
TEST_F(InterpreterConsTest, ConsWithBytes) {
    auto result = evaluate("10b :: [20b, 30b]");
    
    ASSERT_EQ(result->type, RuntimeObjectType::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 3);
    
    EXPECT_EQ(seq->fields[0]->type, RuntimeObjectType::Byte);
    EXPECT_EQ(static_cast<uint8_t>(seq->fields[0]->get<std::byte>()), 10);
    EXPECT_EQ(static_cast<uint8_t>(seq->fields[1]->get<std::byte>()), 20);
    EXPECT_EQ(static_cast<uint8_t>(seq->fields[2]->get<std::byte>()), 30);
}