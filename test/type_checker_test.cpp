#include <catch2/catch_test_macros.hpp>
#include "TypeChecker.h"
#include "Parser.h"
#include "ast.h"
#include "ast_visitor_impl.h"
#include <sstream>

using namespace yona;
using namespace yona::ast;
using namespace yona::parser;
using namespace yona::typechecker;
using namespace yona::compiler::types;
using namespace std;

struct TypeCheckerTest {
    Parser parser;

    unique_ptr<ExprNode> parse_expr(const string& code) {
        stringstream stream(code);
        auto result = parser.parse_input(stream);
        CHECK(result.success);

        // Extract expression from MainNode
        if (auto main = dynamic_cast<MainNode*>(result.node.get())) {
            // Transfer ownership from MainNode
            auto expr = main->node;
            main->node = nullptr; // Prevent double deletion
            return unique_ptr<ExprNode>(dynamic_cast<ExprNode*>(expr));
        }

        return nullptr;
    }

    Type check_expr(const string& code) {
        auto expr = parse_expr(code);
        CHECK(expr != nullptr);

        TypeInferenceContext ctx;
        TypeChecker checker(ctx);
        return checker.check(expr.get());
    }

    bool has_type_errors(const string& code) {
        auto expr = parse_expr(code);
        if (!expr) return true;

        TypeInferenceContext ctx;
        TypeChecker checker(ctx);
        checker.check(expr.get());
        return ctx.has_errors();
    }
};

// Tests for literal type inference
TEST_CASE("IntegerLiteralType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("42");

    // Debug output
    if (holds_alternative<nullptr_t>(t)) {
        INFO("Type is nullptr");
    } else if (holds_alternative<BuiltinType>(t)) {
        INFO("Type is BuiltinType");
    } else if (holds_alternative<shared_ptr<NamedType>>(t)) {
        INFO("Type is NamedType");
    } else if (holds_alternative<shared_ptr<FunctionType>>(t)) {
        INFO("Type is FunctionType");
    } else {
        INFO("Type is something else, index: " << t.index());
    }

    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::SignedInt64);
}

TEST_CASE("FloatLiteralType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("3.14");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Float64);
}

TEST_CASE("StringLiteralType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("\"hello\"");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::String);
}

TEST_CASE("BooleanLiteralTypes", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t1 = fixture.check_expr("true");
    CHECK(holds_alternative<BuiltinType>(t1));
    CHECK(get<BuiltinType>(t1) == compiler::types::Bool);

    Type t2 = fixture.check_expr("false");
    CHECK(holds_alternative<BuiltinType>(t2));
    CHECK(get<BuiltinType>(t2) == compiler::types::Bool);
}

TEST_CASE("CharacterLiteralType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("'a'");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Char);
}

TEST_CASE("UnitLiteralType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("()");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Unit);
}

TEST_CASE("SymbolLiteralType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr(":symbol");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Symbol);
}

// Tests for collection types
TEST_CASE("SequenceType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("[1, 2, 3]");
    CHECK(holds_alternative<shared_ptr<SingleItemCollectionType>>(t));
    auto seq_type = get<shared_ptr<SingleItemCollectionType>>(t);
    CHECK(seq_type->kind == SingleItemCollectionType::Seq);
    CHECK(holds_alternative<BuiltinType>(seq_type->valueType));
    CHECK(get<BuiltinType>(seq_type->valueType) == compiler::types::SignedInt64);
}

TEST_CASE("EmptySequenceType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("[]");
    CHECK(holds_alternative<shared_ptr<SingleItemCollectionType>>(t));
    auto seq_type = get<shared_ptr<SingleItemCollectionType>>(t);
    CHECK(seq_type->kind == SingleItemCollectionType::Seq);
    // Empty sequence has type variable
    CHECK(holds_alternative<shared_ptr<NamedType>>(seq_type->valueType));
}

TEST_CASE("SetType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("{1, 2, 3}");
    CHECK(holds_alternative<shared_ptr<SingleItemCollectionType>>(t));
    auto set_type = get<shared_ptr<SingleItemCollectionType>>(t);
    CHECK(set_type->kind == SingleItemCollectionType::Set);
    CHECK(holds_alternative<BuiltinType>(set_type->valueType));
    CHECK(get<BuiltinType>(set_type->valueType) == compiler::types::SignedInt64);
}

