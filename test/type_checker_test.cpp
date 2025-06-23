#include <gtest/gtest.h>
#include "TypeChecker.h"
#include "Parser.h"
#include "ast.h"
#include <sstream>

using namespace yona;
using namespace yona::ast;
using namespace yona::parser;
using namespace yona::typechecker;
using namespace yona::compiler::types;
using namespace std;

class TypeCheckerTest : public ::testing::Test {
protected:
    Parser parser;
    
    unique_ptr<ExprNode> parse_expr(const string& code) {
        stringstream stream(code);
        auto result = parser.parse_input(stream);
        EXPECT_TRUE(result.success);
        
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
        EXPECT_NE(expr, nullptr);
        
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
TEST_F(TypeCheckerTest, IntegerLiteralType) {
    Type t = check_expr("42");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, FloatLiteralType) {
    Type t = check_expr("3.14");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Float64);
}

TEST_F(TypeCheckerTest, StringLiteralType) {
    Type t = check_expr("\"hello\"");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::String);
}

TEST_F(TypeCheckerTest, BooleanLiteralTypes) {
    Type t1 = check_expr("true");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t1));
    EXPECT_EQ(get<BuiltinType>(t1), compiler::types::Bool);
    
    Type t2 = check_expr("false");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t2));
    EXPECT_EQ(get<BuiltinType>(t2), compiler::types::Bool);
}

TEST_F(TypeCheckerTest, CharacterLiteralType) {
    Type t = check_expr("'a'");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Char);
}

TEST_F(TypeCheckerTest, UnitLiteralType) {
    Type t = check_expr("()");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Unit);
}

TEST_F(TypeCheckerTest, SymbolLiteralType) {
    Type t = check_expr(":symbol");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Symbol);
}

// Tests for collection types
TEST_F(TypeCheckerTest, SequenceType) {
    Type t = check_expr("[1, 2, 3]");
    EXPECT_TRUE(holds_alternative<shared_ptr<SingleItemCollectionType>>(t));
    auto seq_type = get<shared_ptr<SingleItemCollectionType>>(t);
    EXPECT_EQ(seq_type->kind, SingleItemCollectionType::Seq);
    EXPECT_TRUE(holds_alternative<BuiltinType>(seq_type->valueType));
    EXPECT_EQ(get<BuiltinType>(seq_type->valueType), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, EmptySequenceType) {
    Type t = check_expr("[]");
    EXPECT_TRUE(holds_alternative<shared_ptr<SingleItemCollectionType>>(t));
    auto seq_type = get<shared_ptr<SingleItemCollectionType>>(t);
    EXPECT_EQ(seq_type->kind, SingleItemCollectionType::Seq);
    // Empty sequence has type variable
    EXPECT_TRUE(holds_alternative<shared_ptr<NamedType>>(seq_type->valueType));
}

TEST_F(TypeCheckerTest, SetType) {
    Type t = check_expr("{1, 2, 3}");
    EXPECT_TRUE(holds_alternative<shared_ptr<SingleItemCollectionType>>(t));
    auto set_type = get<shared_ptr<SingleItemCollectionType>>(t);
    EXPECT_EQ(set_type->kind, SingleItemCollectionType::Set);
    EXPECT_TRUE(holds_alternative<BuiltinType>(set_type->valueType));
    EXPECT_EQ(get<BuiltinType>(set_type->valueType), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, DictType) {
    Type t = check_expr("{\"a\": 1, \"b\": 2}");
    EXPECT_TRUE(holds_alternative<shared_ptr<DictCollectionType>>(t));
    auto dict_type = get<shared_ptr<DictCollectionType>>(t);
    EXPECT_TRUE(holds_alternative<BuiltinType>(dict_type->keyType));
    EXPECT_EQ(get<BuiltinType>(dict_type->keyType), compiler::types::String);
    EXPECT_TRUE(holds_alternative<BuiltinType>(dict_type->valueType));
    EXPECT_EQ(get<BuiltinType>(dict_type->valueType), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, TupleType) {
    Type t = check_expr("(1, \"hello\", true)");
    EXPECT_TRUE(holds_alternative<shared_ptr<ProductType>>(t));
    auto tuple_type = get<shared_ptr<ProductType>>(t);
    EXPECT_EQ(tuple_type->types.size(), 3);
    EXPECT_TRUE(holds_alternative<BuiltinType>(tuple_type->types[0]));
    EXPECT_EQ(get<BuiltinType>(tuple_type->types[0]), compiler::types::SignedInt64);
    EXPECT_TRUE(holds_alternative<BuiltinType>(tuple_type->types[1]));
    EXPECT_EQ(get<BuiltinType>(tuple_type->types[1]), compiler::types::String);
    EXPECT_TRUE(holds_alternative<BuiltinType>(tuple_type->types[2]));
    EXPECT_EQ(get<BuiltinType>(tuple_type->types[2]), compiler::types::Bool);
}

// Tests for arithmetic operators
TEST_F(TypeCheckerTest, AdditionType) {
    Type t1 = check_expr("1 + 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t1));
    EXPECT_EQ(get<BuiltinType>(t1), compiler::types::SignedInt64);
    
    Type t2 = check_expr("1.5 + 2.5");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t2));
    EXPECT_EQ(get<BuiltinType>(t2), compiler::types::Float64);
    
    Type t3 = check_expr("\"hello\" + \" world\"");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t3));
    EXPECT_EQ(get<BuiltinType>(t3), compiler::types::String);
}

