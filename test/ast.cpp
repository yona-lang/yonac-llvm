//
// Created by akovari on 15.12.24.
//
#include <gtest/gtest.h>

#include "Interpreter.h"
#include "Parser.h"

using namespace std;
using namespace yona;

using ErrorMap = unordered_map<yona_error::Type, unsigned int>;

class AstTest : public testing::TestWithParam<tuple<string, string, optional<pair<interp::RuntimeObjectType, string>>, ErrorMap>> {};

TEST_P(AstTest, YonaTest) {
  auto param = GetParam();
  auto input = get<1>(param);
  auto expected = get<2>(param);
  auto expected_errors = get<3>(param);

  stringstream ss(input);
  parser::Parser parser;
  parser::ParseResult parse_result = parser.parse_input(ss);

  auto node = parse_result.node;
  auto type = parse_result.type;
  auto type_ctx = parse_result.ast_ctx;

  if (parse_result.success && expected.has_value()) {
    interp::Interpreter interpreter;
    auto result = any_cast<shared_ptr<interp::RuntimeObject>>(node->accept(interpreter));
    EXPECT_EQ(result->type, get<0>(expected.value()));

    stringstream res_ss;
    res_ss << *result;
    EXPECT_EQ(res_ss.str(), get<1>(expected.value()));
  } else {
    for (auto [type, cnt] : expected_errors) {
      EXPECT_EQ(type_ctx.getErrors().count(type), cnt);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(SyntaxTests, AstTest,
                         testing::Values(make_tuple("correct_addition_of_ints", "1+1", make_pair(interp::RuntimeObjectType::Int, "2"), ErrorMap{}),
                                         make_tuple("incomplete_addition", "1+", nullopt, ErrorMap{{yona_error::SYNTAX, 1}})),
                         [](const testing::TestParamInfo<AstTest::ParamType> &info) { return get<0>(info.param); });


INSTANTIATE_TEST_SUITE_P(
    ResultTests, AstTest,
    testing::Values(make_tuple("correct_addition_of_ints", "1+1", make_pair(interp::RuntimeObjectType::Int, "2"), ErrorMap{}),
                    make_tuple("correct_addition_of_floats", "1.0+1.5", make_pair(interp::RuntimeObjectType::Float, "2.5"), ErrorMap{}),
                    make_tuple("correct_subtraction_of_ints", "3 - 1", make_pair(interp::RuntimeObjectType::Int, "2"), ErrorMap{}),
                    make_tuple("correct_subtraction_of_floats", "3.0 - 1.5", make_pair(interp::RuntimeObjectType::Float, "1.5"), ErrorMap{}),
                    make_tuple("correct_multiplication_of_ints", "3 * 6", make_pair(interp::RuntimeObjectType::Int, "18"), ErrorMap{}),
                    make_tuple("correct_multiplication_of_floats", "3.0 * 1.5", make_pair(interp::RuntimeObjectType::Float, "4.5"), ErrorMap{}),
                    make_tuple("correct_division_of_ints", "10/2", make_pair(interp::RuntimeObjectType::Int, "5"), ErrorMap{}),
                    make_tuple("correct_division_of_floats", "3.0/2.0", make_pair(interp::RuntimeObjectType::Float, "1.5"), ErrorMap{}),
                    make_tuple("correct_let_value", "let test = 3+5 in test", make_pair(interp::RuntimeObjectType::Int, "8"), ErrorMap{}),
                    make_tuple("correct_logical_and_1", "true && false", make_pair(interp::RuntimeObjectType::Bool, "false"), ErrorMap{}),
                    make_tuple("correct_logical_and_2", "true && true", make_pair(interp::RuntimeObjectType::Bool, "true"), ErrorMap{}),
                    make_tuple("correct_logical_or_1", "true || false", make_pair(interp::RuntimeObjectType::Bool, "true"), ErrorMap{}),
                    make_tuple("correct_logical_or_2", "false || false", make_pair(interp::RuntimeObjectType::Bool, "false"), ErrorMap{})));

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
