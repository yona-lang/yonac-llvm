#include <sstream>
//
// Created by akovari on 15.12.24.
//
#include <doctest/doctest.h>

#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include "common.h"
#include "ast_visitor_impl.h"
#include <unordered_map>
#include <optional>
#include <sstream>

using namespace std;
using namespace yona;
using namespace yona::interp;
using namespace yona::interp::runtime;

using ErrorMap = unordered_map<yona_error::Type, unsigned int>;

// Data-driven tests converted from Catch2
TEST_CASE("YonaTest - SimpleAddition") {
    stringstream ss("1 + 2");
    parser::Parser parser;
    parser::ParseResult parse_result = parser.parse_input(ss);

    auto node = parse_result.node;
    auto type = parse_result.type;
    auto type_ctx = parse_result.ast_ctx;

    if (parse_result.success) {
      Interpreter interpreter;
      auto interpreter_result = node->accept(interpreter);
      auto result = interpreter_result.value;
      CHECK(result->type == Int);

      stringstream res_ss;
      res_ss << *result;
      CHECK(res_ss.str() == "3");
    }
}

TEST_CASE("YonaTest - SimpleSubtraction") {
    stringstream ss("5 - 3");
    parser::Parser parser;
    parser::ParseResult parse_result = parser.parse_input(ss);

    auto node = parse_result.node;
    auto type = parse_result.type;
    auto type_ctx = parse_result.ast_ctx;

    if (parse_result.success) {
      Interpreter interpreter;
      auto interpreter_result = node->accept(interpreter);
      auto result = interpreter_result.value;
      CHECK(result->type == Int);

      stringstream res_ss;
      res_ss << *result;
      CHECK(res_ss.str() == "2");
    }
}

TEST_CASE("YonaTest - ParseError") {
    stringstream ss("1 +");
    parser::Parser parser;
    parser::ParseResult parse_result = parser.parse_input(ss);

    auto node = parse_result.node;
    auto type = parse_result.type;
    auto type_ctx = parse_result.ast_ctx;

    ErrorMap expected_errors = {{yona_error::Type::SYNTAX, 1}};

    for (auto [error_type, cnt] : expected_errors) {
        CHECK(type_ctx.getErrors().count(error_type) == cnt);
    }
}
