#include <gtest/gtest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST(InterpreterBasicTest, AdditionInt) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("10 + 20");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 30);
}

TEST(InterpreterBasicTest, ComparisonEqual) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("10 == 10");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Bool);
    EXPECT_TRUE(result->get<bool>());
}

TEST(InterpreterBasicTest, IfExpression) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("if true then 100 else 200");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 100);
}

TEST(InterpreterBasicTest, SequenceConsLeft) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("0 :: [1, 2, 3]");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 4);
    EXPECT_EQ(seq->fields[0]->get<int>(), 0);
    EXPECT_EQ(seq->fields[1]->get<int>(), 1);
}

TEST(InterpreterBasicTest, InOperatorSeq) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("2 in [1, 2, 3]");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Bool);
    EXPECT_TRUE(result->get<bool>());
}

TEST(InterpreterBasicTest, LetExpression) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("let x = 10 in x + 5");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 15);
}

TEST(InterpreterBasicTest, LogicalAnd) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("true && false");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Bool);
    EXPECT_FALSE(result->get<bool>());
}

TEST(InterpreterBasicTest, PowerOperation) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("2 ** 3");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Float);
    EXPECT_DOUBLE_EQ(result->get<double>(), 8.0);
}

TEST(InterpreterBasicTest, ModuloOperation) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("20 % 7");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Int);
    EXPECT_EQ(result->get<int>(), 6);
}

TEST(InterpreterBasicTest, JoinSequences) {
    parser::Parser parser;
    Interpreter interp;
    
    stringstream ss("[1, 2] ++ [3, 4]");
    auto parse_result = parser.parse_input(ss);
    
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, yona::interp::runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 4);
    EXPECT_EQ(seq->fields[0]->get<int>(), 1);
    EXPECT_EQ(seq->fields[3]->get<int>(), 4);
}