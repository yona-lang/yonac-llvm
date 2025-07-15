#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::parser;

TEST_CASE("Parser can handle OR patterns", "[parser][patterns]") {
    Parser parser;
    Interpreter interp;

    // Test parsing: case x of 1 | 2 | 3 -> "small" end
    const char* source = R"(
        case 2 of
        1 | 2 | 3 -> "small"
        _ -> "large"
        end
    )";

    std::istringstream input(source);
    auto result = parser.parse_input(input);
    REQUIRE(result.success);
    REQUIRE(result.node != nullptr);

    // Just run the interpreter to test if it works
    auto interp_result = interp.visit(result.node.get());
    REQUIRE(interp_result.value->type == runtime::String);
    REQUIRE(interp_result.value->get<std::string>() == "small");
}

TEST_CASE("Parser can handle literal patterns", "[parser][patterns]") {
    Parser parser;

    // Test float literal pattern
    const char* float_source = R"(
        case 3.14 of
        3.14 -> "pi"
        _ -> "not pi"
        end
    )";

    std::istringstream float_input(float_source);
    auto float_result = parser.parse_input(float_input);
    REQUIRE(float_result.success);
    REQUIRE(float_result.node != nullptr);

    // Test string literal pattern
    const char* string_source = R"(
        case "hello" of
        "hello" -> "greeting"
        "bye" -> "farewell"
        _ -> "unknown"
        end
    )";

    std::istringstream string_input(string_source);
    auto string_result = parser.parse_input(string_input);
    REQUIRE(string_result.success);
    REQUIRE(string_result.node != nullptr);

    // Test boolean literal patterns
    const char* bool_source = R"(
        case true of
        true -> "yes"
        false -> "no"
        end
    )";

    std::istringstream bool_input(bool_source);
    auto bool_result = parser.parse_input(bool_input);
    REQUIRE(bool_result.success);
    REQUIRE(bool_result.node != nullptr);
}

TEST_CASE("Interpreter handles OR patterns correctly", "[interpreter][patterns]") {
    Parser parser;
    Interpreter interp;

    const char* source = R"(
        case 2 of
        1 | 2 | 3 -> "small"
        4 | 5 | 6 -> "medium"
        _ -> "large"
        end
    )";

    std::istringstream input(source);
    auto parse_result = parser.parse_input(input);
    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get());
    REQUIRE(result.value->type == runtime::String);
    REQUIRE(result.value->get<std::string>() == "small");

    // Test with value that matches second pattern
    const char* source2 = R"(
        case 5 of
        1 | 2 | 3 -> "small"
        4 | 5 | 6 -> "medium"
        _ -> "large"
        end
    )";

    std::istringstream input2(source2);
    auto parse_result2 = parser.parse_input(input2);
    REQUIRE(parse_result2.success);
    auto result2 = interp.visit(parse_result2.node.get());
    REQUIRE(result2.value->type == runtime::String);
    REQUIRE(result2.value->get<std::string>() == "medium");
}

TEST_CASE("Interpreter handles string literal patterns", "[interpreter][patterns]") {
    Parser parser;
    Interpreter interp;

    const char* source = R"(
        case "yes" of
        "yes" | "y" | "true" -> true
        "no" | "n" | "false" -> false
        _ -> false
        end
    )";

    std::istringstream input(source);
    auto parse_result = parser.parse_input(input);
    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get());
    REQUIRE(result.value->type == runtime::Bool);
    REQUIRE(result.value->get<bool>() == true);
}

TEST_CASE("Interpreter handles record patterns", "[interpreter][patterns]") {
    Parser parser;
    Interpreter interp;

    // Define and use a record with pattern matching
    const char* source = R"(
        record Person(name, age)
        let p = Person("Alice", 30) in
        case p of
        Person(n, _) -> n
        end
    )";

    std::istringstream input(source);
    auto parse_result = parser.parse_input(input);
    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);

    auto result = interp.visit(parse_result.node.get());
    REQUIRE(result.value->type == runtime::String);
    REQUIRE(result.value->get<std::string>() == "Alice");
}

TEST_CASE("Interpreter handles all literal pattern types", "[interpreter][patterns]") {
    Parser parser;
    Interpreter interp;

    // Test float patterns
    const char* float_test = R"(
        case 3.14 of
        3.14 -> "pi"
        2.71 -> "e"
        _ -> "other"
        end
    )";

    std::istringstream float_stream(float_test);
    auto float_parse = parser.parse_input(float_stream);
    REQUIRE(float_parse.success);
    auto float_result = interp.visit(float_parse.node.get());
    REQUIRE(float_result.value->type == runtime::String);
    REQUIRE(float_result.value->get<std::string>() == "pi");

    // Test character patterns
    const char* char_test = R"(
        case 'A' of
        'A' -> 65
        'B' -> 66
        _ -> 0
        end
    )";

    std::istringstream char_stream(char_test);
    auto char_parse = parser.parse_input(char_stream);
    REQUIRE(char_parse.success);
    auto char_result = interp.visit(char_parse.node.get());
    REQUIRE(char_result.value->type == runtime::Int);
    REQUIRE(char_result.value->get<int>() == 65);

    // Test boolean patterns
    const char* bool_test = R"(
        case true of
        true -> 1
        false -> 0
        end
    )";

    std::istringstream bool_stream(bool_test);
    auto bool_parse = parser.parse_input(bool_stream);
    REQUIRE(bool_parse.success);
    auto bool_result = interp.visit(bool_parse.node.get());
    REQUIRE(bool_result.value->type == runtime::Int);
    REQUIRE(bool_result.value->get<int>() == 1);
}
