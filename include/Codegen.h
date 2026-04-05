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

#include <filesystem>
#include <llvm/IR/DIBuilder.h>
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
#include "Diagnostic.h"

namespace yona::compiler::codegen {

using namespace yona::ast;

// Codegen type tag — tracks what kind of value an expression produces.
// Propagates through all expressions so the codegen always knows the
// correct LLVM type to use.
enum class CType { INT, FLOAT, BOOL, STRING, SEQ, TUPLE, UNIT, FUNCTION, SYMBOL, PROMISE, SET, DICT, ADT, BYTES };

// A typed value: LLVM value + its codegen type + optional subtype info
struct TypedValue {
    llvm::Value* val = nullptr;
    CType type = CType::INT;
    std::vector<CType> subtypes; // tuple: element types; SEQ/SET: {elem_type}; DICT: {key_type, val_type}
    std::string adt_type_name;   // For CType::ADT: the ADT type name (e.g., "Option")
    bool is_io_promise = false;  // true: await via yona_rt_io_await, false: yona_rt_async_await

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
    Codegen(const std::string& module_name = "yona_module",
            compiler::DiagnosticEngine* diag = nullptr);
    ~Codegen();

    // Compile a single expression (wraps in main())
    llvm::Module* compile(AstNode* node);

    // Compile a module (exports functions with external linkage)
    llvm::Module* compile_module(ModuleDecl* module);

    bool emit_object_file(const std::string& output_path);
    bool emit_interface_file(const std::string& output_path);
    bool load_interface_file(const std::string& path);
    std::string emit_ir();

    // Enable DWARF debug info emission
    void set_debug_info(bool enabled, const std::string& filename = "");
    void set_opt_level(int level) { opt_level_ = (level < 0) ? 0 : (level > 3) ? 3 : level; }

    // Module search paths for resolving imports
    std::vector<std::string> module_paths_;

    // Run LLVM optimization passes
    void optimize();

    // Link runtime bitcode for cross-module inlining (LTO).
    // If the bitcode file exists, loads and merges it into the module
    // before optimization, enabling LLVM to inline runtime functions.
    bool link_runtime_bitcode(const std::string& bc_path);

    // Apply fastcc to internal functions whose address is never taken
    void apply_fastcc();

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
        bool is_io_async = false;  // true for AFN: call directly, await via yona_rt_io_await
        llvm::Value* closure_env = nullptr;  // for recursive closure calls: prepend this env arg
    };
    std::unordered_map<std::string, CompiledFunction> compiled_functions_;

    int lambda_counter_ = 0;
    std::string last_lambda_name_;

    // Closure devirtualization: map closure Value* → underlying Function*
    // When a known lambda is wrapped in a closure, we remember the mapping
    // so indirect closure calls can be replaced with direct calls.
    std::unordered_map<llvm::Value*, llvm::Function*> closure_known_fn_;

    // Escape analysis: variables whose values don't escape the current scope
    std::unordered_set<std::string> non_escaping_vars_;
    // Current arena pointer (nullptr if no arena active)
    llvm::Value* current_arena_ = nullptr;

    // Trait registry: trait name → info
    struct TraitInfo {
        std::string name;
        std::string type_param;
        std::vector<std::string> method_names;
        std::vector<std::pair<std::string, std::string>> superclasses;  // Phase 3: superclass constraints
        std::unordered_map<std::string, FunctionExpr*> default_impls;   // Phase 3: method → default impl AST
    };
    std::unordered_map<std::string, TraitInfo> trait_registry_;

    // Trait instance registry: "TraitName:TypeName" → instance info
    struct TraitInstanceInfo {
        std::string trait_name;
        std::string type_name;
        std::unordered_map<std::string, std::string> method_mangled_names; // method → mangled fn name
        std::vector<std::pair<std::string, std::string>> constraints;      // Phase 2: constraints
    };
    std::unordered_map<std::string, TraitInstanceInfo> trait_instances_;

    // Resolve a trait method for a concrete type, returns mangled function name (empty if not found)
    std::string resolve_trait_method(const std::string& method_name, CType arg_type,
                                     const std::string& adt_type_name = "");

    // Map CType to Yona type name
    static std::string ctype_to_type_name(CType ct);

    // ADT constructor metadata
    struct AdtInfo {
        std::string type_name;
        int tag;
        int arity;
        int total_variants;
        int max_arity;
        bool is_recursive = false;
        std::vector<std::string> field_names;  // named fields (empty for positional)
        std::vector<CType> field_types;         // CType of each field
    };
    std::unordered_map<std::string, AdtInfo> adt_constructors_;

