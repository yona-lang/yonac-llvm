#include <sstream>
#include <iostream>
#include <doctest/doctest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST_SUITE("ComprehensiveTests") {

TEST_CASE("Interpreter - Pattern Matching") {
    parser::Parser parser;
    Interpreter interp;

    SUBCASE("List pattern matching") {
        stringstream ss(R"(
            case [1, 2, 3] of
                [] -> 0
                [h | t] -> h
            end
        )");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 1);
    }

    SUBCASE("Tuple pattern matching") {
        stringstream ss(R"(
            case (1, 2) of
                (a, b) -> a + b
            end
        )");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 3);
    }

    SUBCASE("Wildcard pattern") {
        stringstream ss(R"(
            case 42 of
                _ -> 100
            end
        )");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 100);
    }
}

TEST_CASE("Interpreter - Arithmetic") {
    parser::Parser parser;
    Interpreter interp;

    SUBCASE("Integer arithmetic") {
        stringstream ss("2 + 3 * 4 - 1");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 13);
    }

    SUBCASE("Float arithmetic") {
        stringstream ss("3.14 + 2.86");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Float);
        CHECK(result.value->get<double>() == doctest::Approx(6.0));
    }
}

TEST_CASE("Interpreter - Let Bindings") {
    parser::Parser parser;
    Interpreter interp;

    SUBCASE("Simple let") {
        stringstream ss("let x = 10 in x * 2");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 20);
    }

    SUBCASE("Nested let") {
        stringstream ss("let x = 5 in let y = x + 3 in y * 2");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 16);
    }
}

TEST_CASE("Interpreter - Control Flow") {
    parser::Parser parser;
    Interpreter interp;

    SUBCASE("If expression - true branch") {
        stringstream ss("if 5 > 3 then 100 else 200 end");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 100);
    }

    SUBCASE("If expression - false branch") {
        stringstream ss("if 2 > 5 then 100 else 200 end");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 200);
    }
}

TEST_CASE("Parser - Type Declarations") {
    parser::Parser parser;

    SUBCASE("Type declarations require module context") {
        stringstream ss("type MyInt = Int");
        auto parse_result = parser.parse_input(ss);
        CHECK(!parse_result.success);
    }

    SUBCASE("Type declaration parsing in module") {
        string module_source = R"(
module TestTypes exports foo as
type MyInt = Int
type Option = None | Some
foo x = x + 1
end
)";
        auto parse_result = parser.parse_module(module_source, "test.yona");
        CHECK(parse_result.has_value());
    }
}

TEST_CASE("Exception Handling") {
    parser::Parser parser;
    Interpreter interp;

    SUBCASE("Try-catch basic") {
        stringstream ss(R"(
            try
                42
            catch
                _ -> 0
            end
        )");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto result = interp.visit(main);
        CHECK(result.value->type == yona::interp::runtime::Int);
        CHECK(result.value->get<int>() == 42);
    }
}

} // TEST_SUITE
