//
// Created by akovari on 6.1.25.
//

#pragma once

#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>

#include "ast.h"
#include "types.h"
#include "common.h"
#include "runtime.h"

namespace yona::typechecker {
using namespace std;
using namespace ast;
using namespace compiler::types;
using namespace interp::runtime;

// Type variable for inference
struct TypeVar {
    int id;
    optional<Type> bound_type;

    explicit TypeVar(int id) : id(id) {}
};

// Type environment for tracking variable types
class TypeEnvironment : public enable_shared_from_this<TypeEnvironment> {
private:
    unordered_map<string, Type> bindings;
    shared_ptr<TypeEnvironment> parent;

public:
    TypeEnvironment() : parent(nullptr) {}
    explicit TypeEnvironment(shared_ptr<TypeEnvironment> parent) : parent(parent) {}

    void bind(const string& name, const Type& type) {
        bindings[name] = type;
    }

    optional<Type> lookup(const string& name) const {
        auto it = bindings.find(name);
        if (it != bindings.end()) {
            return it->second;
        }
        if (parent) {
            return parent->lookup(name);
        }
        return nullopt;
    }

    shared_ptr<TypeEnvironment> extend() {
        return make_shared<TypeEnvironment>(shared_from_this());
    }
};

// Type substitution for unification
class TypeSubstitution {
private:
    unordered_map<int, Type> substitutions;

public:
    void bind(int var_id, const Type& type) {
        substitutions[var_id] = type;
    }

    Type apply(const Type& type) const;

    TypeSubstitution compose(const TypeSubstitution& other) const;
};

// Type inference context
class TypeInferenceContext {
private:
    int next_type_var = 0;
    vector<shared_ptr<yona_error>> errors;

public:
    shared_ptr<TypeVar> fresh_type_var() {
        return make_shared<TypeVar>(next_type_var++);
    }

    void add_error(const SourceLocation& loc, const string& message) {
        errors.push_back(make_shared<yona_error>(loc, yona_error::TYPE, message));
    }

    const vector<shared_ptr<yona_error>>& get_errors() const {
        return errors;
    }

    bool has_errors() const {
        return !errors.empty();
    }
};

// Type unification result
struct UnificationResult {
    bool success;
    TypeSubstitution substitution;
    optional<string> error_message;

    static UnificationResult ok(const TypeSubstitution& sub) {
        return {true, sub, nullopt};
    }

    static UnificationResult error(const string& msg) {
        return {false, TypeSubstitution{}, msg};
    }
};

// Main type checker class
class YONA_API TypeChecker : public AstVisitor<Type> {
private:
    mutable shared_ptr<TypeEnvironment> env;
    TypeInferenceContext& context;

    // Module type information
    mutable unordered_map<string, unordered_map<string, RecordTypeInfo>> module_records;
    mutable unordered_map<string, unordered_map<string, Type>> module_exports;

    // Unification algorithm
    UnificationResult unify(const Type& t1, const Type& t2) const;

    // Helper methods
    Type instantiate(const Type& type) const;
    Type generalize(const Type& type, const TypeEnvironment& env) const;
    Type infer_pattern_type(PatternNode* pattern, const Type& scrutinee_type) const;
    Type type_node_to_type(TypeNameNode* type_node) const;

public:
    TypeChecker(TypeInferenceContext& ctx, shared_ptr<TypeEnvironment> initial_env = nullptr)
        : context(ctx), env(initial_env ? initial_env : make_shared<TypeEnvironment>()) {}

    // Type checking entry point
    Type check(AstNode* node) const;

    // Import module type information
    void import_module_types(const string& module_name,
                           const unordered_map<string, RecordTypeInfo>& records,
                           const unordered_map<string, Type>& exports);

