//
// Created by akovari on 15.12.24.
//
#include <boost/log/utility/setup/file.hpp>
#include <gtest/gtest.h>

#include "Interpreter.h"
#include "Parser.h"

using namespace std;
using namespace yona;

using ErrorMap = unordered_map<yona_error::Type, unsigned int>;

class AstTest : public testing::TestWithParam<tuple<string, string, optional<tuple<interp::RuntimeObjectType, any>>, ErrorMap>> {};

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
  } else {
    for (auto [type, cnt] : expected_errors) {
      EXPECT_EQ(type_ctx.getErrors().count(type), cnt);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(SyntaxTests, AstTest,
                         testing::Values(make_tuple("correct_addition_of_ints", "1+1", make_tuple(interp::RuntimeObjectType::Int, 2), ErrorMap{}),
                                         make_tuple("incomplete_addition", "1+", nullopt, ErrorMap{{yona_error::SYNTAX, 1}})),
                         [](const testing::TestParamInfo<AstTest::ParamType> &info) { return get<0>(info.param); });

INSTANTIATE_TEST_SUITE_P(TypeCheckTests, AstTest,
                         testing::Values(make_tuple("correct_addition_of_ints", "1+1", make_tuple(interp::RuntimeObjectType::Int, 2), ErrorMap{}),
                                         make_tuple("correct_addition_of_floats", "1.0+1.5", make_tuple(interp::RuntimeObjectType::Float, 2.5),
                                                    ErrorMap{}),
                                         make_tuple("failed_addition_of_int_with_char", "1+'1'", make_tuple(interp::RuntimeObjectType::Int, 2),
                                                    ErrorMap{{yona_error::TYPE, 1}})),
                         [](const testing::TestParamInfo<AstTest::ParamType> &info) { return get<0>(info.param); });

void init_logging() { boost::log::add_file_log("tests.log"); }

int main(int argc, char **argv) {
  init_logging();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
