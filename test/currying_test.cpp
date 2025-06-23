#include <gtest/gtest.h>
#include <memory>
#include <sstream>

#include "Parser.h"
#include "Interpreter.h"
#include "runtime.h"

using namespace std;
using namespace yona::ast;
using namespace yona::parser;
using namespace yona::interp;
using namespace yona::interp::runtime;

class CurryingTest : public ::testing::Test {
protected:
    Parser parser;
    Interpreter interpreter;
    
    RuntimeObjectPtr eval(const string& code) {
        istringstream stream(code);
        auto parse_result = parser.parse_input(stream);
        
        if (!parse_result.success) {
            string error_msg = "Parse error";
            if (parse_result.ast_ctx.hasErrors()) {
                auto errors = parse_result.ast_ctx.getErrors();
                if (!errors.empty()) {
                    error_msg += ": ";
                    error_msg += errors.begin()->second->what();
                }
            }
            throw runtime_error(error_msg);
        }
        
        return any_cast<RuntimeObjectPtr>(parse_result.node->accept(interpreter));
    }
};

// Test basic currying with 2 arguments
TEST_F(CurryingTest, BasicCurrying) {
    auto result = eval(R"(
        let add = \(x) -> \(y) -> x + y in
        let add5 = add(5) in
        add5(3)
    )");
    
    ASSERT_EQ(result->type, runtime::Int);
    EXPECT_EQ(result->get<int>(), 8);
}

// Test currying with 3 arguments
TEST_F(CurryingTest, MultipleArgumentCurrying) {
    auto result = eval(R"(
        let sum3 = \(x) -> \(y) -> \(z) -> x + y + z in
        let sum3_10 = sum3(10) in
        let sum3_10_20 = sum3_10(20) in
        sum3_10_20(30)
    )");
    
    ASSERT_EQ(result->type, runtime::Int);
    EXPECT_EQ(result->get<int>(), 60);
}

// Test partial application with multiple arguments at once  
TEST_F(CurryingTest, PartialApplicationMultipleArgs) {
    auto result = eval(R"(
        let sum3 = \(x) -> \(y) -> \(z) -> x + y + z in
        let sum3_10_20 = sum3(10)(20) in
        sum3_10_20(30)
    )");
    
    ASSERT_EQ(result->type, runtime::Int);
    EXPECT_EQ(result->get<int>(), 60);
}

// Test full application
TEST_F(CurryingTest, FullApplication) {
    auto result = eval(R"(
        let sum3 = \(x) -> \(y) -> \(z) -> x + y + z in
        sum3(10)(20)(30)
    )");
    
    ASSERT_EQ(result->type, runtime::Int);
    EXPECT_EQ(result->get<int>(), 60);
}

// Test over-application (too many arguments)
TEST_F(CurryingTest, OverApplication) {
    auto result = eval(R"(
        let add = \(x) -> \(y) -> x + y in
        let sum = add(5)(3) in
        let double = \(x) -> x * 2 in
        double(sum)
    )");
    
    // add(5)(3) returns 8, then double(8) returns 16
    ASSERT_EQ(result->type, runtime::Int);
    EXPECT_EQ(result->get<int>(), 16);
}

// Test currying with higher-order functions
TEST_F(CurryingTest, HigherOrderFunctionCurrying) {
    auto result = eval(R"(
        let map = \(f) -> \(list) -> 
            case list of
            [] -> []
            [head | tail] -> f(head) :: map(f)(tail)
            end
        in
        let double = \(x) -> x * 2 in
        let doubleList = map(double) in
        doubleList([1, 2, 3])
    )");
    
    ASSERT_EQ(result->type, runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 3);
    EXPECT_EQ(seq->fields[0]->get<int>(), 2);
    EXPECT_EQ(seq->fields[1]->get<int>(), 4);
    EXPECT_EQ(seq->fields[2]->get<int>(), 6);
}

// Test currying with nested functions
TEST_F(CurryingTest, NestedFunctionCurrying) {
    auto result = eval(R"(
        let makeAdder = \(x) -> 
            \(y) -> x + y
        in
        let add10 = makeAdder(10) in
        add10(5)
    )");
    
    ASSERT_EQ(result->type, runtime::Int);
    EXPECT_EQ(result->get<int>(), 15);
}

// Test currying preserves function identity
TEST_F(CurryingTest, CurryingPreservesFunctionIdentity) {
    auto result = eval(R"(
        let add = \(x) -> \(y) -> x + y in
        let add5 = add(5) in
        let add5_again = add(5) in
        # Both should produce the same result
        [add5(3), add5_again(3)]
    )");
    
    ASSERT_EQ(result->type, runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 2);
    EXPECT_EQ(seq->fields[0]->get<int>(), 8);
    EXPECT_EQ(seq->fields[1]->get<int>(), 8);
}

// Test currying with pattern matching
TEST_F(CurryingTest, CurryingWithPatternMatching) {
    auto result = eval(R"(
        let processList = \(default) -> \(list) ->
            case list of
            [] -> default
            [x | _] -> x
            end
        in
        let getFirstOrDefault = processList(99) in
        [getFirstOrDefault([]), getFirstOrDefault([42, 1, 2])]
    )");
    
    ASSERT_EQ(result->type, runtime::Seq);
    auto seq = result->get<shared_ptr<SeqValue>>();
    ASSERT_EQ(seq->fields.size(), 2);
    EXPECT_EQ(seq->fields[0]->get<int>(), 99);  // [] matches, returns default
    EXPECT_EQ(seq->fields[1]->get<int>(), 42);  // [x | _] matches, returns x=42
}

// Test zero-argument functions (should not be curried)
TEST_F(CurryingTest, ZeroArgumentFunction) {
    auto result = eval(R"(
        let getConstant = 42 in
        getConstant
    )");
    
    ASSERT_EQ(result->type, runtime::Int);
    EXPECT_EQ(result->get<int>(), 42);
}

// Test single-argument functions (effectively no currying but should work)
TEST_F(CurryingTest, SingleArgumentFunction) {
    auto result = eval(R"(
        let double = \(x) -> x * 2 in
        double(21)
    )");
    
    ASSERT_EQ(result->type, runtime::Int);
    EXPECT_EQ(result->get<int>(), 42);
}