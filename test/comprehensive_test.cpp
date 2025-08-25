#include <sstream>
#include <iostream>
#include <doctest/doctest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "TypeChecker.h"
#include "runtime.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace yona::typechecker;
using namespace std;

TEST_SUITE("ComprehensiveTests") {

// Test suite for all implemented features

TEST_CASE("TypeChecker - Literals") {
    parser::Parser parser;
    TypeInferenceContext ctx;
    TypeChecker type_checker(ctx);

    SUBCASE("Integer literal") {
        stringstream ss("42");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::SignedInt64);
    }

    SUBCASE("Float literal") {
        stringstream ss("3.14");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::Float64);
    }

    SUBCASE("String literal") {
        stringstream ss("\"hello\"");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::String);
    }

    SUBCASE("Boolean literals") {
        stringstream ss1("true");
        auto parse_result1 = parser.parse_input(ss1);
        REQUIRE(parse_result1.success);
        auto main1 = dynamic_cast<MainNode*>(parse_result1.node.get());
        auto type1 = type_checker.check(main1->node);
        CHECK(holds_alternative<BuiltinType>(type1));
        CHECK(get<BuiltinType>(type1) == compiler::types::Bool);

        stringstream ss2("false");
        auto parse_result2 = parser.parse_input(ss2);
        REQUIRE(parse_result2.success);
        auto main2 = dynamic_cast<MainNode*>(parse_result2.node.get());
        auto type2 = type_checker.check(main2->node);
        CHECK(holds_alternative<BuiltinType>(type2));
        CHECK(get<BuiltinType>(type2) == compiler::types::Bool);
    }
}

TEST_CASE("TypeChecker - Arithmetic Operations") {
    parser::Parser parser;
    TypeInferenceContext ctx;
    TypeChecker type_checker(ctx);

    SUBCASE("Addition") {
        stringstream ss("1 + 2");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::SignedInt64);
    }

    SUBCASE("Subtraction") {
        stringstream ss("5 - 3");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::SignedInt64);
    }

    SUBCASE("Multiplication") {
        stringstream ss("4 * 6");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::SignedInt64);
    }

    SUBCASE("Division") {
        stringstream ss("10 / 2");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::Float64);
    }
}

TEST_CASE("TypeChecker - Logical Operations") {
    parser::Parser parser;
    TypeInferenceContext ctx;
    TypeChecker type_checker(ctx);

    SUBCASE("Logical AND") {
        stringstream ss("true && false");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::Bool);
    }

    SUBCASE("Logical OR") {
        stringstream ss("true || false");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::Bool);
    }

    SUBCASE("Logical NOT") {
        stringstream ss("!true");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::Bool);
    }
}

TEST_CASE("TypeChecker - Comparison Operations") {
    parser::Parser parser;
    TypeInferenceContext ctx;
    TypeChecker type_checker(ctx);

    SUBCASE("Equality") {
        stringstream ss("5 == 5");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::Bool);
    }

    SUBCASE("Less than") {
        stringstream ss("3 < 5");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::Bool);
    }
}

TEST_CASE("TypeChecker - Let Expressions") {
    parser::Parser parser;
    TypeInferenceContext ctx;
    TypeChecker type_checker(ctx);

    SUBCASE("Simple let binding") {
        stringstream ss("let x = 42 in x + 1");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::SignedInt64);
    }

    SUBCASE("Multiple bindings") {
        stringstream ss("let x = 1, y = 2 in x + y");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::SignedInt64);
    }
}

TEST_CASE("TypeChecker - If Expressions") {
    parser::Parser parser;
    TypeInferenceContext ctx;
    TypeChecker type_checker(ctx);

    SUBCASE("Simple if expression") {
        stringstream ss("if true then 1 else 2 end");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<BuiltinType>(type));
        CHECK(get<BuiltinType>(type) == compiler::types::SignedInt64);
    }
}

TEST_CASE("TypeChecker - Collections") {
    parser::Parser parser;
    TypeInferenceContext ctx;
    TypeChecker type_checker(ctx);

    SUBCASE("Tuple") {
        stringstream ss("(1, 2, 3)");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<shared_ptr<ProductType>>(type));
    }

    SUBCASE("Sequence") {
        stringstream ss("[1, 2, 3]");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<shared_ptr<SingleItemCollectionType>>(type));
    }

    SUBCASE("Set") {
        stringstream ss("{1, 2, 3}");
        auto parse_result = parser.parse_input(ss);
        REQUIRE(parse_result.success);
        auto main = dynamic_cast<MainNode*>(parse_result.node.get());
        auto type = type_checker.check(main->node);
        CHECK(holds_alternative<shared_ptr<SingleItemCollectionType>>(type));
    }
}

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
        CHECK(result.value->get<int>() == 13);  // 2 + 12 - 1
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
        // Type declarations are not supported as standalone expressions
        // They must be inside a module definition
        stringstream ss("type MyInt = Int");
        auto parse_result = parser.parse_input(ss);
        CHECK(!parse_result.success);  // Should fail - not in module
    }

    SUBCASE("Type declaration parsing in module") {
        // Note: Type declarations are parsed but not fully processed
        // The parser recognizes them but doesn't convert them to AST nodes yet
        // This is a known limitation - see Parser.cpp line 267-270
        string module_source = R"(
module TestTypes exports foo as
type MyInt = Int
type Option = None | Some
foo x = x + 1
end
)";
        auto parse_result = parser.parse_module(module_source, "test.yona");

        // Print error if parsing failed
        if (!parse_result.has_value()) {
            for (const auto& error : parse_result.error()) {
                std::cout << "Parse error: " << error.message << " at "
                         << error.location.filename << ":"
                         << error.location.line << ":"
                         << error.location.column << std::endl;
            }
        }

        // The parser should handle modules with type declarations
        // even though they're not fully converted to AST nodes
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
