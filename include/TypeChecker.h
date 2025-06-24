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
class TypeChecker : public AstVisitor {
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
    any visit(ExprNode *node) const override;
    any visit(IntegerExpr *node) const override;
    any visit(FloatExpr *node) const override;
    any visit(ByteExpr *node) const override;
    any visit(CharacterExpr *node) const override;
    any visit(StringExpr *node) const override;
    any visit(TrueLiteralExpr *node) const override;
    any visit(FalseLiteralExpr *node) const override;
    any visit(UnitExpr *node) const override;
    any visit(SymbolExpr *node) const override;
    any visit(IdentifierExpr *node) const override;
    any visit(TupleExpr *node) const override;
    any visit(ValuesSequenceExpr *node) const override;
    any visit(SetExpr *node) const override;
    any visit(DictExpr *node) const override;
    any visit(RecordInstanceExpr *node) const override;
    any visit(FunctionExpr *node) const override;
    any visit(ApplyExpr *node) const override;
    any visit(LetExpr *node) const override;
    any visit(IfExpr *node) const override;
    any visit(CaseExpr *node) const override;
    any visit(CaseClause *node) const override;
    any visit(RaiseExpr *node) const override;
    any visit(TryCatchExpr *node) const override;
    any visit(WithExpr *node) const override;
    any visit(DoExpr *node) const override;
    any visit(ImportExpr *node) const override;
    any visit(ModuleExpr *node) const override;
    
    // Binary operators
    any visit(AddExpr *node) const override;
    any visit(SubtractExpr *node) const override;
    any visit(MultiplyExpr *node) const override;
    any visit(DivideExpr *node) const override;
    any visit(ModuloExpr *node) const override;
    any visit(PowerExpr *node) const override;
    any visit(EqExpr *node) const override;
    any visit(NeqExpr *node) const override;
    any visit(LtExpr *node) const override;
    any visit(GtExpr *node) const override;
    any visit(LteExpr *node) const override;
    any visit(GteExpr *node) const override;
    any visit(LogicalAndExpr *node) const override;
    any visit(LogicalOrExpr *node) const override;
    any visit(LogicalNotOpExpr *node) const override;
    
    
    // Missing visitor methods
    any visit(DictGeneratorExpr *node) const override { return Type(nullptr); }
    any visit(FieldAccessExpr *node) const override { return Type(nullptr); }
    any visit(RecordNode *node) const override { return Type(nullptr); }
    any visit(SeqGeneratorExpr *node) const override { return Type(nullptr); }
    any visit(SetGeneratorExpr *node) const override { return Type(nullptr); }
    
