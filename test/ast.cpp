//
// Created by akovari on 15.12.24.
//
#include <iostream>
#include <gtest/gtest.h>
#include <boost/log/utility/setup/file.hpp>

#include "utils.h"

using namespace std;
using namespace yona;


class AstTest : public testing::TestWithParam<tuple<string, string, optional<string>, unsigned int>> {};

TEST_P(AstTest, YonaTest)
{
    auto param = GetParam();
    auto input = get<1>(param);
    auto expected = get<2>(param);
    auto expected_errors = get<3>(param);

    stringstream ss(input);
    auto result = parse_input(ss);

    // auto node = result.node;
    auto type = result.type;
    auto type_ctx = result.ast_ctx;

    ASSERT_EQ(expected_errors, type_ctx.getErrors().size());
    if (result.success)
    {
        // TODO
    }
}

INSTANTIATE_TEST_SUITE_P(SyntaxTests, AstTest, testing::Values(
    make_tuple("correct_addition_of_ints", "1+1", "2", 0),
    make_tuple("failed_addition_of_int_with_char", "1+'1'", "2", 1)
    ),
    [](const testing::TestParamInfo<AstTest::ParamType>& info) {
      return get<0>(info.param);
    });

INSTANTIATE_TEST_SUITE_P(TypeCheckTests, AstTest, testing::Values(
    make_tuple("correct_addition_of_ints", "1+1", "2", 0),
    make_tuple("failed_addition_of_int_with_char", "1+'1'", "2", 1)
    ),
    [](const testing::TestParamInfo<AstTest::ParamType>& info) {
      return get<0>(info.param);
    });

void init_logging()
{
    boost::log::add_file_log("tests.log");
}

int main(int argc, char **argv) {
    init_logging();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