TEST_CASE("DictType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("{\"a\": 1, \"b\": 2}");
    CHECK(holds_alternative<shared_ptr<DictCollectionType>>(t));
    auto dict_type = get<shared_ptr<DictCollectionType>>(t);
    CHECK(holds_alternative<BuiltinType>(dict_type->keyType));
    CHECK(get<BuiltinType>(dict_type->keyType) == compiler::types::String);
    CHECK(holds_alternative<BuiltinType>(dict_type->valueType));
    CHECK(get<BuiltinType>(dict_type->valueType) == compiler::types::SignedInt64);
}

TEST_CASE("TupleType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("(1, \"hello\", true)");
    CHECK(holds_alternative<shared_ptr<ProductType>>(t));
    auto tuple_type = get<shared_ptr<ProductType>>(t);
    CHECK(tuple_type->types.size() == 3);
    CHECK(holds_alternative<BuiltinType>(tuple_type->types[0]));
    CHECK(get<BuiltinType>(tuple_type->types[0]) == compiler::types::SignedInt64);
    CHECK(holds_alternative<BuiltinType>(tuple_type->types[1]));
    CHECK(get<BuiltinType>(tuple_type->types[1]) == compiler::types::String);
    CHECK(holds_alternative<BuiltinType>(tuple_type->types[2]));
    CHECK(get<BuiltinType>(tuple_type->types[2]) == compiler::types::Bool);
}

// Tests for arithmetic operators
TEST_CASE("AdditionType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t1 = fixture.check_expr("1 + 2");
    CHECK(holds_alternative<BuiltinType>(t1));
    CHECK(get<BuiltinType>(t1) == compiler::types::SignedInt64);

    Type t2 = fixture.check_expr("1.5 + 2.5");
    CHECK(holds_alternative<BuiltinType>(t2));
    CHECK(get<BuiltinType>(t2) == compiler::types::Float64);

    Type t3 = fixture.check_expr("\"hello\" + \" world\"");
    CHECK(holds_alternative<BuiltinType>(t3));
    CHECK(get<BuiltinType>(t3) == compiler::types::String);
}

TEST_CASE("SubtractionType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("5 - 3");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::SignedInt64);
}

TEST_CASE("MultiplicationType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("3 * 4");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::SignedInt64);
}

TEST_CASE("DivisionType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("10 / 2");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Float64); // Division always returns float
}

TEST_CASE("ModuloType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("10 % 3");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::SignedInt64);
}

TEST_CASE("PowerType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("2 ** 3");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Float64); // Power always returns float
}

// Tests for comparison operators
TEST_CASE("EqualityType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("1 == 2");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Bool);
}

TEST_CASE("InequalityType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("1 != 2");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Bool);
}

TEST_CASE("ComparisonTypes", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t1 = fixture.check_expr("1 < 2");
    CHECK(holds_alternative<BuiltinType>(t1));
    CHECK(get<BuiltinType>(t1) == compiler::types::Bool);

    Type t2 = fixture.check_expr("1 > 2");
    CHECK(holds_alternative<BuiltinType>(t2));
    CHECK(get<BuiltinType>(t2) == compiler::types::Bool);

    Type t3 = fixture.check_expr("1 <= 2");
    CHECK(holds_alternative<BuiltinType>(t3));
    CHECK(get<BuiltinType>(t3) == compiler::types::Bool);

    Type t4 = fixture.check_expr("1 >= 2");
    CHECK(holds_alternative<BuiltinType>(t4));
    CHECK(get<BuiltinType>(t4) == compiler::types::Bool);
}

// Tests for logical operators
TEST_CASE("LogicalAndType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("true && false");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Bool);
}

TEST_CASE("LogicalOrType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("true || false");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Bool);
}

TEST_CASE("LogicalNotType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("!true");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::Bool);
}

// Tests for control flow
TEST_CASE("IfExpressionType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("if true then 1 else 2");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::SignedInt64);
}