    // ADT struct type cache: type_name → struct type
    std::unordered_map<std::string, llvm::StructType*> adt_struct_types_;

    // External module function mapping: local name → mangled symbol name
    std::unordered_map<std::string, std::string> extern_functions_;

    // Cross-module generic function source: mangled name → source text
    std::unordered_map<std::string, std::string> module_function_source_;

    // Imported generic function source from .yonai files
    struct ImportedFunctionSource {
        std::string source_text;
        std::string local_name;
    };
    std::unordered_map<std::string, ImportedFunctionSource> imported_function_sources_;

    // Ownership of re-parsed imported function AST nodes
    std::vector<std::unique_ptr<ast::FunctionExpr>> imported_ast_nodes_;

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
    llvm::Function* rt_seq_is_empty_ = nullptr;
    llvm::Function* rt_print_symbol_ = nullptr;
    llvm::Function* rt_set_alloc_ = nullptr;
    llvm::Function* rt_set_put_ = nullptr;
    llvm::Function* rt_print_set_ = nullptr;
    llvm::Function* rt_dict_alloc_ = nullptr;
    llvm::Function* rt_dict_set_ = nullptr;
    llvm::Function* rt_dict_put_ = nullptr;
    llvm::Function* rt_dict_get_ = nullptr;
    llvm::Function* rt_dict_size_ = nullptr;
    llvm::Function* rt_dict_contains_ = nullptr;
    llvm::Function* rt_dict_keys_ = nullptr;
    llvm::Function* rt_print_dict_ = nullptr;
    llvm::Function* rt_adt_alloc_ = nullptr;
    llvm::Function* rt_adt_get_tag_ = nullptr;
    llvm::Function* rt_adt_get_field_ = nullptr;
    llvm::Function* rt_adt_set_field_ = nullptr;
    llvm::Function* rt_adt_set_heap_mask_ = nullptr;
    llvm::Function* rt_async_call_thunk_ = nullptr;
    llvm::Function* rt_try_begin_ = nullptr;
    llvm::Function* rt_try_end_ = nullptr;
    llvm::Function* rt_raise_ = nullptr;
    llvm::Function* rt_get_exc_sym_ = nullptr;
    llvm::Function* rt_get_exc_msg_ = nullptr;

    // Symbol interning: name → i64 ID, ID → global string constant
    std::unordered_map<std::string, int64_t> symbol_ids_;
    std::vector<llvm::Constant*> symbol_strings_;  // ID → @"name" global string ptr

    // Intern a symbol name, returning its i64 ID
    int64_t intern_symbol(const std::string& name);
    llvm::Function* rt_async_call_ = nullptr;
    llvm::Function* rt_async_await_ = nullptr;
    llvm::Function* rt_closure2_create_ = nullptr;
    llvm::Function* rt_closure2_apply_ = nullptr;

    // General closures: {fn_ptr, cap0, cap1, ...} with env-passing convention
    llvm::Function* rt_closure_create_ = nullptr;
    llvm::Function* rt_closure_set_cap_ = nullptr;
    llvm::Function* rt_closure_get_cap_ = nullptr;
    llvm::Function* rt_closure_set_heap_mask_ = nullptr;
    llvm::Function* rt_tuple_alloc_ = nullptr;
    llvm::Function* rt_tuple_set_ = nullptr;
    llvm::Function* rt_tuple_set_heap_mask_ = nullptr;

    // Reference counting
    llvm::Function* rt_rc_inc_ = nullptr;
    llvm::Function* rt_rc_dec_ = nullptr;

    // io_uring await
    llvm::Function* rt_io_await_ = nullptr;

    // Bytes
    llvm::Function* rt_bytes_alloc_ = nullptr;
    llvm::Function* rt_bytes_length_ = nullptr;
    llvm::Function* rt_bytes_get_ = nullptr;
    llvm::Function* rt_bytes_set_ = nullptr;
    llvm::Function* rt_bytes_concat_ = nullptr;
    llvm::Function* rt_bytes_slice_ = nullptr;
    llvm::Function* rt_bytes_from_string_ = nullptr;
    llvm::Function* rt_bytes_to_string_ = nullptr;
    llvm::Function* rt_bytes_from_seq_ = nullptr;
    llvm::Function* rt_bytes_to_seq_ = nullptr;
    llvm::Function* rt_print_bytes_ = nullptr;

