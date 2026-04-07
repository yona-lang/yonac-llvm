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

    // Stream fusion: deferred single-use generator bindings.
    // When a let-bound seq generator has exactly one use, we skip its codegen
    // and fuse it into the consuming generator at codegen time.
    std::unordered_map<std::string, ast::SeqGeneratorExpr*> deferred_generators_;

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

    // ===== Type Registry — ADTs, traits, CFFI =====
    struct TraitInfo {
        std::string name, type_param;
        std::vector<std::string> method_names;
        std::vector<std::pair<std::string, std::string>> superclasses;
        std::unordered_map<std::string, FunctionExpr*> default_impls;
    };
    struct TraitInstanceInfo {
        std::string trait_name, type_name;
        std::unordered_map<std::string, std::string> method_mangled_names;
        std::vector<std::pair<std::string, std::string>> constraints;
    };
    struct AdtInfo {
        std::string type_name;
        int tag, arity, total_variants, max_arity;
        bool is_recursive = false;
        std::vector<std::string> field_names;
        std::vector<CType> field_types;
    };
    struct CFFISignature {
        CType return_type;
        std::vector<CType> param_types;
    };
    struct ImportedFunctionSource {
        std::string source_text, local_name;
    };
    struct ModuleFunctionMeta {
        std::vector<CType> param_types;
        CType return_type;
        bool is_async = false;
        bool is_io_async = false;
        CType async_inner_type = CType::INT;
    };

    struct TypeRegistry {
        std::unordered_map<std::string, TraitInfo> traits;
        std::unordered_map<std::string, TraitInstanceInfo> trait_instances;
        std::unordered_map<std::string, AdtInfo> adt_constructors;
        std::unordered_map<std::string, llvm::StructType*> adt_struct_types;
        std::unordered_map<std::string, CFFISignature> cffi_signatures;
    } types_;

    // ===== Module System — imports, exports, cross-module =====
    struct ImportState {
        std::unordered_map<std::string, std::string> extern_functions;       // local → mangled
        std::unordered_map<std::string, std::string> function_source;        // mangled → source
        std::unordered_map<std::string, ImportedFunctionSource> imported_sources;
        std::vector<std::unique_ptr<ast::FunctionExpr>> imported_ast_nodes;  // ownership
        std::unordered_map<std::string, ModuleFunctionMeta> meta;            // function metadata
    } imports_;

    // ===== Symbol Interning =====
    struct SymbolTable {
        std::unordered_map<std::string, int64_t> ids;
        std::vector<llvm::Constant*> strings;
    } symbols_;

    // ===== Debug Info =====
    struct DebugState {
        std::unique_ptr<llvm::DIBuilder> builder;
        llvm::DICompileUnit* cu = nullptr;
        llvm::DIFile* file = nullptr;
        llvm::DIScope* scope = nullptr;
        bool enabled = false;
    } debug_;

    std::string resolve_trait_method(const std::string& method_name, CType arg_type,
                                     const std::string& adt_type_name = "");
    static std::string ctype_to_type_name(CType ct);
    void register_cffi_signatures();
    static bool is_cffi_import(const std::string& mod_fqn);

    // All runtime function declarations grouped in a struct
    struct RuntimeDecls {
        // Printing
        llvm::Function *print_int_ = nullptr, *print_float_ = nullptr,
            *print_string_ = nullptr, *print_bool_ = nullptr, *print_newline_ = nullptr,
            *print_seq_ = nullptr, *print_symbol_ = nullptr, *print_set_ = nullptr,
            *print_dict_ = nullptr, *print_bytes_ = nullptr;
        // Strings
        llvm::Function* string_concat_ = nullptr;
        // Sequences
        llvm::Function *seq_alloc_ = nullptr, *seq_set_ = nullptr, *seq_get_ = nullptr,
            *seq_length_ = nullptr, *seq_cons_ = nullptr, *seq_join_ = nullptr,
            *seq_head_ = nullptr, *seq_tail_ = nullptr, *seq_is_empty_ = nullptr;
        // Sets
        llvm::Function *set_alloc_ = nullptr, *set_put_ = nullptr, *set_insert_ = nullptr,
            *set_contains_ = nullptr, *set_size_ = nullptr, *set_elements_ = nullptr,
            *set_union_ = nullptr, *set_intersection_ = nullptr, *set_difference_ = nullptr;
        // Dicts
        llvm::Function *dict_alloc_ = nullptr, *dict_set_ = nullptr, *dict_put_ = nullptr,
            *dict_get_ = nullptr, *dict_size_ = nullptr, *dict_contains_ = nullptr,
            *dict_keys_ = nullptr;
        // ADTs
        llvm::Function *adt_alloc_ = nullptr, *adt_get_tag_ = nullptr,
            *adt_get_field_ = nullptr, *adt_set_field_ = nullptr, *adt_set_heap_mask_ = nullptr;
        // Async
        llvm::Function *async_call_ = nullptr, *async_call_thunk_ = nullptr,
            *async_await_ = nullptr, *io_await_ = nullptr;
        // Exceptions
        llvm::Function *try_begin_ = nullptr, *try_end_ = nullptr, *raise_ = nullptr,
            *get_exc_sym_ = nullptr, *get_exc_msg_ = nullptr;
        // Closures
        llvm::Function *closure_create_ = nullptr, *closure_set_cap_ = nullptr,
            *closure_get_cap_ = nullptr, *closure_set_heap_mask_ = nullptr,
            *closure2_create_ = nullptr, *closure2_apply_ = nullptr;
        // Tuples
        llvm::Function *tuple_alloc_ = nullptr, *tuple_set_ = nullptr,
            *tuple_set_heap_mask_ = nullptr;
        // Reference counting
        llvm::Function *rc_inc_ = nullptr, *rc_dec_ = nullptr;
        // Bytes
        llvm::Function *bytes_alloc_ = nullptr, *bytes_length_ = nullptr,
            *bytes_get_ = nullptr, *bytes_set_ = nullptr, *bytes_concat_ = nullptr,
            *bytes_slice_ = nullptr, *bytes_from_string_ = nullptr, *bytes_to_string_ = nullptr,
            *bytes_from_seq_ = nullptr, *bytes_to_seq_ = nullptr;
        // Misc
        llvm::Function *box_ = nullptr, *close_ = nullptr;
        // Arena
        llvm::Function *arena_create_ = nullptr, *arena_alloc_ = nullptr,
            *arena_destroy_ = nullptr;
    } rt_;

    int64_t intern_symbol(const std::string& name);
    int opt_level_ = 2;

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
    // codegen_let helpers
    std::unordered_set<std::string> analyze_let_escaping(LetExpr* node);
    llvm::Value* setup_let_arena(const std::unordered_set<std::string>& non_escaping);
    void codegen_let_aliases(LetExpr* node, llvm::Value* arena,
                              const std::unordered_set<std::string>& non_escaping,
                              std::vector<TypedValue>& scope_bindings,
                              std::vector<bool>& binding_is_arena);
    void cleanup_let_scope(const std::vector<TypedValue>& scope_bindings,
                            const std::vector<bool>& binding_is_arena,
                            const TypedValue& result, llvm::Value* arena);

    TypedValue codegen_if(IfExpr* node);
    TypedValue codegen_case(CaseExpr* node);
    // codegen_case pattern helpers — return true if body codegen was inlined
    bool codegen_pattern_value(PatternValue* pat, const TypedValue& scrutinee,
                                llvm::BasicBlock* body_bb, llvm::BasicBlock* next_bb);
    bool codegen_pattern_headtail(HeadTailsPattern* pat, CaseExpr* node,
                                   CaseClause* clause, const TypedValue& scrutinee,
                                   llvm::Value* seq_ptr,
                                   llvm::BasicBlock* body_bb, llvm::BasicBlock* next_bb);
    bool codegen_pattern_seq(SeqPattern* pat, const TypedValue& scrutinee,
                              llvm::BasicBlock* body_bb, llvm::BasicBlock* next_bb);
    bool codegen_pattern_tuple(TuplePattern* pat, const TypedValue& scrutinee,
                                llvm::BasicBlock* body_bb, llvm::BasicBlock* next_bb);
    bool codegen_pattern_constructor(ConstructorPattern* pat, const TypedValue& scrutinee,
                                      llvm::BasicBlock* body_bb, llvm::BasicBlock* next_bb);
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

    // codegen_apply helpers (extracted for readability)
    struct ApplyChain {
        std::string fn_name;
        std::string module_fqn;
        std::vector<ApplyExpr*> chain;
    };
    ApplyChain flatten_apply_chain(ApplyExpr* node);

    struct EvaluatedArgs {
        std::vector<TypedValue> all_args;
        std::vector<std::string> arg_lambda_names;
    };
    EvaluatedArgs evaluate_apply_args(const std::vector<ApplyExpr*>& chain);
    void precompile_function_args(EvaluatedArgs& args);
    void wrap_function_args_in_closures(std::vector<TypedValue>& all_args);

    TypedValue codegen_adt_construct(const std::string& fn_name, const std::vector<TypedValue>& all_args);
    std::unordered_map<std::string, CompiledFunction>::iterator
        resolve_apply_function(const std::string& fn_name, const std::vector<TypedValue>& all_args);
    TypedValue codegen_higher_order_call(const std::string& fn_name, const std::vector<TypedValue>& all_args);
    TypedValue codegen_extern_call(ApplyExpr* node, const std::string& fn_name,
                                    const std::vector<TypedValue>& all_args);
    TypedValue codegen_partial_apply(const std::string& fn_name, CompiledFunction& cf,
                                      const std::vector<TypedValue>& all_args);
    TypedValue codegen_curry_apply(const std::string& fn_name, CompiledFunction& cf,
                                    const std::vector<TypedValue>& all_args);
    TypedValue emit_direct_call(const std::string& fn_name, CompiledFunction& cf,
                                 const std::vector<TypedValue>& all_args);

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
    TypedValue codegen_fused_seq_generator(SeqGeneratorExpr* outer, SeqGeneratorExpr* inner);
    static int count_identifier_refs(ast::AstNode* node, const std::string& name);
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
};

} // namespace yona::compiler::codegen