    // Visitor methods for type inference
    Type visit(ExprNode *node) const override;
    Type visit(IntegerExpr *node) const override;
    Type visit(FloatExpr *node) const override;
    Type visit(ByteExpr *node) const override;
    Type visit(CharacterExpr *node) const override;
    Type visit(StringExpr *node) const override;
    Type visit(TrueLiteralExpr *node) const override;
    Type visit(FalseLiteralExpr *node) const override;
    Type visit(UnitExpr *node) const override;
    Type visit(SymbolExpr *node) const override;
    Type visit(IdentifierExpr *node) const override;
    Type visit(TupleExpr *node) const override;
    Type visit(ValuesSequenceExpr *node) const override;
    Type visit(SetExpr *node) const override;
    Type visit(DictExpr *node) const override;
    Type visit(RecordInstanceExpr *node) const override;
    Type visit(FunctionExpr *node) const override;
    Type visit(ApplyExpr *node) const override;
    Type visit(LetExpr *node) const override;
    Type visit(IfExpr *node) const override;
    Type visit(CaseExpr *node) const override;
    Type visit(CaseClause *node) const override;
    Type visit(RaiseExpr *node) const override;
    Type visit(TryCatchExpr *node) const override;
    Type visit(WithExpr *node) const override;
    Type visit(DoExpr *node) const override;
    Type visit(ImportExpr *node) const override;
    Type visit(ModuleExpr *node) const override;

    // Binary operators
    Type visit(AddExpr *node) const override;
    Type visit(SubtractExpr *node) const override;
    Type visit(MultiplyExpr *node) const override;
    Type visit(DivideExpr *node) const override;
    Type visit(ModuloExpr *node) const override;
    Type visit(PowerExpr *node) const override;
    Type visit(EqExpr *node) const override;
    Type visit(NeqExpr *node) const override;
    Type visit(LtExpr *node) const override;
    Type visit(GtExpr *node) const override;
    Type visit(LteExpr *node) const override;
    Type visit(GteExpr *node) const override;
    Type visit(LogicalAndExpr *node) const override;
    Type visit(LogicalOrExpr *node) const override;
    Type visit(LogicalNotOpExpr *node) const override;


    // Missing visitor methods
    Type visit(DictGeneratorExpr *node) const override { return Type(nullptr); }
    Type visit(FieldAccessExpr *node) const override { return Type(nullptr); }
    Type visit(RecordNode *node) const override;
    Type visit(SeqGeneratorExpr *node) const override { return Type(nullptr); }
    Type visit(SetGeneratorExpr *node) const override { return Type(nullptr); }