TEST_F(TypeCheckerTest, SubtractionType) {
    Type t = check_expr("5 - 3");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, MultiplicationType) {
    Type t = check_expr("3 * 4");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, DivisionType) {
    Type t = check_expr("10 / 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Float64); // Division always returns float
}

TEST_F(TypeCheckerTest, ModuloType) {
    Type t = check_expr("10 % 3");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, PowerType) {
    Type t = check_expr("2 ** 3");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Float64); // Power always returns float
}

// Tests for comparison operators
TEST_F(TypeCheckerTest, EqualityType) {
    Type t = check_expr("1 == 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Bool);
}

TEST_F(TypeCheckerTest, InequalityType) {
    Type t = check_expr("1 != 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Bool);
}

TEST_F(TypeCheckerTest, ComparisonTypes) {
    Type t1 = check_expr("1 < 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t1));
    EXPECT_EQ(get<BuiltinType>(t1), compiler::types::Bool);
    
    Type t2 = check_expr("1 > 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t2));
    EXPECT_EQ(get<BuiltinType>(t2), compiler::types::Bool);
    
    Type t3 = check_expr("1 <= 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t3));
    EXPECT_EQ(get<BuiltinType>(t3), compiler::types::Bool);
    
    Type t4 = check_expr("1 >= 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t4));
    EXPECT_EQ(get<BuiltinType>(t4), compiler::types::Bool);
}

// Tests for logical operators
TEST_F(TypeCheckerTest, LogicalAndType) {
    Type t = check_expr("true && false");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Bool);
}

TEST_F(TypeCheckerTest, LogicalOrType) {
    Type t = check_expr("true || false");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Bool);
}

TEST_F(TypeCheckerTest, LogicalNotType) {
    Type t = check_expr("!true");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::Bool);
}

// Tests for control flow
TEST_F(TypeCheckerTest, IfExpressionType) {
    Type t = check_expr("if true then 1 else 2");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, LetExpressionType) {
    Type t = check_expr("let x = 42 in x + 1");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::SignedInt64);
}

TEST_F(TypeCheckerTest, DoExpressionType) {
    Type t = check_expr("do 1 2 3 end");
    EXPECT_TRUE(holds_alternative<BuiltinType>(t));
    EXPECT_EQ(get<BuiltinType>(t), compiler::types::SignedInt64); // Returns last expression
}

// Tests for type errors
TEST_F(TypeCheckerTest, TypeMismatchInArithmetic) {
    EXPECT_TRUE(has_type_errors("1 + \"string\"")); // Can't add int and string (except for string concat)
    EXPECT_TRUE(has_type_errors("true - false")); // Can't subtract booleans
}

TEST_F(TypeCheckerTest, TypeMismatchInComparison) {
    EXPECT_TRUE(has_type_errors("1 < \"string\"")); // Can't compare int and string
}

TEST_F(TypeCheckerTest, TypeMismatchInLogical) {
    EXPECT_TRUE(has_type_errors("1 && 2")); // Logical operators require booleans
    EXPECT_TRUE(has_type_errors("!42")); // Not operator requires boolean
}

TEST_F(TypeCheckerTest, TypeMismatchInIf) {
    EXPECT_TRUE(has_type_errors("if 42 then 1 else 2")); // Condition must be boolean
    EXPECT_TRUE(has_type_errors("if true then 1 else \"string\"")); // Branches must have same type
}

TEST_F(TypeCheckerTest, UndefinedVariable) {
    EXPECT_TRUE(has_type_errors("undefined_var"));
}

TEST_F(TypeCheckerTest, TypeMismatchInSequence) {
    EXPECT_TRUE(has_type_errors("[1, \"string\", 3]")); // Mixed types in sequence
}

TEST_F(TypeCheckerTest, TypeMismatchInSet) {
    EXPECT_TRUE(has_type_errors("{1, \"string\", 3}")); // Mixed types in set
}

TEST_F(TypeCheckerTest, TypeMismatchInDict) {
    EXPECT_TRUE(has_type_errors("{1: \"a\", \"b\": \"c\"}")); // Mixed key types
    EXPECT_TRUE(has_type_errors("{\"a\": 1, \"b\": \"c\"}")); // Mixed value types
}

// Tests for let polymorphism
TEST_F(TypeCheckerTest, LetPolymorphism) {
    // Identity function should work with different types
    Type t = check_expr("let id = \\(x) -> x in (id(1), id(\"hello\"))");
    // This test would pass once lambda and function application are properly implemented
    // For now it will create type variables
}

// Tests for type unification
// Note: unify is now private, so these tests are commented out
/*
TEST_F(TypeCheckerTest, UnificationBasic) {
    TypeInferenceContext ctx;
    TypeChecker checker(ctx);
    
    // Same types unify
    auto result1 = checker.unify(Type(compiler::types::SignedInt64), Type(compiler::types::SignedInt64));
    EXPECT_TRUE(result1.success);
    
    // Different types don't unify
    auto result2 = checker.unify(Type(compiler::types::SignedInt64), Type(compiler::types::String));
    EXPECT_FALSE(result2.success);
}

TEST_F(TypeCheckerTest, UnificationWithTypeVariables) {
    TypeInferenceContext ctx;
    TypeChecker checker(ctx);
    
    // Type variable unifies with concrete type
    auto var = make_shared<NamedType>();
    var->name = "0"; // Type variable
    var->type = nullptr;
    
    auto result = checker.unify(Type(var), Type(compiler::types::SignedInt64));
    EXPECT_TRUE(result.success);
    
    // Substitution should map variable to Int
    Type substituted = result.substitution.apply(Type(var));
    EXPECT_TRUE(holds_alternative<BuiltinType>(substituted));
    EXPECT_EQ(get<BuiltinType>(substituted), compiler::types::SignedInt64);
}
*/

// Tests for numeric type promotion
TEST_F(TypeCheckerTest, NumericPromotion) {
    // Test that derive_bin_op_result_type works correctly
    Type int_type(SignedInt64);
    Type float_type(Float64);
    
    Type result = derive_bin_op_result_type(int_type, float_type);
    EXPECT_TRUE(holds_alternative<BuiltinType>(result));
    EXPECT_EQ(get<BuiltinType>(result), Float64); // Float is "larger" than Int
}