    // Boxing (tuples in collections)
    llvm::Function* rt_box_ = nullptr;

    // Resource cleanup (with expression)
    llvm::Function* rt_close_ = nullptr;

    // Arena allocator
    llvm::Function* rt_arena_create_ = nullptr;
    llvm::Function* rt_arena_alloc_ = nullptr;
    llvm::Function* rt_arena_destroy_ = nullptr;

    // DWARF debug info
    std::unique_ptr<llvm::DIBuilder> di_builder_;
    llvm::DICompileUnit* di_cu_ = nullptr;
    llvm::DIFile* di_file_ = nullptr;
    llvm::DIScope* di_scope_ = nullptr;  // current scope (function or lexical block)
    bool debug_info_ = false;
    int opt_level_ = 2; // 0=O0, 1=O1, 2=O2 (default), 3=O3

    // Debug info helpers
    void init_debug_info(const std::string& filename);
    void finalize_debug_info();
    void set_debug_loc(const SourceLocation& loc);
    llvm::DIType* di_type_for(CType ct);
    llvm::DISubroutineType* di_func_type(const std::vector<CType>& param_types, CType ret_type);

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
    TypedValue codegen_raise(RaiseExpr* node);
    TypedValue codegen_try_catch(TryCatchExpr* node);
    TypedValue codegen_with(WithExpr* node);

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
    std::pair<std::string, std::filesystem::path> build_fqn_path(FqnExpr* fqn);
    void load_module_interface(const std::filesystem::path& mod_path);
    void register_import(const std::string& mod_fqn, const std::string& func_name, const std::string& import_name);
    void register_all_imports(const std::string& mod_fqn);
    void register_trait_externs();
    llvm::Type* adt_llvm_type(const std::string& type_name);
    std::unique_ptr<ast::ModuleDecl> reparse_genfn(const std::string& local_name, const std::string& source_text);

    // Collections
    TypedValue codegen_tuple(TupleExpr* node);
    TypedValue codegen_seq(ValuesSequenceExpr* node);
    TypedValue codegen_set(SetExpr* node);
    TypedValue codegen_dict(DictExpr* node);
    TypedValue codegen_cons(ConsLeftExpr* node);
    TypedValue codegen_join(JoinExpr* node);

    // Generators / comprehensions
    TypedValue codegen_seq_generator(SeqGeneratorExpr* node);
    TypedValue codegen_set_generator(SetGeneratorExpr* node);
    TypedValue codegen_dict_generator(DictGeneratorExpr* node);

    // Auto-await: if a TypedValue is PROMISE, insert yona_rt_async_await
    TypedValue auto_await(TypedValue tv);

    // Wrap a Function* in a closure for uniform calling convention.
    // Generates an env-passing wrapper and creates a trivial closure {wrapper, ret_tag}.
    llvm::Value* wrap_in_closure(llvm::Function* fn, CType ret_type);

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

    // Reference counting helpers — emit rc_inc/rc_dec for heap-typed values
    static bool is_heap_type(CType ct);
    void emit_rc_inc(llvm::Value* val, CType type);
    void emit_rc_dec(llvm::Value* val, CType type);

    std::pair<llvm::Type*, CType> infer_return_type(ast::AstNode* body_expr);
    llvm::Value* emit_arena_alloc(int64_t type_tag, llvm::Value* payload_bytes);

    // Print a typed value (with newline) / value only (no newline)
    void codegen_print(const TypedValue& tv);
    void codegen_print_value(const TypedValue& tv);

    // Compile error reporting with source location
    void report_error(const SourceLocation& loc, const std::string& message);

    // "Did you mean?" suggestion for undefined names
    std::string suggest_similar(const std::string& name) const;

    compiler::DiagnosticEngine* diag_ = nullptr;        // external, not owned
    std::unique_ptr<compiler::DiagnosticEngine> owned_diag_; // fallback if none provided

public:
    int error_count_ = 0;
    // Module function type metadata — populated during compile_module,
    // can be queried by importers for type-safe cross-module linking.
    struct ModuleFunctionMeta {
        std::vector<CType> param_types;
        CType return_type;
        bool is_async = false;       // AFN: thread-pool async
        bool is_io_async = false;    // IO: io_uring submit-and-return
        CType async_inner_type = CType::INT;
    };
    std::unordered_map<std::string, ModuleFunctionMeta> module_meta_;
};

} // namespace yona::compiler::codegen
