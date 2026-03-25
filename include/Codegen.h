//
// LLVM Code Generation for Yona
//
// Generates LLVM IR from a type-checked AST. Since Yona uses Hindley-Milner
// type inference, all types are known at compile time and primitives are unboxed.
//
// Every expression produces a TypedValue — an LLVM Value paired with its
// codegen type tag. Types propagate structurally through expressions, enabling
// type-directed code generation without relying on the TypeChecker.
//

#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Target/TargetMachine.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>

#include "ast.h"
#include "types.h"

namespace yona::compiler::codegen {

using namespace yona::ast;

// Codegen type tag — tracks what kind of value an expression produces.
// Propagates through all expressions so the codegen always knows the
// correct LLVM type to use.
enum class CType { INT, FLOAT, BOOL, STRING, SEQ, TUPLE, UNIT, FUNCTION, SYMBOL, PROMISE, SET, DICT, ADT };

// A typed value: LLVM value + its codegen type + optional subtype info
struct TypedValue {
    llvm::Value* val = nullptr;
    CType type = CType::INT;
    std::vector<CType> subtypes; // tuple: element types; SEQ/SET: {elem_type}; DICT: {key_type, val_type}

    TypedValue() = default;
    TypedValue(llvm::Value* v, CType t) : val(v), type(t) {}
    TypedValue(llvm::Value* v, CType t, std::vector<CType> st)
        : val(v), type(t), subtypes(std::move(st)) {}

    explicit operator bool() const { return val != nullptr; }
};

// Deferred function — stored AST, compiled at call site with known arg types
struct DeferredFunction {
    FunctionExpr* ast;
    std::vector<std::string> param_names;
    std::vector<std::string> free_vars;
};

class Codegen {
public:
    Codegen(const std::string& module_name = "yona_module");
    ~Codegen();

    // Compile a single expression (wraps in main())
    llvm::Module* compile(AstNode* node);

    // Compile a module (exports functions with external linkage)
    llvm::Module* compile_module(ModuleExpr* module);

    bool emit_object_file(const std::string& output_path);
    bool emit_interface_file(const std::string& output_path);
    bool load_interface_file(const std::string& path);
    std::string emit_ir();

    // Module search paths for resolving imports
    std::vector<std::string> module_paths_;

    // Run LLVM optimization passes
    void optimize();

    // Mangle a module function name for export
    static std::string mangle_name(const std::string& module_fqn, const std::string& func_name);

private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    llvm::TargetMachine* target_machine_ = nullptr;

    // Scope: variable name → typed value
    std::unordered_map<std::string, TypedValue> named_values_;

    // Deferred functions: name → AST (compiled at call site)
    std::unordered_map<std::string, DeferredFunction> deferred_functions_;

    // Compiled function cache: name + arg types → LLVM function + return type
    struct CompiledFunction {
        llvm::Function* fn;
        CType return_type;
        std::vector<CType> param_types;
        std::vector<std::string> capture_names;
    };
    std::unordered_map<std::string, CompiledFunction> compiled_functions_;

    int lambda_counter_ = 0;
    std::string last_lambda_name_;

    // ADT constructor metadata
    struct AdtInfo {
        std::string type_name;
        int tag;
        int arity;
        int total_variants; // total number of constructors in this ADT
        int max_arity;      // maximum arity across all constructors
    };
    std::unordered_map<std::string, AdtInfo> adt_constructors_;

    // ADT struct type cache: type_name → struct type
    std::unordered_map<std::string, llvm::StructType*> adt_struct_types_;

    // External module function mapping: local name → mangled symbol name
    std::unordered_map<std::string, std::string> extern_functions_;

    // C FFI function info: symbol name → {return type, param types}
    struct CFFISignature {
        CType return_type;
        std::vector<CType> param_types;
    };
    std::unordered_map<std::string, CFFISignature> cffi_signatures_;

    // Register known C library function signatures
    void register_cffi_signatures();

    // Check if a module FQN is a C FFI import (starts with "C\")
    static bool is_cffi_import(const std::string& mod_fqn);

    // Runtime function declarations
    llvm::Function* rt_print_int_ = nullptr;
    llvm::Function* rt_print_float_ = nullptr;
    llvm::Function* rt_print_string_ = nullptr;
    llvm::Function* rt_print_bool_ = nullptr;
    llvm::Function* rt_print_newline_ = nullptr;
    llvm::Function* rt_print_seq_ = nullptr;
    llvm::Function* rt_string_concat_ = nullptr;
    llvm::Function* rt_seq_alloc_ = nullptr;
    llvm::Function* rt_seq_set_ = nullptr;
    llvm::Function* rt_seq_get_ = nullptr;
    llvm::Function* rt_seq_length_ = nullptr;
    llvm::Function* rt_seq_cons_ = nullptr;
    llvm::Function* rt_seq_join_ = nullptr;
    llvm::Function* rt_seq_head_ = nullptr;
    llvm::Function* rt_seq_tail_ = nullptr;
    llvm::Function* rt_print_symbol_ = nullptr;
    llvm::Function* rt_set_alloc_ = nullptr;
    llvm::Function* rt_set_put_ = nullptr;
    llvm::Function* rt_print_set_ = nullptr;
    llvm::Function* rt_dict_alloc_ = nullptr;
    llvm::Function* rt_dict_set_ = nullptr;
    llvm::Function* rt_print_dict_ = nullptr;

