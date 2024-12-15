//
// Created by akovari on 15.12.24.
//
#include <iostream>
#include <gtest/gtest.h>

#include "utils.h"

using namespace std;
using namespace yona;

class AstTest : public testing::TestWithParam<tuple<string, string, unsigned int>> {};

TEST_P(AstTest, MyTest)
{
    auto param = GetParam();
    auto input = get<0>(param);
    auto expected = get<1>(param);
    auto expected_errors = get<2>(param);

    stringstream ss(input);
    auto result = parse_input(ss);

    auto node = result.node;
    auto type = result.type;
    auto type_ctx = result.type_ctx;

    ASSERT_EQ(expected_errors, type_ctx.getErrors().size());
    if (result.success)
    {
        // TODO
    }
}

INSTANTIATE_TEST_SUITE_P(SyntaxTests, AstTest, testing::Values(
    make_tuple("1+1", "2", 0),
    make_tuple("1+'1'", "2", 1)
    ));