    // Other visitor methods (can be left as default for now)
    Type visit(AstNode *node) const override { return dispatchVisit(node); }
    Type visit(PatternNode *node) const override { return dispatchVisit(node); }
    Type visit(UnderscoreNode *node) const override { return Type(nullptr); }
    Type visit(ValueExpr *node) const override { return dispatchVisit(node); }
    Type visit(SequenceExpr *node) const override { return dispatchVisit(node); }
    Type visit(ScopedNode *node) const override { return dispatchVisit(node); }
    Type visit(OpExpr *node) const override { return dispatchVisit(node); }
    Type visit(BinaryOpExpr *node) const override { return dispatchVisit(node); }
    Type visit(AliasExpr *node) const override { return dispatchVisit(node); }
    Type visit(CallExpr *node) const override { return dispatchVisit(node); }
    Type visit(ImportClauseExpr *node) const override { return Type(nullptr); }
    Type visit(GeneratorExpr *node) const override { return dispatchVisit(node); }
    Type visit(CollectionExtractorExpr *node) const override { return dispatchVisit(node); }
    Type visit(FunctionBody *node) const override { return dispatchVisit(node); }
    Type visit(NameExpr *node) const override { return Type(nullptr); }
    Type visit(FqnExpr *node) const override { return Type(nullptr); }
    Type visit(BodyWithGuards *node) const override { return Type(nullptr); }
    Type visit(BodyWithoutGuards *node) const override { return Type(nullptr); }
    Type visit(BitwiseAndExpr *node) const override { return Type(nullptr); }
    Type visit(BitwiseXorExpr *node) const override { return Type(nullptr); }
    Type visit(BitwiseOrExpr *node) const override { return Type(nullptr); }
    Type visit(BinaryNotOpExpr *node) const override { return Type(nullptr); }
    Type visit(ConsLeftExpr *node) const override { return Type(nullptr); }
    Type visit(ConsRightExpr *node) const override { return Type(nullptr); }
    Type visit(JoinExpr *node) const override { return Type(nullptr); }
    Type visit(LeftShiftExpr *node) const override { return Type(nullptr); }
    Type visit(RightShiftExpr *node) const override { return Type(nullptr); }
    Type visit(ZerofillRightShiftExpr *node) const override { return Type(nullptr); }
    Type visit(InExpr *node) const override { return Type(nullptr); }
    Type visit(PipeLeftExpr *node) const override { return Type(nullptr); }
    Type visit(PipeRightExpr *node) const override { return Type(nullptr); }
    Type visit(PatternExpr *node) const override { return Type(nullptr); }
    Type visit(PatternValue *node) const override { return Type(nullptr); }
    Type visit(AsDataStructurePattern *node) const override { return Type(nullptr); }
    Type visit(RecordPattern *node) const override { return Type(nullptr); }
    Type visit(OrPattern *node) const override { return Type(nullptr); }
    Type visit(TuplePattern *node) const override { return Type(nullptr); }
    Type visit(SeqPattern *node) const override { return Type(nullptr); }
    Type visit(HeadTailsPattern *node) const override { return Type(nullptr); }
    Type visit(TailsHeadPattern *node) const override { return Type(nullptr); }
    Type visit(HeadTailsHeadPattern *node) const override { return Type(nullptr); }
    Type visit(DictPattern *node) const override { return Type(nullptr); }
    Type visit(ValueAlias *node) const override;
    Type visit(FunctionAlias *node) const override { return Type(nullptr); }
    Type visit(ModuleImport *node) const override { return Type(nullptr); }
    Type visit(FunctionsImport *node) const override { return Type(nullptr); }
    Type visit(ValueCollectionExtractorExpr *node) const override { return Type(nullptr); }
    Type visit(KeyValueCollectionExtractorExpr *node) const override { return Type(nullptr); }
    Type visit(RangeSequenceExpr *node) const override { return Type(nullptr); }
    Type visit(FunctionDeclaration *node) const override { return Type(nullptr); }
    Type visit(NameCall *node) const override { return Type(nullptr); }
    Type visit(AliasCall *node) const override { return Type(nullptr); }
    Type visit(CatchPatternExpr *node) const override { return Type(nullptr); }
    Type visit(CatchExpr *node) const override { return Type(nullptr); }
    Type visit(FieldUpdateExpr *node) const override { return Type(nullptr); }
    Type visit(MainNode *node) const override { return Type(nullptr); }
    Type visit(TypeNameNode *node) const override { return Type(nullptr); }
    Type visit(BuiltinTypeNode *node) const override { return Type(nullptr); }
    Type visit(UserDefinedTypeNode *node) const override { return Type(nullptr); }
    Type visit(TypeDeclaration *node) const override { return Type(nullptr); }
    Type visit(TypeDefinition *node) const override { return Type(nullptr); }
    Type visit(TypeNode *node) const override { return Type(nullptr); }
    Type visit(TypeInstance *node) const override { return Type(nullptr); }
    Type visit(ModuleAlias *node) const override { return Type(nullptr); }
    Type visit(PatternAlias *node) const override;
    Type visit(FqnAlias *node) const override { return Type(nullptr); }
    Type visit(LambdaAlias *node) const override;
    Type visit(ModuleCall *node) const override { return Type(nullptr); }
    Type visit(ExprCall *node) const override { return Type(nullptr); }
    Type visit(PatternWithGuards *node) const override { return Type(nullptr); }
    Type visit(PatternWithoutGuards *node) const override { return Type(nullptr); }
    Type visit(DictGeneratorReducer *node) const override { return Type(nullptr); }
    Type visit(PackageNameExpr *node) const override { return Type(nullptr); }
};

} // namespace yona::typechecker