    // Symbol interning: name → i64 ID, ID → global string constant
    std::unordered_map<std::string, int64_t> symbol_ids_;
    std::vector<llvm::Constant*> symbol_strings_;  // ID → @"name" global string ptr

    // Intern a symbol name, returning its i64 ID
    int64_t intern_symbol(const std::string& name);
    llvm::Function* rt_async_call_ = nullptr;
    llvm::Function* rt_async_await_ = nullptr;
    llvm::Function* rt_closure2_create_ = nullptr;
    llvm::Function* rt_closure2_apply_ = nullptr;

    void init_target();
    void declare_runtime();

    // Get LLVM type for a CType
    llvm::Type* llvm_type(CType ct);

    // Top-level: wrap expression in main()
    llvm::Function* codegen_main(AstNode* node);

    // Core codegen — returns TypedValue
    TypedValue codegen(AstNode* node);

    // Literals
    TypedValue codegen_integer(IntegerExpr* node);
    TypedValue codegen_float(FloatExpr* node);
    TypedValue codegen_bool_true(TrueLiteralExpr* node);
    TypedValue codegen_bool_false(FalseLiteralExpr* node);
    TypedValue codegen_string(StringExpr* node);
    TypedValue codegen_unit(UnitExpr* node);
    TypedValue codegen_symbol(SymbolExpr* node);

    // Arithmetic (type-directed: int vs float dispatch)
    TypedValue codegen_binary(AstNode* left_node, AstNode* right_node,
                               const std::string& op);
    TypedValue codegen_comparison(AstNode* left_node, AstNode* right_node,
                                   const std::string& op);

    // Control flow
    TypedValue codegen_let(LetExpr* node);
    TypedValue codegen_if(IfExpr* node);
    TypedValue codegen_case(CaseExpr* node);
    TypedValue codegen_do(DoExpr* node);

    // Identifiers
    TypedValue codegen_identifier(IdentifierExpr* node);
    TypedValue codegen_main_node(MainNode* node);

    // Functions (deferred compilation)
    TypedValue codegen_function_def(FunctionExpr* node, const std::string& name);
    TypedValue codegen_apply(ApplyExpr* node);
    TypedValue codegen_lambda_alias(LambdaAlias* node);

    // Compile a deferred function with known argument types
    CompiledFunction compile_function(const std::string& name,
                                       const DeferredFunction& def,
                                       const std::vector<TypedValue>& args);

    // Imports and extern declarations
    TypedValue codegen_import(ImportExpr* node);
    TypedValue codegen_extern_decl(ExternDeclExpr* node);

    // Collections
    TypedValue codegen_tuple(TupleExpr* node);
    TypedValue codegen_seq(ValuesSequenceExpr* node);
    TypedValue codegen_set(SetExpr* node);
    TypedValue codegen_dict(DictExpr* node);
    TypedValue codegen_cons(ConsLeftExpr* node);
    TypedValue codegen_join(JoinExpr* node);

    // Auto-await: if a TypedValue is PROMISE, insert yona_rt_async_await
    TypedValue auto_await(TypedValue tv);

    // Free variable analysis
    static void collect_free_vars(AstNode* node,
                                   const std::unordered_set<std::string>& bound,
                                   std::unordered_set<std::string>& free_vars);

    // Infer parameter type from a function pattern node
    CType infer_type_from_pattern(PatternNode* pat);

    // Inferred parameter type with source pattern for struct layout
    struct InferredParamType {
        CType type = CType::INT;
        PatternNode* source_pattern = nullptr; // tuple/seq pattern for element types
    };

    // Infer parameter types for a module function by analyzing patterns and body
    std::vector<InferredParamType> infer_param_types(FunctionExpr* func);

    // Print a typed value (with newline) / value only (no newline)
    void codegen_print(const TypedValue& tv);
    void codegen_print_value(const TypedValue& tv);

public:
    // Module function type metadata — populated during compile_module,
    // can be queried by importers for type-safe cross-module linking.
    struct ModuleFunctionMeta {
        std::vector<CType> param_types;
        CType return_type;
    };
    std::unordered_map<std::string, ModuleFunctionMeta> module_meta_;
};

} // namespace yona::compiler::codegen
