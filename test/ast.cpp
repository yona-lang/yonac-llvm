//
// Created by akovari on 15.12.24.
//
#include <boost/log/utility/setup/file.hpp>
#include <gtest/gtest.h>
#include <iostream>

#include "utils.h"

using namespace std;
using namespace yona;

using ErrorMap = unordered_map<YonaError::Type, unsigned int>;

class AstTest : public testing::TestWithParam<tuple<string, string, optional<string>, ErrorMap>>
{
};

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

    if (result.success)
    {
        // TODO
    }
    else
    {
        cout << type_ctx.getErrors().size() << endl;
        for (auto [type, cnt] : expected_errors)
        {
            EXPECT_EQ(type_ctx.getErrors(type).size(), cnt);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(SyntaxTests, AstTest,
                         testing::Values(make_tuple("correct_addition_of_ints", "1+1", nullopt, ErrorMap{}),
                                         make_tuple("incomplete_addition", "1+", nullopt,
                                                    ErrorMap{ { YonaError::SYNTAX, 1 } })),
                         [](const testing::TestParamInfo<AstTest::ParamType>& info) { return get<0>(info.param); });

INSTANTIATE_TEST_SUITE_P(TypeCheckTests, AstTest,
                         testing::Values(make_tuple("correct_addition_of_ints", "1+1", "2", ErrorMap{}),
                                         make_tuple("failed_addition_of_int_with_char", "1+'1'", "2",
                                                    ErrorMap{ { YonaError::TYPE, 1 } })),
                         [](const testing::TestParamInfo<AstTest::ParamType>& info) { return get<0>(info.param); });

void init_logging() { boost::log::add_file_log("tests.log"); }

int main(int argc, char** argv)
{
    init_logging();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
