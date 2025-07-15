#include <sstream>
#include <doctest/doctest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST_CASE("AdditionInt") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("10 + 20");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 30);
}

TEST_CASE("ComparisonEqual") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("10 == 10");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Bool);
    CHECK(result->get<bool>());
}

TEST_CASE("IfExpression") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("if true then 100 else 200");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 100);
}

TEST_CASE("SequenceConsLeft") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("0 :: [1, 2, 3]");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 4);
    CHECK(seq->fields[0]->get<int>() == 0);
    CHECK(seq->fields[1]->get<int>() == 1);
}

TEST_CASE("InOperatorSeq") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("2 in [1, 2, 3]");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Bool);
    CHECK(result->get<bool>());
}

TEST_CASE("LetExpression") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("let x = 10 in x + 5");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 15);
}

TEST_CASE("LogicalAnd") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("true && false");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Bool);
    CHECK_FALSE(result->get<bool>());
}

TEST_CASE("PowerOperation") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("2 ** 3");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Float);
    CHECK(result->get<double>() == doctest::Approx(8.0));
}

TEST_CASE("ModuloOperation") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("20 % 7");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Int);
    CHECK(result->get<int>() == 6);
}

TEST_CASE("JoinSequences") {
    parser::Parser parser;
    Interpreter interp;

    stringstream ss("[1, 2] ++ [3, 4]");
    auto parse_result = parser.parse_input(ss);

    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get()).value;

    CHECK(result->type == yona::interp::runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    REQUIRE(seq->fields.size() == 4);
    CHECK(seq->fields[0]->get<int>() == 1);
    CHECK(seq->fields[3]->get<int>() == 4);
}