    // Other visitor methods (can be left as default for now)
    any visit(AstNode *node) const override { return Type(nullptr); }
    any visit(PatternNode *node) const override { return Type(nullptr); }
    any visit(UnderscoreNode *node) const override { return Type(nullptr); }
    any visit(ValueExpr *node) const override { return Type(nullptr); }
    any visit(ScopedNode *node) const override { return Type(nullptr); }
    any visit(OpExpr *node) const override { return Type(nullptr); }
    any visit(BinaryOpExpr *node) const override { return Type(nullptr); }
    any visit(AliasExpr *node) const override { return Type(nullptr); }
    any visit(CallExpr *node) const override { return Type(nullptr); }
    any visit(ImportClauseExpr *node) const override { return Type(nullptr); }
    any visit(GeneratorExpr *node) const override { return Type(nullptr); }
    any visit(CollectionExtractorExpr *node) const override { return Type(nullptr); }
    any visit(FunctionBody *node) const override { return Type(nullptr); }
    any visit(NameExpr *node) const override { return Type(nullptr); }
    any visit(FqnExpr *node) const override { return Type(nullptr); }
    any visit(BodyWithGuards *node) const override { return Type(nullptr); }
    any visit(BodyWithoutGuards *node) const override { return Type(nullptr); }
    any visit(BitwiseAndExpr *node) const override { return Type(nullptr); }
    any visit(BitwiseXorExpr *node) const override { return Type(nullptr); }
    any visit(BitwiseOrExpr *node) const override { return Type(nullptr); }
    any visit(BinaryNotOpExpr *node) const override { return Type(nullptr); }
    any visit(ConsLeftExpr *node) const override { return Type(nullptr); }
    any visit(ConsRightExpr *node) const override { return Type(nullptr); }
    any visit(JoinExpr *node) const override { return Type(nullptr); }
    any visit(LeftShiftExpr *node) const override { return Type(nullptr); }
    any visit(RightShiftExpr *node) const override { return Type(nullptr); }
    any visit(ZerofillRightShiftExpr *node) const override { return Type(nullptr); }
    any visit(InExpr *node) const override { return Type(nullptr); }
    any visit(PipeLeftExpr *node) const override { return Type(nullptr); }
    any visit(PipeRightExpr *node) const override { return Type(nullptr); }
    any visit(PatternExpr *node) const override { return Type(nullptr); }
    any visit(PatternValue *node) const override { return Type(nullptr); }
    any visit(AsDataStructurePattern *node) const override { return Type(nullptr); }
    any visit(RecordPattern *node) const override { return Type(nullptr); }
    any visit(TuplePattern *node) const override { return Type(nullptr); }
    any visit(SeqPattern *node) const override { return Type(nullptr); }
    any visit(HeadTailsPattern *node) const override { return Type(nullptr); }
    any visit(TailsHeadPattern *node) const override { return Type(nullptr); }
    any visit(HeadTailsHeadPattern *node) const override { return Type(nullptr); }
    any visit(DictPattern *node) const override { return Type(nullptr); }
    any visit(ValueAlias *node) const override;
    any visit(FunctionAlias *node) const override { return Type(nullptr); }
    any visit(ModuleImport *node) const override { return Type(nullptr); }
    any visit(FunctionsImport *node) const override { return Type(nullptr); }
    any visit(ValueCollectionExtractorExpr *node) const override { return Type(nullptr); }
    any visit(KeyValueCollectionExtractorExpr *node) const override { return Type(nullptr); }
    any visit(RangeSequenceExpr *node) const override { return Type(nullptr); }
    any visit(FunctionDeclaration *node) const override { return Type(nullptr); }
    any visit(NameCall *node) const override { return Type(nullptr); }
    any visit(AliasCall *node) const override { return Type(nullptr); }
    any visit(CatchPatternExpr *node) const override { return Type(nullptr); }
    any visit(CatchExpr *node) const override { return Type(nullptr); }
    any visit(FieldUpdateExpr *node) const override { return Type(nullptr); }
    any visit(MainNode *node) const override { return Type(nullptr); }
    any visit(TypeNameNode *node) const override { return Type(nullptr); }
    any visit(BuiltinTypeNode *node) const override { return Type(nullptr); }
    any visit(UserDefinedTypeNode *node) const override { return Type(nullptr); }
    any visit(TypeDeclaration *node) const override { return Type(nullptr); }
    any visit(TypeDefinition *node) const override { return Type(nullptr); }
    any visit(TypeNode *node) const override { return Type(nullptr); }
    any visit(TypeInstance *node) const override { return Type(nullptr); }
    any visit(ModuleAlias *node) const override { return Type(nullptr); }
    any visit(PatternAlias *node) const override;
    any visit(FqnAlias *node) const override { return Type(nullptr); }
    any visit(LambdaAlias *node) const override;
    any visit(ModuleCall *node) const override { return Type(nullptr); }
    any visit(ExprCall *node) const override { return Type(nullptr); }
    any visit(PatternWithGuards *node) const override { return Type(nullptr); }
    any visit(PatternWithoutGuards *node) const override { return Type(nullptr); }
    any visit(DictGeneratorReducer *node) const override { return Type(nullptr); }
    any visit(PackageNameExpr *node) const override { return Type(nullptr); }
};

} // namespace yona::typechecker