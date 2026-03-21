//
// LLVM Code Generation for Yona
//
// Generates LLVM IR from a type-checked AST. Since Yona uses Hindley-Milner
// type inference, all types are known at compile time and primitives are unboxed.
//

#pragma once

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Target/TargetMachine.h>

#include <string>
#include <unordered_map>
#include <memory>

#include "ast.h"
#include "types.h"

namespace yona::compiler::codegen {

using namespace yona::ast;
using namespace yona::compiler::types;

class Codegen {
public:
    Codegen(const std::string& module_name = "yona_module");
    ~Codegen();

    // Compile an AST node to LLVM IR. Returns the module.
    llvm::Module* compile(AstNode* node);

    // Emit object file to path. Returns true on success.
    bool emit_object_file(const std::string& output_path);

    // Emit LLVM IR as text (for debugging)
    std::string emit_ir();

private:
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;
    llvm::TargetMachine* target_machine_ = nullptr;

    // Named values in current scope (variable name → LLVM value)
    std::unordered_map<std::string, llvm::Value*> named_values_;

    // Runtime function declarations
    llvm::Function* rt_print_int_ = nullptr;
    llvm::Function* rt_print_float_ = nullptr;
    llvm::Function* rt_print_string_ = nullptr;
    llvm::Function* rt_print_bool_ = nullptr;
    llvm::Function* rt_print_newline_ = nullptr;
    llvm::Function* rt_string_concat_ = nullptr;
    llvm::Function* rt_string_alloc_ = nullptr;
    llvm::Function* rt_seq_alloc_ = nullptr;
    llvm::Function* rt_seq_set_ = nullptr;
    llvm::Function* rt_seq_get_ = nullptr;
    llvm::Function* rt_seq_length_ = nullptr;
    llvm::Function* rt_seq_cons_ = nullptr;
    llvm::Function* rt_seq_join_ = nullptr;
    llvm::Function* rt_seq_head_ = nullptr;
    llvm::Function* rt_seq_tail_ = nullptr;
    llvm::Function* rt_print_seq_ = nullptr;

    // Initialize target machine
    void init_target();

    // Declare runtime functions
    void declare_runtime();

    // Generate code for a top-level expression (wraps in main())
    llvm::Function* codegen_main(AstNode* node);

    // Expression codegen — returns the LLVM value
    llvm::Value* codegen(AstNode* node);
    llvm::Value* codegen_integer(IntegerExpr* node);
    llvm::Value* codegen_float(FloatExpr* node);
    llvm::Value* codegen_bool(TrueLiteralExpr* node);
    llvm::Value* codegen_bool_false(FalseLiteralExpr* node);
    llvm::Value* codegen_string(StringExpr* node);
    llvm::Value* codegen_add(AddExpr* node);
    llvm::Value* codegen_subtract(SubtractExpr* node);
    llvm::Value* codegen_multiply(MultiplyExpr* node);
    llvm::Value* codegen_divide(DivideExpr* node);
    llvm::Value* codegen_modulo(ModuloExpr* node);
    llvm::Value* codegen_eq(EqExpr* node);
    llvm::Value* codegen_neq(NeqExpr* node);
    llvm::Value* codegen_lt(LtExpr* node);
    llvm::Value* codegen_gt(GtExpr* node);
    llvm::Value* codegen_lte(LteExpr* node);
    llvm::Value* codegen_gte(GteExpr* node);
    llvm::Value* codegen_and(LogicalAndExpr* node);
    llvm::Value* codegen_or(LogicalOrExpr* node);
    llvm::Value* codegen_not(LogicalNotOpExpr* node);
    llvm::Value* codegen_negate(SubtractExpr* node);
    llvm::Value* codegen_let(LetExpr* node);
    llvm::Value* codegen_if(IfExpr* node);
    llvm::Value* codegen_identifier(IdentifierExpr* node);
    llvm::Value* codegen_main_node(MainNode* node);
    llvm::Value* codegen_function(FunctionExpr* node);
    llvm::Value* codegen_apply(ApplyExpr* node);
    llvm::Value* codegen_lambda_alias(LambdaAlias* node);
    llvm::Value* codegen_case(CaseExpr* node);
    llvm::Value* codegen_do(DoExpr* node);
    llvm::Value* codegen_unit(UnitExpr* node);
    llvm::Value* codegen_negate(IntegerExpr* node);
    llvm::Value* codegen_tuple(TupleExpr* node);
    llvm::Value* codegen_seq(ValuesSequenceExpr* node);
    llvm::Value* codegen_cons(ConsLeftExpr* node);
    llvm::Value* codegen_join(JoinExpr* node);

    // Pattern matching helpers
    llvm::Value* codegen_pattern_match(PatternNode* pattern, llvm::Value* scrutinee,
                                        llvm::BasicBlock* match_bb, llvm::BasicBlock* fail_bb);

    // Function name counter for unique anonymous function names
    int lambda_counter_ = 0;

    // Closure capture info: function name → captured variable values
    std::unordered_map<std::string, std::vector<llvm::Value*>> closure_captures_;

    // Track which values are sequences (vs strings) — both are ptr in opaque pointer mode
    std::unordered_set<llvm::Value*> seq_values_;

    // Helper: get LLVM type for a Yona type
    llvm::Type* get_llvm_type(const Type& yona_type);

    // Helper: print a value (for top-level expression result)
    void codegen_print_value(llvm::Value* val);
};

} // namespace yona::compiler::codegen
