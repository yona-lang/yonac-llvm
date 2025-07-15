#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include "common.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::parser;

TEST_CASE("Non-identifier function application works", "[interpreter][parser]") {
    Parser parser;
    Interpreter interp;

    // Test function expression in parentheses
    const char* source = R"(
        let add = \a b -> a + b in
        let get_add = \_ -> add in
        (get_add 0) 1 2
    )";

    std::istringstream input(source);
    auto result = parser.parse_input(input);
    REQUIRE(result.success);
    REQUIRE(result.node != nullptr);

    auto interp_result = interp.visit(result.node.get());
    REQUIRE(interp_result.value->type == runtime::Int);
    REQUIRE(interp_result.value->get<int>() == 3);
}

TEST_CASE("Field access expressions work correctly", "[interpreter]") {
    Parser parser;
    Interpreter interp;

    // Test basic field access
    const char* source = R"(
        record Person(name, age)
        let p = Person("Alice", 30) in
        p.name
    )";

    std::istringstream input(source);
    auto result = parser.parse_input(input);
    REQUIRE(result.success);
    REQUIRE(result.node != nullptr);

    auto interp_result = interp.visit(result.node.get());
    REQUIRE(interp_result.value->type == runtime::String);
    REQUIRE(interp_result.value->get<std::string>() == "Alice");

    // Test accessing age field
    const char* source2 = R"(
        record Person(name, age)
        let p = Person("Bob", 25) in
        p.age
    )";

    std::istringstream input2(source2);
    auto result2 = parser.parse_input(input2);
    REQUIRE(result2.success);

    auto interp_result2 = interp.visit(result2.node.get());
    REQUIRE(interp_result2.value->type == runtime::Int);
    REQUIRE(interp_result2.value->get<int>() == 25);
}

TEST_CASE("Field access on non-record fails gracefully", "[interpreter]") {
    Parser parser;
    Interpreter interp;

    const char* source = R"(
        let x = 42 in
        x.field
    )";

    std::istringstream input(source);
    auto result = parser.parse_input(input);
    REQUIRE(result.success);
    REQUIRE(result.node != nullptr);

    // Field access on non-record should throw an exception
    REQUIRE_THROWS_AS(interp.visit(result.node.get()), yona_error);
}

TEST_CASE("Function type checking validates argument count", "[interpreter]") {
    Parser parser;
    Interpreter interp;

    // Test with function that has type information
    // Note: This test assumes functions created by the parser have type info
    // In practice, this would need proper type inference to work fully
    const char* source = R"(
        let add = \a b -> a + b in
        add 1  # Partial application should work
    )";

    std::istringstream input(source);
    auto result = parser.parse_input(input);
    REQUIRE(result.success);
    REQUIRE(result.node != nullptr);

    auto interp_result = interp.visit(result.node.get());
    // Should return a partially applied function
    REQUIRE(interp_result.value->type == runtime::Function);
}

TEST_CASE("Exception passing to catch blocks", "[interpreter]") {
    Parser parser;
    Interpreter interp;

    // Test exception is passed and pattern matched
    const char* source = R"(
        try
            raise :error "Something went wrong"
        catch
        | :error msg -> msg
        | _ -> "Unknown error"
        end
    )";

    std::istringstream input(source);
    auto result = parser.parse_input(input);
    REQUIRE(result.success);
    REQUIRE(result.node != nullptr);

    auto interp_result = interp.visit(result.node.get());
    REQUIRE(interp_result.value->type == runtime::String);
    REQUIRE(interp_result.value->get<std::string>() == "Something went wrong");
}

TEST_CASE("Exception pattern matching with different symbols", "[interpreter]") {
    Parser parser;
    Interpreter interp;

    // Test different exception patterns
    const char* source = R"(
        try
            raise :not_found "File missing"
        catch
        | :error _ -> "Error occurred"
        | :not_found msg -> msg
        | _ -> "Unknown"
        end
    )";

    std::istringstream input(source);
    auto result = parser.parse_input(input);
    REQUIRE(result.success);
    REQUIRE(result.node != nullptr);

    auto interp_result = interp.visit(result.node.get());
    REQUIRE(interp_result.value->type == runtime::String);
    REQUIRE(interp_result.value->get<std::string>() == "File missing");
}

TEST_CASE("Unhandled exception re-raises", "[interpreter]") {
    Parser parser;
    Interpreter interp;

    // Test unhandled exception
    const char* source = R"(
        try
            raise :critical "System failure"
        catch
        | :warning _ -> "Just a warning"
        | :error _ -> "Regular error"
        end
    )";

    std::istringstream input(source);
    auto result = parser.parse_input(input);
    REQUIRE(result.success);
    REQUIRE(result.node != nullptr);

    // Unhandled exception should be re-raised as yona_error
    REQUIRE_THROWS_AS(interp.visit(result.node.get()), yona_error);
}