TEST_CASE("LetExpressionType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("let x = 42 in x + 1");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::SignedInt64);
}

TEST_CASE("DoExpressionType", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    Type t = fixture.check_expr("do 1 2 3 end");
    CHECK(holds_alternative<BuiltinType>(t));
    CHECK(get<BuiltinType>(t) == compiler::types::SignedInt64); // Returns last expression
}

// Tests for type errors
TEST_CASE("TypeMismatchInArithmetic", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    CHECK(fixture.has_type_errors("1 + \"string\"")); // Can't add int and string (except for string concat)
    CHECK(fixture.has_type_errors("true - false")); // Can't subtract booleans
}

TEST_CASE("TypeMismatchInComparison", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    CHECK(fixture.has_type_errors("1 < \"string\"")); // Can't compare int and string
}

TEST_CASE("TypeMismatchInLogical", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    CHECK(fixture.has_type_errors("1 && 2")); // Logical operators require booleans
    CHECK(fixture.has_type_errors("!42")); // Not operator requires boolean
}

TEST_CASE("TypeMismatchInIf", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    CHECK(fixture.has_type_errors("if 42 then 1 else 2")); // Condition must be boolean
    CHECK(fixture.has_type_errors("if true then 1 else \"string\"")); // Branches must have same type
}

TEST_CASE("UndefinedVariable", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    CHECK(fixture.has_type_errors("undefined_var"));
}

TEST_CASE("TypeMismatchInSequence", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    CHECK(fixture.has_type_errors("[1, \"string\", 3]")); // Mixed types in sequence
}

TEST_CASE("TypeMismatchInSet", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    CHECK(fixture.has_type_errors("{1, \"string\", 3}")); // Mixed types in set
}

TEST_CASE("TypeMismatchInDict", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    CHECK(fixture.has_type_errors("{1: \"a\", \"b\": \"c\"}")); // Mixed key types
    CHECK(fixture.has_type_errors("{\"a\": 1, \"b\": \"c\"}")); // Mixed value types
}

// Tests for let polymorphism
TEST_CASE("LetPolymorphism", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    // Identity function should work with different types
    Type t = fixture.check_expr("let id = \\(x) -> x in (id(1), id(\"hello\"))");
    // This test would pass once lambda and function application are properly implemented
    // For now it will create type variables
}

// Tests for type unification
// Note: unify is now private, so these tests are commented out
/*
TEST_CASE("UnificationBasic", "[TypeCheckerTest]") {
    TypeCheckerTest fixture;
    TypeInferenceContext ctx;
    TypeChecker checker(ctx);

    // Same types unify
    auto result1 = checker.unify(Type(compiler::types::SignedInt64), Type(compiler::types::SignedInt64));
    CHECK(result1.success);

    // Different types don't unify
    auto result2 = checker.unify(Type(compiler::types::SignedInt64), Type(compiler::types::String));
    CHECK_FALSE(result2.success);
}

TEST_CASE("UnificationWithTypeVariables", "[TypeCheckerTest]") {
    TypeCheckerTest fixture;
    TypeInferenceContext ctx;
    TypeChecker checker(ctx);

    // Type variable unifies with concrete type
    auto var = make_shared<NamedType>();
    var->name = "0"; // Type variable
    var->type = nullptr;

    auto result = checker.unify(Type(var), Type(compiler::types::SignedInt64));
    CHECK(result.success);

    // Substitution should map variable to Int
    Type substituted = result.substitution.apply(Type(var));
    CHECK(holds_alternative<BuiltinType>(substituted));
    CHECK(get<BuiltinType>(substituted) == compiler::types::SignedInt64);
}
*/

// Tests for numeric type promotion
TEST_CASE("NumericPromotion", "[TypeCheckerTest]") /* FIXTURE */ {
    TypeCheckerTest fixture;
    // Test that derive_bin_op_result_type works correctly
    Type int_type(SignedInt64);
    Type float_type(Float64);

    Type result = derive_bin_op_result_type(int_type, float_type);
    CHECK(holds_alternative<BuiltinType>(result));
    CHECK(get<BuiltinType>(result) == Float64); // Float is "larger" than Int
}
