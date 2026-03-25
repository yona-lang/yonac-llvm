//
// LLVM Code Generation for Yona — Type-directed codegen
//
// Every expression produces a TypedValue (LLVM Value + CType tag).
// Types propagate structurally: codegen_integer returns {i64, INT},
// codegen_add checks operand CTypes to choose iadd vs fadd, etc.
// Functions are deferred until call sites where arg types are known.
//

#include "Codegen.h"

#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>

#include <iostream>
#include <sstream>

namespace yona::compiler::codegen {

using namespace llvm;
using LType = llvm::Type; // avoid collision with yona::compiler::types::Type

// ===== Free variable analysis =====

void Codegen::collect_free_vars(AstNode* node, const std::unordered_set<std::string>& bound,
                                 std::unordered_set<std::string>& free_vars) {
    if (!node) return;
    switch (node->get_type()) {
        case AST_IDENTIFIER_EXPR: {
            auto* id = static_cast<IdentifierExpr*>(node);
            if (!bound.count(id->name->value)) free_vars.insert(id->name->value);
            break;
        }
        case AST_ADD_EXPR: case AST_SUBTRACT_EXPR: case AST_MULTIPLY_EXPR:
        case AST_DIVIDE_EXPR: case AST_MODULO_EXPR:
        case AST_EQ_EXPR: case AST_NEQ_EXPR:
        case AST_LT_EXPR: case AST_GT_EXPR: case AST_LTE_EXPR: case AST_GTE_EXPR:
        case AST_LOGICAL_AND_EXPR: case AST_LOGICAL_OR_EXPR: {
            auto* bin = static_cast<AddExpr*>(node);
            collect_free_vars(bin->left, bound, free_vars);
            collect_free_vars(bin->right, bound, free_vars);
            break;
        }
        case AST_IF_EXPR: {
            auto* e = static_cast<IfExpr*>(node);
            collect_free_vars(e->condition, bound, free_vars);
            collect_free_vars(e->thenExpr, bound, free_vars);
            collect_free_vars(e->elseExpr, bound, free_vars);
            break;
        }
        case AST_LET_EXPR: {
            auto* e = static_cast<LetExpr*>(node);
            auto nb = bound;
            for (auto* a : e->aliases) {
                if (auto* va = dynamic_cast<ValueAlias*>(a)) {
                    collect_free_vars(va->expr, bound, free_vars);
                    nb.insert(va->identifier->name->value);
                } else if (auto* la = dynamic_cast<LambdaAlias*>(a))
                    nb.insert(la->name->value);
            }
            collect_free_vars(e->expr, nb, free_vars);
            break;
        }
        case AST_CASE_EXPR: {
            auto* e = static_cast<CaseExpr*>(node);
            collect_free_vars(e->expr, bound, free_vars);
            for (auto* c : e->clauses) collect_free_vars(c->body, bound, free_vars);
            break;
        }
        case AST_APPLY_EXPR: {
            auto* e = static_cast<ApplyExpr*>(node);
            if (auto* nc = dynamic_cast<NameCall*>(e->call))
                if (!bound.count(nc->name->value)) free_vars.insert(nc->name->value);
            for (auto& a : e->args) {
                if (std::holds_alternative<ExprNode*>(a))
                    collect_free_vars(std::get<ExprNode*>(a), bound, free_vars);
                else collect_free_vars(std::get<ValueExpr*>(a), bound, free_vars);
            }
            break;
        }
        default: break;
    }
}

// ===== Constructor / Init =====

Codegen::Codegen(const std::string& module_name) {
    context_ = std::make_unique<LLVMContext>();
    module_ = std::make_unique<Module>(module_name, *context_);
    builder_ = std::make_unique<IRBuilder<>>(*context_);
    init_target();
    declare_runtime();
}
Codegen::~Codegen() = default;

void Codegen::init_target() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    auto triple = sys::getDefaultTargetTriple();
    module_->setTargetTriple(Triple(triple));
    std::string err;
    auto target = TargetRegistry::lookupTarget(triple, err);
    if (!target) { std::cerr << "Target error: " << err << "\n"; return; }
    TargetOptions opt;
    target_machine_ = target->createTargetMachine(triple, "generic", "", opt, Reloc::PIC_);
    module_->setDataLayout(target_machine_->createDataLayout());
}

LType* Codegen::llvm_type(CType ct) {
    switch (ct) {
        case CType::INT:    return LType::getInt64Ty(*context_);
        case CType::FLOAT:  return LType::getDoubleTy(*context_);
        case CType::BOOL:   return LType::getInt1Ty(*context_);
        case CType::STRING: return PointerType::get(LType::getInt8Ty(*context_), 0);
        case CType::SEQ:    return PointerType::get(LType::getInt64Ty(*context_), 0);
        case CType::TUPLE:  return LType::getInt64Ty(*context_); // overridden per-tuple
        case CType::UNIT:   return LType::getInt64Ty(*context_);
        case CType::FUNCTION: return PointerType::get(LType::getInt8Ty(*context_), 0);
        case CType::SYMBOL: return LType::getInt64Ty(*context_); // interned symbol ID
        case CType::SET:    return PointerType::get(LType::getInt64Ty(*context_), 0);
        case CType::DICT:   return PointerType::get(LType::getInt64Ty(*context_), 0);
        case CType::PROMISE: return PointerType::get(LType::getInt8Ty(*context_), 0);
        case CType::ADT:    return LType::getInt64Ty(*context_); // overridden per-ADT
    }
    return LType::getInt64Ty(*context_);
}

void Codegen::declare_runtime() {
    auto i64 = LType::getInt64Ty(*context_);
    auto f64 = LType::getDoubleTy(*context_);
    auto vd = LType::getVoidTy(*context_);
    auto i1 = LType::getInt1Ty(*context_);
    auto ptr = PointerType::get(LType::getInt8Ty(*context_), 0);
    auto i64p = PointerType::get(i64, 0);

    auto decl = [&](const char* name, LType* ret, std::vector<LType*> args) {
        return Function::Create(llvm::FunctionType::get(ret, args, false),
                                Function::ExternalLinkage, name, module_.get());
    };

    rt_print_int_     = decl("yona_rt_print_int", vd, {i64});
    rt_print_float_   = decl("yona_rt_print_float", vd, {f64});
    rt_print_string_  = decl("yona_rt_print_string", vd, {ptr});
    rt_print_bool_    = decl("yona_rt_print_bool", vd, {i1});
    rt_print_newline_ = decl("yona_rt_print_newline", vd, {});
    rt_print_seq_     = decl("yona_rt_print_seq", vd, {i64p});
    rt_string_concat_ = decl("yona_rt_string_concat", ptr, {ptr, ptr});
    rt_seq_alloc_     = decl("yona_rt_seq_alloc", i64p, {i64});
    rt_seq_set_       = decl("yona_rt_seq_set", vd, {i64p, i64, i64});
    rt_seq_get_       = decl("yona_rt_seq_get", i64, {i64p, i64});
    rt_seq_length_    = decl("yona_rt_seq_length", i64, {i64p});
    rt_seq_cons_      = decl("yona_rt_seq_cons", i64p, {i64, i64p});
    rt_seq_join_      = decl("yona_rt_seq_join", i64p, {i64p, i64p});
    rt_seq_head_      = decl("yona_rt_seq_head", i64, {i64p});
    rt_seq_tail_      = decl("yona_rt_seq_tail", i64p, {i64p});
    rt_print_symbol_  = decl("yona_rt_print_symbol", vd, {ptr}); // takes char* name

    // Set runtime
    rt_set_alloc_     = decl("yona_rt_set_alloc", i64p, {i64});
    rt_set_put_       = decl("yona_rt_set_put", vd, {i64p, i64, i64});
    rt_print_set_     = decl("yona_rt_print_set", vd, {i64p});

    // Dict runtime
    rt_dict_alloc_    = decl("yona_rt_dict_alloc", i64p, {i64});
    rt_dict_set_      = decl("yona_rt_dict_set", vd, {i64p, i64, i64, i64});
    rt_print_dict_    = decl("yona_rt_print_dict", vd, {i64p});

    // Async runtime: promise = async_call(fn_ptr, arg), result = async_await(promise)
    // fn_ptr type: i64 (*)(i64) — function pointer taking and returning i64
    auto fn_ptr_ty = PointerType::get(llvm::FunctionType::get(i64, {i64}, false), 0);
    auto promise_ptr = ptr; // opaque pointer to yona_promise_t
    rt_async_call_    = decl("yona_rt_async_call", promise_ptr, {fn_ptr_ty, i64});
    rt_async_await_   = decl("yona_rt_async_await", i64, {promise_ptr});
}

// ===== Public API =====

std::string Codegen::mangle_name(const std::string& module_fqn, const std::string& func_name) {
    // Replace backslash with underscore: Test\Test → Test_Test
    std::string mangled = "yona_";
    for (char c : module_fqn) {
        mangled += (c == '\\' || c == '/') ? '_' : c;
    }
    mangled += "__";
    mangled += func_name;
    return mangled;
}

Module* Codegen::compile(AstNode* node) {
    auto fn = codegen_main(node);
    if (!fn) return nullptr;
    std::string err;
    raw_string_ostream os(err);
    if (verifyModule(*module_, &os)) {
        std::cerr << "Module verification failed:\n" << err << "\n";
        return nullptr;
    }
    optimize();
    return module_.get();
}

// Infer a CType from a single pattern node.
// Tuple patterns → TUPLE, sequence/head-tail patterns → SEQ,
// symbol patterns → SYMBOL, etc. Variable/wildcard defaults to INT.
CType Codegen::infer_type_from_pattern(PatternNode* pat) {
    if (!pat) return CType::INT;

    switch (pat->get_type()) {
        case AST_TUPLE_PATTERN:
            return CType::TUPLE;

        case AST_SEQ_PATTERN:
        case AST_HEAD_TAILS_PATTERN:
        case AST_TAILS_HEAD_PATTERN:
        case AST_HEAD_TAILS_HEAD_PATTERN:
            return CType::SEQ;

        case AST_PATTERN_VALUE: {
            auto* pv = static_cast<PatternValue*>(pat);
            // Check what kind of value it is
            if (std::holds_alternative<SymbolExpr*>(pv->expr))
                return CType::SYMBOL;
            if (std::holds_alternative<IdentifierExpr*>(pv->expr))
                return CType::INT; // variable — unknown, default to INT
            if (std::holds_alternative<LiteralExpr<void*>*>(pv->expr)) {
                auto* lit = std::get<LiteralExpr<void*>*>(pv->expr);
                // Check literal type from AST node type
                if (dynamic_cast<IntegerExpr*>(lit))
                    return CType::INT;
                if (dynamic_cast<FloatExpr*>(lit))
                    return CType::FLOAT;
                if (dynamic_cast<StringExpr*>(lit))
                    return CType::STRING;
                if (dynamic_cast<TrueLiteralExpr*>(lit) || dynamic_cast<FalseLiteralExpr*>(lit))
                    return CType::BOOL;
            }
            return CType::INT;
        }

        case AST_UNDERSCORE_PATTERN:
        case AST_UNDERSCORE_NODE:
            return CType::INT; // wildcard — unknown

        case AST_AS_DATA_STRUCTURE_PATTERN: {
            auto* as = static_cast<AsDataStructurePattern*>(pat);
            return infer_type_from_pattern(as->pattern);
        }

        case AST_OR_PATTERN: {
            auto* op = static_cast<OrPattern*>(pat);
            if (!op->patterns.empty())
                return infer_type_from_pattern(op->patterns[0].get());
            return CType::INT;
        }

        case AST_CONSTRUCTOR_PATTERN:
            return CType::ADT;

        default:
            return CType::INT;
    }
}

// Infer parameter types for a function by analyzing its patterns
// and body. Pattern-based inference handles tuple/seq/symbol patterns.
// Body-based inference handles variable parameters used as case scrutinees.
std::vector<Codegen::InferredParamType> Codegen::infer_param_types(FunctionExpr* func) {
    std::vector<InferredParamType> result;
    result.resize(func->patterns.size());

    // First: infer from patterns directly
    std::unordered_map<std::string, size_t> param_index;
    for (size_t i = 0; i < func->patterns.size(); i++) {
        auto* pat = func->patterns[i];
        result[i].type = infer_type_from_pattern(pat);
        result[i].source_pattern = pat;

        if (pat->get_type() == AST_PATTERN_VALUE) {
            auto* pv = static_cast<PatternValue*>(pat);
            if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                param_index[(*id)->name->value] = i;
        }
    }

    // Second: analyze body for case expressions that reveal parameter types
    if (!func->bodies.empty()) {
        ExprNode* body_expr = nullptr;
        if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(func->bodies[0]))
            body_expr = bwg->expr;

        std::function<void(AstNode*)> analyze = [&](AstNode* node) {
            if (!node) return;

            // Case expressions: scrutinee type inference
            if (node->get_type() == AST_CASE_EXPR) {
                auto* ce = static_cast<CaseExpr*>(node);
                if (ce->expr && ce->expr->get_type() == AST_IDENTIFIER_EXPR) {
                    auto* id = static_cast<IdentifierExpr*>(ce->expr);
                    auto it = param_index.find(id->name->value);
                    if (it != param_index.end() && result[it->second].type == CType::INT) {
                        for (auto* clause : ce->clauses) {
                            CType pat_type = infer_type_from_pattern(clause->pattern);
                            if (pat_type != CType::INT) {
                                result[it->second].type = pat_type;
                                result[it->second].source_pattern = clause->pattern;
                                break;
                            }
                        }
                    }
                }
                for (auto* clause : ce->clauses) analyze(clause->body);
            }

            // Apply expressions: if a parameter is in function position, it's FUNCTION
            if (node->get_type() == AST_APPLY_EXPR) {
                auto* ae = static_cast<ApplyExpr*>(node);
                // Check NameCall: fn x → fn is in function position
                if (auto* nc = dynamic_cast<NameCall*>(ae->call)) {
                    auto it = param_index.find(nc->name->value);
                    if (it != param_index.end() && result[it->second].type == CType::INT) {
                        result[it->second].type = CType::FUNCTION;
                        result[it->second].source_pattern = nullptr;
                    }
                }
                // Check ExprCall where expr is an identifier (curried application)
                if (auto* ec = dynamic_cast<ExprCall*>(ae->call)) {
                    if (auto* id = dynamic_cast<IdentifierExpr*>(ec->expr)) {
                        auto it = param_index.find(id->name->value);
                        if (it != param_index.end() && result[it->second].type == CType::INT) {
                            result[it->second].type = CType::FUNCTION;
                            result[it->second].source_pattern = nullptr;
                        }
                    }
                    // Recurse into the call expression (nested apply chains)
                    analyze(ec->expr);
                }
                // Recurse into arguments
                for (auto& a : ae->args) {
                    if (std::holds_alternative<ExprNode*>(a)) analyze(std::get<ExprNode*>(a));
                    else analyze(std::get<ValueExpr*>(a));
                }
            }

            // Recurse into all expression types that may contain sub-expressions
            if (auto* le = dynamic_cast<LetExpr*>(node)) {
                analyze(le->expr);
                for (auto* alias : le->aliases) analyze(alias);
            } else if (auto* ie = dynamic_cast<IfExpr*>(node)) {
                analyze(ie->condition); analyze(ie->thenExpr); analyze(ie->elseExpr);
            } else if (auto* de = dynamic_cast<DoExpr*>(node)) {
                for (auto* s : de->steps) analyze(s);
            } else if (auto* bop = dynamic_cast<BinaryOpExpr*>(node)) {
                analyze(bop->left); analyze(bop->right);
            } else if (auto* te = dynamic_cast<TupleExpr*>(node)) {
                for (auto* v : te->values) analyze(v);
            } else if (auto* ve = dynamic_cast<ValuesSequenceExpr*>(node)) {
                for (auto* v : ve->values) analyze(v);
            }
        };
        if (body_expr) analyze(body_expr);
    }

    return result;
}

Module* Codegen::compile_module(ModuleExpr* mod) {
    // Build the module FQN string
    std::string fqn;
    if (mod->fqn->packageName.has_value()) {
        auto* pkg = mod->fqn->packageName.value();
        for (size_t i = 0; i < pkg->parts.size(); i++) {
            if (i > 0) fqn += "\\";
            fqn += pkg->parts[i]->value;
        }
        fqn += "\\";
    }
    fqn += mod->fqn->moduleName->value;

    // Build export set for visibility control
    std::unordered_set<std::string> export_set(mod->exports.begin(), mod->exports.end());

    // Process ADT declarations: register constructors
    for (auto* adt : mod->adt_declarations) {
        // Compute max arity across all constructors in this ADT
        int max_arity = 0;
        for (auto* ctor : adt->variants) {
            int a = static_cast<int>(ctor->field_type_names.size());
            if (a > max_arity) max_arity = a;
        }

        for (size_t ci = 0; ci < adt->variants.size(); ci++) {
            auto* ctor = adt->variants[ci];
            int arity = static_cast<int>(ctor->field_type_names.size());
            adt_constructors_[ctor->name] = {adt->name, static_cast<int>(ci), arity,
                                              static_cast<int>(adt->variants.size()), max_arity};
        }
    }

    // First pass: register all functions as deferred
    for (auto* func : mod->functions) {
        std::string fn_name = func->name;
        codegen_function_def(func, fn_name);
    }

    // Second pass: compile each function with inferred parameter types
    for (auto* func : mod->functions) {
        std::string fn_name = func->name;
        bool is_exported = export_set.count(fn_name) > 0;

        auto def_it = deferred_functions_.find(fn_name);
        if (def_it == deferred_functions_.end()) continue;

        // Infer parameter types from function patterns and body analysis
        auto inferred = infer_param_types(func);

        std::vector<TypedValue> typed_args;
        for (size_t i = 0; i < func->patterns.size(); i++) {
            CType ct = (i < inferred.size()) ? inferred[i].type : CType::INT;
            auto* dummy_val = ConstantInt::get(LType::getInt64Ty(*context_), 0);

            if (ct == CType::TUPLE) {
                // Find the tuple pattern (from direct pattern or body case pattern)
                PatternNode* src = (i < inferred.size()) ? inferred[i].source_pattern : nullptr;
                auto* tp = src ? dynamic_cast<TuplePattern*>(src) : nullptr;
                if (tp) {
                    std::vector<LType*> elem_types;
                    std::vector<CType> elem_ctypes;
                    for (size_t j = 0; j < tp->patterns.size(); j++) {
                        CType et = infer_type_from_pattern(static_cast<PatternNode*>(tp->patterns[j]));
                        elem_types.push_back(llvm_type(et));
                        elem_ctypes.push_back(et);
                    }
                    auto* st = StructType::get(*context_, elem_types);
                    typed_args.push_back({UndefValue::get(st), CType::TUPLE, elem_ctypes});
                } else {
                    typed_args.push_back({dummy_val, ct});
                }
            } else if (ct == CType::SEQ) {
                auto* ptr_type = PointerType::get(*context_, 0);
                typed_args.push_back({ConstantPointerNull::get(ptr_type), CType::SEQ});
            } else if (ct == CType::STRING) {
                auto* ptr_type = PointerType::get(*context_, 0);
                typed_args.push_back({ConstantPointerNull::get(ptr_type), CType::STRING});
            } else if (ct == CType::FLOAT) {
                typed_args.push_back({ConstantFP::get(LType::getDoubleTy(*context_), 0.0), CType::FLOAT});
            } else if (ct == CType::BOOL) {
                typed_args.push_back({ConstantInt::get(LType::getInt1Ty(*context_), 0), CType::BOOL});
            } else if (ct == CType::SYMBOL) {
                typed_args.push_back({ConstantInt::get(LType::getInt64Ty(*context_), 0), CType::SYMBOL});
            } else if (ct == CType::FUNCTION) {
                auto* ptr_type = PointerType::get(*context_, 0);
                typed_args.push_back({ConstantPointerNull::get(ptr_type), CType::FUNCTION});
            } else if (ct == CType::ADT) {
                // Build the ADT struct type based on the constructor pattern
                // Find which ADT type by looking at the case patterns
                PatternNode* src = (i < inferred.size()) ? inferred[i].source_pattern : nullptr;
                auto* cp = src ? dynamic_cast<ConstructorPattern*>(src) : nullptr;
                if (cp) {
                    auto ctor_it = adt_constructors_.find(cp->constructor_name);
                    if (ctor_it != adt_constructors_.end()) {
                        auto i8_ty = LType::getInt8Ty(*context_);
                        auto i64_ty = LType::getInt64Ty(*context_);
                        std::vector<LType*> fields = {i8_ty};
                        for (int f = 0; f < ctor_it->second.max_arity; f++)
                            fields.push_back(i64_ty);
                        auto* st = StructType::get(*context_, fields);
                        typed_args.push_back({UndefValue::get(st), CType::ADT});
                    } else {
                        typed_args.push_back({dummy_val, ct});
                    }
                } else {
                    typed_args.push_back({dummy_val, ct});
                }
            } else {
                typed_args.push_back({dummy_val, ct});
            }
        }

        auto cf = compile_function(fn_name, def_it->second, typed_args);

        if (is_exported) {
            // Store type metadata for importers
            std::string mangled = mangle_name(fqn, fn_name);
            module_meta_[mangled] = {cf.param_types, cf.return_type};

            // Check if the function already has the right linkage
            if (cf.fn->getName() != mangled) {
                // Create a wrapper function with external linkage and mangled name
                auto* wrapper = Function::Create(
                    cf.fn->getFunctionType(),
                    Function::ExternalLinkage,
                    mangled, module_.get());

                auto* bb = BasicBlock::Create(*context_, "entry", wrapper);
                builder_->SetInsertPoint(bb);

                std::vector<Value*> args;
                for (auto& arg : wrapper->args()) args.push_back(&arg);
                auto* result = builder_->CreateCall(cf.fn, args);
                builder_->CreateRet(result);
            } else {
                cf.fn->setLinkage(Function::ExternalLinkage);
            }
        }
    }

    // Verify
    std::string err;
    raw_string_ostream os(err);
    if (verifyModule(*module_, &os)) {
        std::cerr << "Module verification failed:\n" << err << "\n";
        return nullptr;
    }
    optimize();
    return module_.get();
}



bool Codegen::emit_object_file(const std::string& path) {
    if (!target_machine_) return false;
    std::error_code ec;
    raw_fd_ostream dest(path, ec, sys::fs::OF_None);
    if (ec) return false;
    legacy::PassManager pass;
    if (target_machine_->addPassesToEmitFile(pass, dest, nullptr, CodeGenFileType::ObjectFile))
        return false;
    pass.run(*module_);
    dest.flush();
    return true;
}

std::string Codegen::emit_ir() {
    std::string ir;
    raw_string_ostream os(ir);
    module_->print(os, nullptr);
    return ir;
}

void Codegen::optimize() {
    if (!module_) return;

    legacy::FunctionPassManager fpm(module_.get());

    // Promote allocas to registers (mem2reg)
    fpm.add(createPromoteMemoryToRegisterPass());
    // Combine redundant instructions
    fpm.add(createInstructionCombiningPass());
    // Reassociate expressions for better constant folding
    fpm.add(createReassociatePass());
    // Eliminate common subexpressions
    fpm.add(createGVNPass());
    // Simplify control flow (remove unreachable blocks, etc.)
    fpm.add(createCFGSimplificationPass());
    // Tail call elimination
    fpm.add(createTailCallEliminationPass());

    fpm.doInitialization();
    for (auto& fn : *module_) {
        fpm.run(fn);
    }
    fpm.doFinalization();
}

// ===== Entry Point =====

Function* Codegen::codegen_main(AstNode* node) {
    auto i32 = LType::getInt32Ty(*context_);
    auto fn = Function::Create(llvm::FunctionType::get(i32, {}, false),
                                Function::ExternalLinkage, "main", module_.get());
    auto bb = BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(bb);

    auto result = codegen(node);
    if (result) codegen_print(result);
    builder_->CreateRet(ConstantInt::get(i32, 0));
    return fn;
}

// ===== Print (type-directed) =====

void Codegen::codegen_print_value(const TypedValue& tv) {
    if (!tv.val) return;
    switch (tv.type) {
        case CType::INT:    builder_->CreateCall(rt_print_int_, {tv.val}); break;
        case CType::FLOAT:  builder_->CreateCall(rt_print_float_, {tv.val}); break;
        case CType::BOOL:   builder_->CreateCall(rt_print_bool_, {tv.val}); break;
        case CType::STRING: builder_->CreateCall(rt_print_string_, {tv.val}); break;
        case CType::SEQ:    builder_->CreateCall(rt_print_seq_, {tv.val}); break;
        case CType::TUPLE: {
            builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr("(")});
            if (tv.val->getType()->isStructTy()) {
                auto* st = cast<StructType>(tv.val->getType());
                for (unsigned i = 0; i < st->getNumElements(); i++) {
                    if (i > 0)
                        builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr(", ")});
                    auto elem = builder_->CreateExtractValue(tv.val, {i});
                    CType et = (i < tv.subtypes.size()) ? tv.subtypes[i] : CType::INT;
                    codegen_print_value({elem, et});
                }
            }
            builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr(")")});
            break;
        }
        case CType::SET:    builder_->CreateCall(rt_print_set_, {tv.val}); break;
        case CType::DICT:   builder_->CreateCall(rt_print_dict_, {tv.val}); break;
        case CType::UNIT:     builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr("()")}); break;
        case CType::FUNCTION: builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr("<function>")}); break;
        case CType::SYMBOL: {
            // Symbol is an interned i64 ID. Look up the string for printing.
            if (auto* ci = dyn_cast<ConstantInt>(tv.val)) {
                int64_t id = ci->getSExtValue();
                if (id >= 0 && id < (int64_t)symbol_strings_.size()) {
                    builder_->CreateCall(rt_print_symbol_, {symbol_strings_[id]});
                }
            } else {
                // Runtime symbol value — need a table lookup.
                // Emit a GEP into the symbol names table (emitted at finalization).
                // For now, emit a placeholder.
                builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr(":<dynamic>")});
            }
            break;
        }
        case CType::ADT: {
            builder_->CreateCall(rt_print_string_, {builder_->CreateGlobalStringPtr("<adt>")});
            break;
        }
    }
}

void Codegen::codegen_print(const TypedValue& tv) {
    auto resolved = auto_await(tv);
    codegen_print_value(resolved);
    builder_->CreateCall(rt_print_newline_, {});
}

// ===== Core Dispatch =====

TypedValue Codegen::codegen(AstNode* node) {
    if (!node) return {};
    switch (node->get_type()) {
        case AST_MAIN:            return codegen_main_node(static_cast<MainNode*>(node));
        case AST_INTEGER_EXPR:    return codegen_integer(static_cast<IntegerExpr*>(node));
        case AST_FLOAT_EXPR:      return codegen_float(static_cast<FloatExpr*>(node));
        case AST_TRUE_LITERAL_EXPR:  return codegen_bool_true(static_cast<TrueLiteralExpr*>(node));
        case AST_FALSE_LITERAL_EXPR: return codegen_bool_false(static_cast<FalseLiteralExpr*>(node));
        case AST_STRING_EXPR:     return codegen_string(static_cast<StringExpr*>(node));
        case AST_UNIT_EXPR:       return codegen_unit(static_cast<UnitExpr*>(node));
        case AST_SYMBOL_EXPR:    return codegen_symbol(static_cast<SymbolExpr*>(node));
        case AST_ADD_EXPR:        return codegen_binary(static_cast<AddExpr*>(node)->left, static_cast<AddExpr*>(node)->right, "+");
        case AST_SUBTRACT_EXPR:   return codegen_binary(static_cast<SubtractExpr*>(node)->left, static_cast<SubtractExpr*>(node)->right, "-");
        case AST_MULTIPLY_EXPR:   return codegen_binary(static_cast<MultiplyExpr*>(node)->left, static_cast<MultiplyExpr*>(node)->right, "*");
        case AST_DIVIDE_EXPR:     return codegen_binary(static_cast<DivideExpr*>(node)->left, static_cast<DivideExpr*>(node)->right, "/");
        case AST_MODULO_EXPR:     return codegen_binary(static_cast<ModuloExpr*>(node)->left, static_cast<ModuloExpr*>(node)->right, "%");
        case AST_EQ_EXPR:         return codegen_comparison(static_cast<EqExpr*>(node)->left, static_cast<EqExpr*>(node)->right, "==");
        case AST_NEQ_EXPR:        return codegen_comparison(static_cast<NeqExpr*>(node)->left, static_cast<NeqExpr*>(node)->right, "!=");
        case AST_LT_EXPR:         return codegen_comparison(static_cast<LtExpr*>(node)->left, static_cast<LtExpr*>(node)->right, "<");
        case AST_GT_EXPR:         return codegen_comparison(static_cast<GtExpr*>(node)->left, static_cast<GtExpr*>(node)->right, ">");
        case AST_LTE_EXPR:        return codegen_comparison(static_cast<LteExpr*>(node)->left, static_cast<LteExpr*>(node)->right, "<=");
        case AST_GTE_EXPR:        return codegen_comparison(static_cast<GteExpr*>(node)->left, static_cast<GteExpr*>(node)->right, ">=");
        case AST_LOGICAL_AND_EXPR: { auto l = codegen(static_cast<LogicalAndExpr*>(node)->left); auto r = codegen(static_cast<LogicalAndExpr*>(node)->right); return {builder_->CreateAnd(l.val, r.val), CType::BOOL}; }
        case AST_LOGICAL_OR_EXPR:  { auto l = codegen(static_cast<LogicalOrExpr*>(node)->left); auto r = codegen(static_cast<LogicalOrExpr*>(node)->right); return {builder_->CreateOr(l.val, r.val), CType::BOOL}; }
        case AST_LOGICAL_NOT_OP_EXPR: { auto v = codegen(static_cast<LogicalNotOpExpr*>(node)->expr); return {builder_->CreateNot(v.val), CType::BOOL}; }
        case AST_LET_EXPR:        return codegen_let(static_cast<LetExpr*>(node));
        case AST_IF_EXPR:         return codegen_if(static_cast<IfExpr*>(node));
        case AST_CASE_EXPR:       return codegen_case(static_cast<CaseExpr*>(node));
        case AST_DO_EXPR:         return codegen_do(static_cast<DoExpr*>(node));
        case AST_IDENTIFIER_EXPR: return codegen_identifier(static_cast<IdentifierExpr*>(node));
        case AST_FUNCTION_EXPR:   return codegen_function_def(static_cast<FunctionExpr*>(node), "");
        case AST_APPLY_EXPR:      return codegen_apply(static_cast<ApplyExpr*>(node));
        case AST_LAMBDA_ALIAS:    return codegen_lambda_alias(static_cast<LambdaAlias*>(node));
        case AST_IMPORT_EXPR:     return codegen_import(static_cast<ImportExpr*>(node));
        case AST_EXTERN_DECL:    return codegen_extern_decl(static_cast<ExternDeclExpr*>(node));
        case AST_TUPLE_EXPR:      return codegen_tuple(static_cast<TupleExpr*>(node));
        case AST_VALUES_SEQUENCE_EXPR: return codegen_seq(static_cast<ValuesSequenceExpr*>(node));
        case AST_SET_EXPR:        return codegen_set(static_cast<SetExpr*>(node));
        case AST_DICT_EXPR:       return codegen_dict(static_cast<DictExpr*>(node));
        case AST_CONS_LEFT_EXPR:  return codegen_cons(static_cast<ConsLeftExpr*>(node));
        case AST_JOIN_EXPR:       return codegen_join(static_cast<JoinExpr*>(node));
        case AST_ADT_DECL:       return {}; // handled at module level
        case AST_ADT_CONSTRUCTOR: return {};
        case AST_CONSTRUCTOR_PATTERN: return {};
        default:
            std::cerr << "Codegen: unsupported node type " << node->get_type() << "\n";
            return {};
    }
}

TypedValue Codegen::codegen_main_node(MainNode* node) { return codegen(node->node); }

// ===== Literals =====

TypedValue Codegen::codegen_integer(IntegerExpr* node) {
    return {ConstantInt::get(LType::getInt64Ty(*context_), node->value), CType::INT};
}
TypedValue Codegen::codegen_float(FloatExpr* node) {
    return {ConstantFP::get(LType::getDoubleTy(*context_), node->value), CType::FLOAT};
}
TypedValue Codegen::codegen_bool_true(TrueLiteralExpr*) {
    return {ConstantInt::getTrue(*context_), CType::BOOL};
}
TypedValue Codegen::codegen_bool_false(FalseLiteralExpr*) {
    return {ConstantInt::getFalse(*context_), CType::BOOL};
}
TypedValue Codegen::codegen_string(StringExpr* node) {
    return {builder_->CreateGlobalStringPtr(node->value), CType::STRING};
}
TypedValue Codegen::codegen_unit(UnitExpr*) {
    return {ConstantInt::get(LType::getInt64Ty(*context_), 0), CType::UNIT};
}
int64_t Codegen::intern_symbol(const std::string& name) {
    auto it = symbol_ids_.find(name);
    if (it != symbol_ids_.end()) return it->second;
    int64_t id = static_cast<int64_t>(symbol_strings_.size());
    symbol_ids_[name] = id;
    symbol_strings_.push_back(builder_->CreateGlobalStringPtr(name, "sym." + name));
    return id;
}

TypedValue Codegen::codegen_symbol(SymbolExpr* node) {
    int64_t id = intern_symbol(node->value);
    return {ConstantInt::get(LType::getInt64Ty(*context_), id), CType::SYMBOL};
}

// ===== Arithmetic (type-directed) =====

TypedValue Codegen::codegen_binary(AstNode* left_node, AstNode* right_node, const std::string& op) {
    auto left = auto_await(codegen(left_node));
    auto right = auto_await(codegen(right_node));
    if (!left || !right) return {};

    // String concatenation
    if (left.type == CType::STRING && right.type == CType::STRING && op == "+") {
        return {builder_->CreateCall(rt_string_concat_, {left.val, right.val}), CType::STRING};
    }

    // Promote int to float if mixed
    if (left.type == CType::INT && right.type == CType::FLOAT) {
        left.val = builder_->CreateSIToFP(left.val, LType::getDoubleTy(*context_));
        left.type = CType::FLOAT;
    }
    if (left.type == CType::FLOAT && right.type == CType::INT) {
        right.val = builder_->CreateSIToFP(right.val, LType::getDoubleTy(*context_));
        right.type = CType::FLOAT;
    }

    bool is_float = (left.type == CType::FLOAT);
    Value* result = nullptr;

    if (op == "+") result = is_float ? builder_->CreateFAdd(left.val, right.val) : builder_->CreateAdd(left.val, right.val);
    else if (op == "-") result = is_float ? builder_->CreateFSub(left.val, right.val) : builder_->CreateSub(left.val, right.val);
    else if (op == "*") result = is_float ? builder_->CreateFMul(left.val, right.val) : builder_->CreateMul(left.val, right.val);
    else if (op == "/") result = is_float ? builder_->CreateFDiv(left.val, right.val) : builder_->CreateSDiv(left.val, right.val);
    else if (op == "%") result = builder_->CreateSRem(left.val, right.val);

    return {result, is_float ? CType::FLOAT : CType::INT};
}

TypedValue Codegen::codegen_comparison(AstNode* left_node, AstNode* right_node, const std::string& op) {
    auto left = auto_await(codegen(left_node));
    auto right = auto_await(codegen(right_node));
    if (!left || !right) return {};

    bool is_float = (left.type == CType::FLOAT || right.type == CType::FLOAT);
    if (is_float) {
        if (left.type == CType::INT) left.val = builder_->CreateSIToFP(left.val, LType::getDoubleTy(*context_));
        if (right.type == CType::INT) right.val = builder_->CreateSIToFP(right.val, LType::getDoubleTy(*context_));
    }

    Value* result = nullptr;
    if (is_float) {
        if (op == "==") result = builder_->CreateFCmpOEQ(left.val, right.val);
        else if (op == "!=") result = builder_->CreateFCmpONE(left.val, right.val);
        else if (op == "<") result = builder_->CreateFCmpOLT(left.val, right.val);
        else if (op == ">") result = builder_->CreateFCmpOGT(left.val, right.val);
        else if (op == "<=") result = builder_->CreateFCmpOLE(left.val, right.val);
        else if (op == ">=") result = builder_->CreateFCmpOGE(left.val, right.val);
    } else {
        if (op == "==") result = builder_->CreateICmpEQ(left.val, right.val);
        else if (op == "!=") result = builder_->CreateICmpNE(left.val, right.val);
        else if (op == "<") result = builder_->CreateICmpSLT(left.val, right.val);
        else if (op == ">") result = builder_->CreateICmpSGT(left.val, right.val);
        else if (op == "<=") result = builder_->CreateICmpSLE(left.val, right.val);
        else if (op == ">=") result = builder_->CreateICmpSGE(left.val, right.val);
    }
    return {result, CType::BOOL};
}

// ===== Let Bindings =====

TypedValue Codegen::codegen_let(LetExpr* node) {
    for (auto* alias : node->aliases) {
        if (auto* va = dynamic_cast<ValueAlias*>(alias)) {
            auto tv = codegen(va->expr);
            if (tv) named_values_[va->identifier->name->value] = tv;
        } else if (auto* la = dynamic_cast<LambdaAlias*>(alias)) {
            codegen_lambda_alias(la);
        } else if (auto* pa = dynamic_cast<PatternAlias*>(alias)) {
            auto tv = codegen(pa->expr);
            if (tv && tv.type == CType::TUPLE && tv.val->getType()->isStructTy()) {
                auto* tp = dynamic_cast<TuplePattern*>(pa->pattern);
                if (tp) {
                    for (size_t i = 0; i < tp->patterns.size(); i++) {
                        auto elem = builder_->CreateExtractValue(tv.val, {(unsigned)i});
                        CType et = (i < tv.subtypes.size()) ? tv.subtypes[i] : CType::INT;
                        auto* sub = tp->patterns[i];
                        if (sub->get_type() == AST_PATTERN_VALUE) {
                            auto* pv = static_cast<PatternValue*>(sub);
                            if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                                named_values_[(*id)->name->value] = {elem, et};
                        }
                    }
                }
            }
        }
    }
    return codegen(node->expr);
}

// ===== If Expression =====

TypedValue Codegen::codegen_if(IfExpr* node) {
    auto cond = auto_await(codegen(node->condition));
    if (!cond) return {};
    if (cond.type == CType::INT)
        cond.val = builder_->CreateICmpNE(cond.val, ConstantInt::get(LType::getInt64Ty(*context_), 0));

    auto fn = builder_->GetInsertBlock()->getParent();
    auto then_bb = BasicBlock::Create(*context_, "then", fn);
    auto else_bb = BasicBlock::Create(*context_, "else");
    auto merge_bb = BasicBlock::Create(*context_, "ifcont");
    builder_->CreateCondBr(cond.val, then_bb, else_bb);

    builder_->SetInsertPoint(then_bb);
    auto then_tv = codegen(node->thenExpr);
    if (!then_tv) return {};
    builder_->CreateBr(merge_bb);
    auto then_end = builder_->GetInsertBlock();

    fn->insert(fn->end(), else_bb);
    builder_->SetInsertPoint(else_bb);
    auto else_tv = codegen(node->elseExpr);
    if (!else_tv) return {};
    builder_->CreateBr(merge_bb);
    auto else_end = builder_->GetInsertBlock();

    fn->insert(fn->end(), merge_bb);
    builder_->SetInsertPoint(merge_bb);
    auto phi = builder_->CreatePHI(then_tv.val->getType(), 2);
    phi->addIncoming(then_tv.val, then_end);
    phi->addIncoming(else_tv.val, else_end);
    return {phi, then_tv.type, then_tv.subtypes};
}

// ===== Identifier =====

TypedValue Codegen::codegen_identifier(IdentifierExpr* node) {
    auto it = named_values_.find(node->name->value);
    if (it != named_values_.end()) {
        // If it's a FUNCTION with nullptr val, set last_lambda_name_ for higher-order support
        if (it->second.type == CType::FUNCTION && !it->second.val) {
            last_lambda_name_ = node->name->value;
        }
        return it->second;
    }
    // Check if it's a compiled function
    auto fit = compiled_functions_.find(node->name->value);
    if (fit != compiled_functions_.end()) return {fit->second.fn, CType::FUNCTION, {fit->second.return_type}};
    // Check if it's a deferred function (not yet compiled — referenced as value)
    auto def_it = deferred_functions_.find(node->name->value);
    if (def_it != deferred_functions_.end()) {
        last_lambda_name_ = node->name->value;
        return {nullptr, CType::FUNCTION};
    }
    // Check if it's a zero-arity ADT constructor
    auto adt_it = adt_constructors_.find(node->name->value);
    if (adt_it != adt_constructors_.end() && adt_it->second.arity == 0) {
        // Build the ADT struct type for this type: {i8, i64*max_arity}
        auto i8_ty = LType::getInt8Ty(*context_);
        auto i64_ty = LType::getInt64Ty(*context_);
        std::vector<LType*> fields = {i8_ty};
        for (int f = 0; f < adt_it->second.max_arity; f++) {
            fields.push_back(i64_ty);
        }
        auto* struct_type = StructType::get(*context_, fields);
        Value* val = UndefValue::get(struct_type);
        val = builder_->CreateInsertValue(val, ConstantInt::get(i8_ty, adt_it->second.tag), {0});
        return {val, CType::ADT};
    }
    // Check if it's a non-zero-arity ADT constructor (used as a function reference)
    if (adt_it != adt_constructors_.end()) {
        last_lambda_name_ = node->name->value;
        return {nullptr, CType::FUNCTION};
    }
    std::cerr << "Codegen: unknown variable '" << node->name->value << "'\n";
    return {};
}

// ===== Functions (deferred compilation) =====

TypedValue Codegen::codegen_function_def(FunctionExpr* node, const std::string& name) {
    std::string fn_name = name.empty() ? (node->name.empty() ? "lambda_" + std::to_string(lambda_counter_++) : node->name) : name;

    // Extract parameter names and collect bound variables from patterns.
    // For simple variable patterns: param name is the variable name.
    // For complex patterns (tuples, sequences): param name is synthetic,
    // and the pattern's variables are added to the bound set for free var analysis.
    DeferredFunction def;
    def.ast = node;
    std::unordered_set<std::string> bound;
    for (size_t i = 0; i < node->patterns.size(); i++) {
        std::string pname = "arg" + std::to_string(i);
        auto* pat = node->patterns[i];
        if (pat->get_type() == AST_PATTERN_VALUE) {
            auto* pv = static_cast<PatternValue*>(pat);
            if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                pname = (*id)->name->value;
                bound.insert(pname);
            }
        } else if (pat->get_type() == AST_TUPLE_PATTERN) {
            // Collect variable names from tuple elements
            auto* tp = static_cast<TuplePattern*>(pat);
            for (auto* elem : tp->patterns) {
                if (elem->get_type() == AST_PATTERN_VALUE) {
                    auto* pv = static_cast<PatternValue*>(elem);
                    if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                        bound.insert((*id)->name->value);
                }
            }
        } else if (pat->get_type() == AST_HEAD_TAILS_PATTERN) {
            auto* hp = static_cast<HeadTailsPattern*>(pat);
            for (auto* head : hp->heads) {
                if (head->get_type() == AST_PATTERN_VALUE) {
                    auto* pv = static_cast<PatternValue*>(head);
                    if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                        bound.insert((*id)->name->value);
                }
            }
            if (hp->tail && hp->tail->get_type() == AST_PATTERN_VALUE) {
                auto* pv = static_cast<PatternValue*>(hp->tail);
                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                    bound.insert((*id)->name->value);
            }
        }
        def.param_names.push_back(pname);
        bound.insert(pname);
    }
    std::unordered_set<std::string> fv_set;
    if (!node->bodies.empty()) {
        auto* body = node->bodies[0];
        if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body))
            collect_free_vars(bwg->expr, bound, fv_set);
    }
    for (auto& v : fv_set) {
        auto it = named_values_.find(v);
        if (it != named_values_.end()) {
            // Skip known functions (compiled or deferred) — they're global symbols.
            // But DO capture function pointers (runtime values: parameters, closures).
            if (it->second.type == CType::FUNCTION) {
                // Compiled function: skip (it's a global LLVM function)
                if (it->second.val && isa<Function>(it->second.val))
                    continue;
                // Deferred function: skip (will be compiled later as a global)
                if (deferred_functions_.count(v) > 0)
                    continue;
                // Runtime function pointer (parameter, closure): capture it
            }
            def.free_vars.push_back(v);
        }
    }

    deferred_functions_[fn_name] = def;
    last_lambda_name_ = fn_name;
    return {nullptr, CType::FUNCTION}; // no value yet, compiled at call site
}

TypedValue Codegen::codegen_lambda_alias(LambdaAlias* node) {
    auto fn_name = node->name->value;
    codegen_function_def(node->lambda, fn_name);
    named_values_[fn_name] = {nullptr, CType::FUNCTION};
    last_lambda_name_ = fn_name;
    return {nullptr, CType::FUNCTION};
}

Codegen::CompiledFunction Codegen::compile_function(
    const std::string& name, const DeferredFunction& def, const std::vector<TypedValue>& args) {

    // Build parameter types from actual argument types.
    // For complex types (TUPLE, SEQ, STRING), use the actual LLVM type from args
    // since llvm_type() only returns the generic fallback (i64 for TUPLE).
    std::vector<LType*> param_types;
    std::vector<CType> param_ctypes;
    for (size_t i = 0; i < def.param_names.size(); i++) {
        CType ct = (i < args.size()) ? args[i].type : CType::INT;
        if (i < args.size() && args[i].val && args[i].val->getType() != llvm_type(ct)) {
            // Use the actual LLVM type from the argument (e.g., struct for TUPLE)
            param_types.push_back(args[i].val->getType());
        } else {
            param_types.push_back(llvm_type(ct));
        }
        param_ctypes.push_back(ct);
    }

    // Add free variable types
    std::vector<TypedValue> capture_values;
    for (auto& fv : def.free_vars) {
        auto it = named_values_.find(fv);
        if (it != named_values_.end()) {
            param_types.push_back(llvm_type(it->second.type));
            param_ctypes.push_back(it->second.type);
            capture_values.push_back(it->second);
        }
    }

    // Pre-infer return type from body structure to avoid type mismatches
    // during first compilation pass (especially for recursive functions).
    // Walks the AST to determine the type of the first evaluable expression.
    std::function<CType(AstNode*)> infer_expr_type = [&](AstNode* node) -> CType {
        if (!node) return CType::INT;
        auto ct = node->get_type();
        if (ct == AST_VALUES_SEQUENCE_EXPR) return CType::SEQ;
        if (ct == AST_SET_EXPR) return CType::SET;
        if (ct == AST_DICT_EXPR) return CType::DICT;
        if (ct == AST_TUPLE_EXPR) return CType::TUPLE;
        if (ct == AST_SYMBOL_EXPR) return CType::SYMBOL;
        if (ct == AST_STRING_EXPR) return CType::STRING;
        if (ct == AST_INTEGER_EXPR) return CType::INT;
        if (ct == AST_FLOAT_EXPR) return CType::FLOAT;
        if (ct == AST_TRUE_LITERAL_EXPR || ct == AST_FALSE_LITERAL_EXPR) return CType::BOOL;
        if (ct == AST_CONS_LEFT_EXPR || ct == AST_CONS_RIGHT_EXPR || ct == AST_JOIN_EXPR) return CType::SEQ;
        if (ct == AST_IDENTIFIER_EXPR) {
            auto* id = static_cast<IdentifierExpr*>(node);
            for (size_t pi = 0; pi < def.param_names.size(); pi++) {
                if (def.param_names[pi] == id->name->value && pi < args.size())
                    return args[pi].type;
            }
        }
        // Recurse through wrappers
        if (ct == AST_CASE_EXPR) {
            auto* ce = static_cast<CaseExpr*>(node);
            if (!ce->clauses.empty()) return infer_expr_type(ce->clauses[0]->body);
        }
        if (ct == AST_LET_EXPR) {
            auto* le = static_cast<LetExpr*>(node);
            return infer_expr_type(le->expr);
        }
        if (ct == AST_IF_EXPR) {
            auto* ie = static_cast<IfExpr*>(node);
            return infer_expr_type(ie->thenExpr);
        }
        return CType::INT;
    };

    CType preliminary_ret = CType::INT;
    if (!def.ast->bodies.empty()) {
        auto* body = def.ast->bodies[0];
        if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body))
            preliminary_ret = infer_expr_type(bwg->expr);
    }

    auto ret_type = llvm_type(preliminary_ret);
    auto fn_type = llvm::FunctionType::get(ret_type, param_types, false);
    auto fn = Function::Create(fn_type, Function::InternalLinkage, name, module_.get());

    // Register immediately so recursive calls find this function
    CompiledFunction cf_preliminary;
    cf_preliminary.fn = fn;
    cf_preliminary.return_type = preliminary_ret;
    cf_preliminary.param_types = param_ctypes;
    cf_preliminary.capture_names = def.free_vars;
    compiled_functions_[name] = cf_preliminary;
    named_values_[name] = {fn, CType::FUNCTION};

    // Save state
    auto saved_block = builder_->GetInsertBlock();
    auto saved_point = builder_->GetInsertPoint();
    auto saved_values = named_values_;

    auto entry = BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(entry);

    // Bind parameters
    named_values_.clear();
    // Keep function references from outer scope
    for (auto& [k, v] : saved_values) {
        if (v.type == CType::FUNCTION) named_values_[k] = v;
    }
    // Also keep deferred function knowledge
    named_values_[name] = {fn, CType::FUNCTION};

    size_t i = 0;
    for (auto& arg : fn->args()) {
        std::string pname;
        CType ct;
        std::vector<CType> st;
        if (i < def.param_names.size()) {
            pname = def.param_names[i];
            ct = (i < args.size()) ? args[i].type : CType::INT;
            if (i < args.size()) st = args[i].subtypes;
            // For FUNCTION args, look up return type from compiled function
            if (ct == CType::FUNCTION && st.empty() && i < args.size() && args[i].val) {
                if (auto* fn_val = dyn_cast<Function>(args[i].val)) {
                    auto cf_it = compiled_functions_.find(fn_val->getName().str());
                    if (cf_it != compiled_functions_.end())
                        st = {cf_it->second.return_type};
                }
            }
        } else {
            pname = def.free_vars[i - def.param_names.size()];
            size_t ci = i - def.param_names.size();
            ct = (ci < capture_values.size()) ? capture_values[ci].type : CType::INT;
        }
        arg.setName(pname);
        named_values_[pname] = {&arg, ct, st};

        // Destructure complex patterns (tuples, sequences) into element variables
        if (i < def.ast->patterns.size()) {
            auto* pat = def.ast->patterns[i];
            if (ct == CType::TUPLE && pat->get_type() == AST_TUPLE_PATTERN) {
                auto* tp = static_cast<TuplePattern*>(pat);
                for (size_t ti = 0; ti < tp->patterns.size(); ti++) {
                    auto* elem_pat = tp->patterns[ti];
                    if (elem_pat->get_type() == AST_PATTERN_VALUE) {
                        auto* pv = static_cast<PatternValue*>(elem_pat);
                        if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                            CType et = (ti < st.size()) ? st[ti] : CType::INT;
                            auto* elem = builder_->CreateExtractValue(&arg, {(unsigned)ti});
                            named_values_[(*id)->name->value] = {elem, et};
                        }
                    }
                    // Underscore patterns are silently ignored
                }
            }
        }

        i++;
    }

    // Compile body
    TypedValue body_tv;
    if (!def.ast->bodies.empty()) {
        auto* body = def.ast->bodies[0];
        if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body))
            body_tv = codegen(bwg->expr);
    }

    CType ret_ctype = body_tv ? body_tv.type : CType::INT;

    if (body_tv && body_tv.val) {
        // If body type doesn't match return type, we need to recreate the function
        if (body_tv.val->getType() != ret_type) {
            // Remove the function and recreate with correct return type
            fn->eraseFromParent();
            auto new_fn_type = llvm::FunctionType::get(body_tv.val->getType(), param_types, false);
            fn = Function::Create(new_fn_type, Function::InternalLinkage, name, module_.get());

            // Recompile
            named_values_.clear();
            for (auto& [k, v] : saved_values) {
                if (v.type == CType::FUNCTION) named_values_[k] = v;
            }
            named_values_[name] = {fn, CType::FUNCTION};

            auto new_entry = BasicBlock::Create(*context_, "entry", fn);
            builder_->SetInsertPoint(new_entry);

            i = 0;
            for (auto& arg : fn->args()) {
                std::string pname;
                CType ct;
                std::vector<CType> st;
                if (i < def.param_names.size()) {
                    pname = def.param_names[i];
                    ct = (i < args.size()) ? args[i].type : CType::INT;
                    if (i < args.size()) st = args[i].subtypes;
                    if (ct == CType::FUNCTION && st.empty() && i < args.size() && args[i].val) {
                        if (auto* fn_val = dyn_cast<Function>(args[i].val)) {
                            auto cf_it2 = compiled_functions_.find(fn_val->getName().str());
                            if (cf_it2 != compiled_functions_.end())
                                st = {cf_it2->second.return_type};
                        }
                    }
                } else {
                    pname = def.free_vars[i - def.param_names.size()];
                    size_t ci = i - def.param_names.size();
                    ct = (ci < capture_values.size()) ? capture_values[ci].type : CType::INT;
                }
                arg.setName(pname);
                named_values_[pname] = {&arg, ct, st};

                // Destructure complex patterns (same as first pass)
                if (i < def.ast->patterns.size()) {
                    auto* pat = def.ast->patterns[i];
                    if (ct == CType::TUPLE && pat->get_type() == AST_TUPLE_PATTERN) {
                        auto* tp = static_cast<TuplePattern*>(pat);
                        for (size_t ti = 0; ti < tp->patterns.size(); ti++) {
                            auto* elem_pat = tp->patterns[ti];
                            if (elem_pat->get_type() == AST_PATTERN_VALUE) {
                                auto* pv = static_cast<PatternValue*>(elem_pat);
                                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                                    CType et = (ti < st.size()) ? st[ti] : CType::INT;
                                    auto* elem = builder_->CreateExtractValue(&arg, {(unsigned)ti});
                                    named_values_[(*id)->name->value] = {elem, et};
                                }
                            }
                        }
                    }
                }

                i++;
            }

            body_tv = {};
            if (!def.ast->bodies.empty()) {
                auto* body = def.ast->bodies[0];
                if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body))
                    body_tv = codegen(bwg->expr);
            }
            ret_ctype = body_tv ? body_tv.type : CType::INT;
        }
        builder_->CreateRet(body_tv.val);
    } else {
        builder_->CreateRet(ConstantInt::get(ret_type, 0));
    }

    // Restore
    named_values_ = saved_values;
    if (saved_block) builder_->SetInsertPoint(saved_block, saved_point);

    CompiledFunction cf;
    cf.fn = fn;
    cf.return_type = ret_ctype;
    cf.param_types = param_ctypes;
    cf.capture_names = def.free_vars;
    compiled_functions_[name] = cf;
    named_values_[name] = {fn, CType::FUNCTION};

    return cf;
}

TypedValue Codegen::codegen_apply(ApplyExpr* node) {
    // Flatten juxtaposition chain: f x y → collect all args and root name
    std::vector<ApplyExpr*> chain;
    std::string fn_name;
    ApplyExpr* cur = node;
    while (cur) {
        chain.push_back(cur);
        if (auto* nc = dynamic_cast<NameCall*>(cur->call)) {
            fn_name = nc->name->value;
            break;
        } else if (auto* ec = dynamic_cast<ExprCall*>(cur->call)) {
            if (auto* inner = dynamic_cast<ApplyExpr*>(ec->expr)) cur = inner;
            else break;
        } else break;
    }

    // Evaluate all arguments in order, tracking lambda names for FUNCTION args
    std::vector<TypedValue> all_args;
    std::vector<std::string> arg_lambda_names; // deferred lambda name per arg (empty if not a lambda)
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        for (auto& a : (*it)->args) {
            last_lambda_name_.clear();
            TypedValue tv;
            if (std::holds_alternative<ExprNode*>(a)) tv = codegen(std::get<ExprNode*>(a));
            else tv = codegen(std::get<ValueExpr*>(a));
            // FUNCTION args may have nullptr val (deferred compilation) — that's OK
            if (tv.type != CType::FUNCTION && !tv) return {};
            // Auto-await PROMISE args (functions expect concrete values)
            if (tv.type == CType::PROMISE) tv = auto_await(tv);
            all_args.push_back(tv);
            arg_lambda_names.push_back(last_lambda_name_);
        }
    }

    // Pre-compile any FUNCTION args that have nullptr val (lambdas passed as values)
    for (size_t ai = 0; ai < all_args.size(); ai++) {
        if (all_args[ai].type == CType::FUNCTION && !all_args[ai].val && !arg_lambda_names[ai].empty()) {
            auto& lname = arg_lambda_names[ai];
            auto def_it = deferred_functions_.find(lname);
            if (def_it != deferred_functions_.end()) {
                // Compile with INT args as default type hint
                std::vector<TypedValue> hint_args;
                for (size_t pi = 0; pi < def_it->second.param_names.size(); pi++)
                    hint_args.push_back({ConstantInt::get(LType::getInt64Ty(*context_), 0), CType::INT});
                auto cf = compile_function(lname, def_it->second, hint_args);
                all_args[ai] = {cf.fn, CType::FUNCTION, {cf.return_type}};
            }
        }
    }

    // Check if it's an ADT constructor call
    auto adt_it = adt_constructors_.find(fn_name);
    if (adt_it != adt_constructors_.end() && adt_it->second.arity > 0) {
        auto& info = adt_it->second;
        auto i8_ty = LType::getInt8Ty(*context_);
        auto i64_ty = LType::getInt64Ty(*context_);
        // Build struct type: {i8, i64*max_arity}
        std::vector<LType*> fields = {i8_ty};
        for (int f = 0; f < info.max_arity; f++) {
            fields.push_back(i64_ty);
        }
        auto* struct_type = StructType::get(*context_, fields);
        Value* val = UndefValue::get(struct_type);
        val = builder_->CreateInsertValue(val, ConstantInt::get(i8_ty, info.tag), {0});
        for (size_t ai = 0; ai < all_args.size() && ai < (size_t)info.arity; ai++) {
            Value* arg_val = all_args[ai].val;
            // Cast to i64 if needed
            if (arg_val->getType() != i64_ty) {
                if (arg_val->getType()->isIntegerTy())
                    arg_val = builder_->CreateZExtOrTrunc(arg_val, i64_ty);
                else if (arg_val->getType()->isDoubleTy())
                    arg_val = builder_->CreateBitCast(arg_val, i64_ty);
                else if (arg_val->getType()->isPointerTy())
                    arg_val = builder_->CreatePtrToInt(arg_val, i64_ty);
                else if (arg_val->getType()->isStructTy()) {
                    // For struct values (e.g. nested ADTs), store to alloca and load as i64
                    auto* alloca = builder_->CreateAlloca(arg_val->getType());
                    builder_->CreateStore(arg_val, alloca);
                    auto* cast_ptr = builder_->CreateBitCast(alloca, PointerType::get(i64_ty, 0));
                    arg_val = builder_->CreateLoad(i64_ty, cast_ptr);
                }
            }
            val = builder_->CreateInsertValue(val, arg_val, {(unsigned)(ai + 1)});
        }
        return {val, CType::ADT};
    }

    // Find or compile the function
    // First try direct lookup, then resolve through named_values_ indirection
    auto cf_it = compiled_functions_.find(fn_name);
    if (cf_it == compiled_functions_.end()) {
        // Check if fn_name is an alias for a compiled function (e.g., let g = partial_fn)
        auto nv_it = named_values_.find(fn_name);
        if (nv_it != named_values_.end() && nv_it->second.type == CType::FUNCTION && nv_it->second.val) {
            if (auto* llvm_fn = dyn_cast<Function>(nv_it->second.val)) {
                // Find by LLVM function name
                cf_it = compiled_functions_.find(llvm_fn->getName().str());
            }
        }
    }
    if (cf_it == compiled_functions_.end()) {
        // Check deferred
        auto def_it = deferred_functions_.find(fn_name);
        if (def_it != deferred_functions_.end()) {
            compile_function(fn_name, def_it->second, all_args);
            cf_it = compiled_functions_.find(fn_name);
        }
    }

    if (cf_it == compiled_functions_.end()) {
        // Check if fn_name is a FUNCTION-typed variable (higher-order: indirect call)
        auto var_it = named_values_.find(fn_name);
        if (var_it != named_values_.end() && var_it->second.type == CType::FUNCTION && var_it->second.val) {
            // Build function type from actual argument types
            std::vector<LType*> arg_types;
            for (auto& a : all_args) arg_types.push_back(a.val->getType());
            // Return type from subtypes[0] if available, else match first arg type
            CType ret_ctype = (!var_it->second.subtypes.empty()) ? var_it->second.subtypes[0]
                : (all_args.empty() ? CType::INT : all_args[0].type);
            auto ret_llvm = llvm_type(ret_ctype);
            auto fn_type = llvm::FunctionType::get(ret_llvm, arg_types, false);
            auto fn_ptr = var_it->second.val;
            std::vector<Value*> vals;
            for (auto& a : all_args) vals.push_back(a.val);
            auto result = builder_->CreateCall(fn_type, fn_ptr, vals, "indirect_call");
            return {result, ret_ctype};
        }

        // Check if it's an imported extern function
        auto ext_it = extern_functions_.find(fn_name);
        if (ext_it != extern_functions_.end()) {
            std::string mangled = ext_it->second;

            // Try to get return type from module metadata
            CType ret_ctype = CType::INT; // default
            auto meta_it = module_meta_.find(mangled);
            if (meta_it != module_meta_.end()) {
                ret_ctype = meta_it->second.return_type;
            } else {
                // Check CFFI signatures
                auto cffi_it = cffi_signatures_.find(mangled);
                if (cffi_it != cffi_signatures_.end())
                    ret_ctype = cffi_it->second.return_type;
            }

            // Declare the extern function with the correct signature
            std::vector<LType*> arg_types;
            for (auto& a : all_args) arg_types.push_back(a.val->getType());
            auto ret_llvm = llvm_type(ret_ctype);
            auto fn_type = llvm::FunctionType::get(ret_llvm, arg_types, false);

            auto* ext_fn = module_->getFunction(mangled);
            if (!ext_fn) {
                ext_fn = Function::Create(fn_type, Function::ExternalLinkage,
                                           mangled, module_.get());
            }

            std::vector<Value*> vals;
            for (auto& a : all_args) vals.push_back(a.val);
            return {builder_->CreateCall(ext_fn, vals, "extern_call"), ret_ctype};
        }

        // Try as an LLVM function in the module
        auto* fn = module_->getFunction(fn_name);
        if (!fn) {
            std::cerr << "Codegen: unknown function '" << fn_name << "'\n";
            return {};
        }
        std::vector<Value*> vals;
        for (auto& a : all_args) vals.push_back(a.val);
        return {builder_->CreateCall(fn, vals), CType::INT};
    }

    auto& cf = cf_it->second;

    // Check for partial application: fewer args than arity
    size_t func_arity = cf.param_types.size() - cf.capture_names.size();
    if (all_args.size() < func_arity) {
        // Partial application: generate a wrapper function that captures the
        // provided args and takes the remaining ones as parameters.
        // add 5 where add(x,y) → generate partial_N(y) { return add(5, y); }

        size_t n_provided = all_args.size();
        size_t n_remaining = func_arity - n_provided;

        // Build the wrapper's parameter types (remaining params)
        std::vector<LType*> wrapper_params;
        for (size_t i = n_provided; i < func_arity; i++)
            wrapper_params.push_back(llvm_type(cf.param_types[i]));
        // Add captured params from original function
        for (size_t i = 0; i < cf.capture_names.size(); i++)
            wrapper_params.push_back(llvm_type(cf.param_types[func_arity + i]));
        // Add the partially applied args as captured params
        for (auto& a : all_args)
            wrapper_params.push_back(a.val->getType());

        auto ret_llvm = cf.fn->getReturnType();
        auto wrapper_type = llvm::FunctionType::get(ret_llvm, wrapper_params, false);
        std::string wrapper_name = fn_name + "_partial" + std::to_string(n_provided) + "of" + std::to_string(func_arity);
        auto* wrapper = Function::Create(wrapper_type, Function::InternalLinkage,
                                          wrapper_name, module_.get());

        // Save state
        auto saved_block = builder_->GetInsertBlock();
        auto saved_point = builder_->GetInsertPoint();

        auto* entry = BasicBlock::Create(*context_, "entry", wrapper);
        builder_->SetInsertPoint(entry);

        // Build call args: captured partial args + remaining params
        std::vector<Value*> inner_call_args;
        // First: the remaining params (from wrapper's parameters)
        size_t pi = 0;
        for (size_t i = 0; i < n_remaining; i++) {
            // These are the wrapper's first params — but we need to put
            // the partial args FIRST in the call, so collect remaining params
        }
        // Actually: original function expects (provided_args..., remaining_args..., captures...)
        // The wrapper's params are (remaining_args..., captures..., provided_args_captured...)

        // Reorder: provided args (from captured) + remaining args (from params) + captures
        auto arg_it = wrapper->arg_begin();

        // Skip to captured partial args (at the end of wrapper params)
        // Wrapper params layout: [remaining_args..., orig_captures..., partial_captured...]
        // For calling original: [partial_captured..., remaining_args..., orig_captures...]

        // Collect all wrapper args
        std::vector<Value*> wrapper_args_vec;
        for (auto& arg : wrapper->args()) wrapper_args_vec.push_back(&arg);

        // partial_captured are the last n_provided args of wrapper
        // remaining are the first n_remaining
        // orig_captures are in between

        size_t n_orig_captures = cf.capture_names.size();

        // The wrapper takes only the remaining args as parameters.
        // The partially applied args are embedded as constants in the wrapper body.
        wrapper_params.clear();
        for (size_t i = n_provided; i < func_arity; i++)
            wrapper_params.push_back(llvm_type(cf.param_types[i]));

        // Rebuild wrapper with only remaining params
        wrapper->eraseFromParent();
        wrapper_type = llvm::FunctionType::get(ret_llvm, wrapper_params, false);
        wrapper = Function::Create(wrapper_type, Function::InternalLinkage,
                                    wrapper_name, module_.get());

        entry = BasicBlock::Create(*context_, "entry", wrapper);
        builder_->SetInsertPoint(entry);

        // Build call to original function:
        // args = [captured_partial_args..., remaining_params..., orig_captures...]
        inner_call_args.clear();

        // First: the partially applied args (constants from the call site)
        for (auto& a : all_args)
            inner_call_args.push_back(a.val);

        // Then: the remaining params (wrapper's parameters)
        for (auto& arg : wrapper->args())
            inner_call_args.push_back(&arg);

        // Then: original function's captures (from enclosing scope)
        for (auto& cap_name : cf.capture_names) {
            auto cap_it = named_values_.find(cap_name);
            if (cap_it != named_values_.end())
                inner_call_args.push_back(cap_it->second.val);
        }

        auto* result = builder_->CreateCall(cf.fn, inner_call_args, "partial_call");
        builder_->CreateRet(result);

        // Restore
        builder_->SetInsertPoint(saved_block, saved_point);

        // Register wrapper — no captures needed (args are embedded as constants)
        CompiledFunction partial_cf;
        partial_cf.fn = wrapper;
        partial_cf.return_type = cf.return_type;
        for (size_t i = n_provided; i < func_arity; i++)
            partial_cf.param_types.push_back(cf.param_types[i]);

        compiled_functions_[wrapper_name] = partial_cf;
        named_values_[wrapper_name] = {wrapper, CType::FUNCTION};
        return {wrapper, CType::FUNCTION};
    }

    std::vector<Value*> call_args;
    for (auto& a : all_args) call_args.push_back(a.val);

    // Append capture values
    for (auto& cap_name : cf.capture_names) {
        auto it = named_values_.find(cap_name);
        if (it != named_values_.end()) call_args.push_back(it->second.val);
    }

    // Async function: wrap in yona_rt_async_call → returns PROMISE
    if (cf.return_type == CType::PROMISE) {
        CType inner_ret = CType::INT;
        auto nv_it = named_values_.find(fn_name);
        if (nv_it != named_values_.end() && !nv_it->second.subtypes.empty())
            inner_ret = nv_it->second.subtypes[0];
        auto promise = builder_->CreateCall(rt_async_call_, {cf.fn, call_args[0]}, "async_call");
        return {promise, CType::PROMISE, {inner_ret}};
    }

    return {builder_->CreateCall(cf.fn, call_args, "calltmp"), cf.return_type};
}

// ===== Case Expression =====

TypedValue Codegen::codegen_case(CaseExpr* node) {
    auto scrutinee = auto_await(codegen(node->expr));
    if (!scrutinee) return {};

    auto fn = builder_->GetInsertBlock()->getParent();
    auto merge_bb = BasicBlock::Create(*context_, "case.end");
    std::vector<std::pair<TypedValue, BasicBlock*>> results;

    for (size_t i = 0; i < node->clauses.size(); i++) {
        auto* clause = node->clauses[i];
        auto* pat = clause->pattern;
        auto body_bb = BasicBlock::Create(*context_, "case.body." + std::to_string(i), fn);
        auto next_bb = (i + 1 < node->clauses.size())
            ? BasicBlock::Create(*context_, "case.next." + std::to_string(i+1), fn)
            : merge_bb;

        bool body_inline = false;

        if (pat->get_type() == AST_UNDERSCORE_PATTERN) {
            builder_->CreateBr(body_bb);
        } else if (pat->get_type() == AST_PATTERN_VALUE) {
            auto* pv = static_cast<PatternValue*>(pat);
            if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                named_values_[(*id)->name->value] = scrutinee;
                builder_->CreateBr(body_bb);
            } else if (auto* sym = std::get_if<SymbolExpr*>(&pv->expr)) {
                // Symbol comparison: integer equality on interned IDs
                int64_t sym_id = intern_symbol((*sym)->value);
                auto sym_val = ConstantInt::get(LType::getInt64Ty(*context_), sym_id);
                auto cmp = builder_->CreateICmpEQ(scrutinee.val, sym_val);
                builder_->CreateCondBr(cmp, body_bb, next_bb);
            } else if (auto* lit = std::get_if<LiteralExpr<void*>*>(&pv->expr)) {
                auto* an = reinterpret_cast<AstNode*>(*lit);
                if (an->get_type() == AST_INTEGER_EXPR) {
                    auto* ie = static_cast<IntegerExpr*>(an);
                    auto mv = ConstantInt::get(LType::getInt64Ty(*context_), ie->value);
                    auto cmp = builder_->CreateICmpEQ(scrutinee.val, mv);
                    builder_->CreateCondBr(cmp, body_bb, next_bb);
                } else builder_->CreateBr(body_bb);
            } else builder_->CreateBr(body_bb);
        } else if (pat->get_type() == AST_HEAD_TAILS_PATTERN) {
            auto* htp = static_cast<HeadTailsPattern*>(pat);
            auto len = builder_->CreateCall(rt_seq_length_, {scrutinee.val});
            auto min_len = ConstantInt::get(LType::getInt64Ty(*context_), htp->heads.size());
            builder_->CreateCondBr(builder_->CreateICmpSGE(len, min_len), body_bb, next_bb);

            builder_->SetInsertPoint(body_bb);
            for (size_t hi = 0; hi < htp->heads.size(); hi++) {
                auto idx = ConstantInt::get(LType::getInt64Ty(*context_), hi);
                auto hv = builder_->CreateCall(rt_seq_get_, {scrutinee.val, idx});
                auto* hp = htp->heads[hi];
                if (hp->get_type() == AST_PATTERN_VALUE) {
                    auto* pv = static_cast<PatternValue*>(hp);
                    if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                        named_values_[(*id)->name->value] = {hv, CType::INT};
                }
            }
            if (htp->tail && htp->tail->get_type() == AST_PATTERN_VALUE) {
                auto tv = builder_->CreateCall(rt_seq_tail_, {scrutinee.val});
                auto* pv = static_cast<PatternValue*>(htp->tail);
                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                    named_values_[(*id)->name->value] = {tv, CType::SEQ};
            }
            body_inline = true;
        } else if (pat->get_type() == AST_SEQ_PATTERN) {
            auto* sp = static_cast<SeqPattern*>(pat);
            if (sp->patterns.empty()) {
                auto len = builder_->CreateCall(rt_seq_length_, {scrutinee.val});
                auto cmp = builder_->CreateICmpEQ(len, ConstantInt::get(LType::getInt64Ty(*context_), 0));
                builder_->CreateCondBr(cmp, body_bb, next_bb);
            } else builder_->CreateBr(body_bb);
        } else if (pat->get_type() == AST_TUPLE_PATTERN) {
            auto* tp = static_cast<TuplePattern*>(pat);
            for (size_t ti = 0; ti < tp->patterns.size(); ti++) {
                auto elem = builder_->CreateExtractValue(scrutinee.val, {(unsigned)ti});
                CType et = (ti < scrutinee.subtypes.size()) ? scrutinee.subtypes[ti] : CType::INT;
                auto* sub = tp->patterns[ti];
                if (sub->get_type() == AST_PATTERN_VALUE) {
                    auto* pv = static_cast<PatternValue*>(sub);
                    if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                        named_values_[(*id)->name->value] = {elem, et};
                }
            }
            builder_->CreateBr(body_bb);
        } else if (pat->get_type() == AST_OR_PATTERN) {
            auto* op = static_cast<OrPattern*>(pat);
            // For each alternative in the or-pattern, test and branch to body_bb on match
            for (size_t oi = 0; oi < op->patterns.size(); oi++) {
                auto* alt = op->patterns[oi].get();
                auto alt_next = (oi + 1 < op->patterns.size())
                    ? BasicBlock::Create(*context_, "case.or." + std::to_string(i) + "." + std::to_string(oi+1), fn)
                    : next_bb;

                if (alt->get_type() == AST_UNDERSCORE_PATTERN) {
                    builder_->CreateBr(body_bb);
                } else if (alt->get_type() == AST_PATTERN_VALUE) {
                    auto* pv = static_cast<PatternValue*>(alt);
                    if (auto* sym = std::get_if<SymbolExpr*>(&pv->expr)) {
                        int64_t sym_id = intern_symbol((*sym)->value);
                        auto sym_val = ConstantInt::get(LType::getInt64Ty(*context_), sym_id);
                        auto cmp = builder_->CreateICmpEQ(scrutinee.val, sym_val);
                        builder_->CreateCondBr(cmp, body_bb, alt_next);
                    } else if (auto* lit = std::get_if<LiteralExpr<void*>*>(&pv->expr)) {
                        auto* an = reinterpret_cast<AstNode*>(*lit);
                        if (an->get_type() == AST_INTEGER_EXPR) {
                            auto* ie = static_cast<IntegerExpr*>(an);
                            auto mv = ConstantInt::get(LType::getInt64Ty(*context_), ie->value);
                            auto cmp = builder_->CreateICmpEQ(scrutinee.val, mv);
                            builder_->CreateCondBr(cmp, body_bb, alt_next);
                        } else {
                            builder_->CreateBr(body_bb);
                        }
                    } else if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                        named_values_[(*id)->name->value] = scrutinee;
                        builder_->CreateBr(body_bb);
                    } else {
                        builder_->CreateBr(body_bb);
                    }
                } else {
                    builder_->CreateBr(body_bb);
                }

                if (oi + 1 < op->patterns.size())
                    builder_->SetInsertPoint(alt_next);
            }
        } else if (pat->get_type() == AST_CONSTRUCTOR_PATTERN) {
            auto* cp = static_cast<ConstructorPattern*>(pat);
            auto ctor_it = adt_constructors_.find(cp->constructor_name);
            if (ctor_it != adt_constructors_.end()) {
                int8_t tag = static_cast<int8_t>(ctor_it->second.tag);
                // Extract tag from scrutinee (field 0 of the struct)
                auto scr_tag = builder_->CreateExtractValue(scrutinee.val, {0});
                auto tag_val = ConstantInt::get(LType::getInt8Ty(*context_), tag);
                auto cmp = builder_->CreateICmpEQ(scr_tag, tag_val);
                builder_->CreateCondBr(cmp, body_bb, next_bb);

                builder_->SetInsertPoint(body_bb);
                // Bind sub-patterns to payload fields
                for (size_t fi = 0; fi < cp->sub_patterns.size(); fi++) {
                    auto field_val = builder_->CreateExtractValue(scrutinee.val, {(unsigned)(fi + 1)});
                    auto* sub_pat = cp->sub_patterns[fi];
                    if (sub_pat->get_type() == AST_PATTERN_VALUE) {
                        auto* pv = static_cast<PatternValue*>(sub_pat);
                        if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                            named_values_[(*id)->name->value] = {field_val, CType::INT};
                    } else if (sub_pat->get_type() == AST_UNDERSCORE_PATTERN) {
                        // Wildcard — ignore this field
                    }
                }
                body_inline = true;
            } else {
                builder_->CreateBr(body_bb);
            }
        } else {
            builder_->CreateBr(body_bb);
        }

        if (!body_inline) builder_->SetInsertPoint(body_bb);
        auto body_tv = codegen(clause->body);
        if (!body_tv) return {};
        builder_->CreateBr(merge_bb);
        results.push_back({body_tv, builder_->GetInsertBlock()});

        if (i + 1 < node->clauses.size() && next_bb != merge_bb)
            builder_->SetInsertPoint(next_bb);
    }

    fn->insert(fn->end(), merge_bb);
    builder_->SetInsertPoint(merge_bb);
    if (results.empty()) return {};

    unsigned pred_count = 0;
    for (auto it = pred_begin(merge_bb); it != pred_end(merge_bb); ++it) pred_count++;

    auto phi = builder_->CreatePHI(results[0].first.val->getType(), pred_count);
    for (auto& [tv, bb] : results) phi->addIncoming(tv.val, bb);
    for (auto it = pred_begin(merge_bb); it != pred_end(merge_bb); ++it) {
        bool found = false;
        for (auto& [tv, bb] : results) if (bb == *it) { found = true; break; }
        if (!found) phi->addIncoming(Constant::getNullValue(results[0].first.val->getType()), *it);
    }
    return {phi, results[0].first.type, results[0].first.subtypes};
}

// ===== Do Expression =====

TypedValue Codegen::codegen_do(DoExpr* node) {
    TypedValue last;
    for (auto* step : node->steps) last = codegen(step);
    return last ? last : TypedValue{ConstantInt::get(LType::getInt64Ty(*context_), 0), CType::INT};
}

// ===== Collections =====

TypedValue Codegen::codegen_tuple(TupleExpr* node) {
    std::vector<Value*> elems;
    std::vector<CType> subtypes;
    std::vector<LType*> llvm_types;

    for (auto* v : node->values) {
        auto tv = codegen(v);
        if (!tv) return {};
        elems.push_back(tv.val);
        subtypes.push_back(tv.type);
        llvm_types.push_back(tv.val->getType());
    }

    auto struct_type = StructType::get(*context_, llvm_types);
    Value* tuple = UndefValue::get(struct_type);
    for (size_t i = 0; i < elems.size(); i++)
        tuple = builder_->CreateInsertValue(tuple, elems[i], {(unsigned)i});

    return {tuple, CType::TUPLE, subtypes};
}

TypedValue Codegen::codegen_seq(ValuesSequenceExpr* node) {
    size_t n = node->values.size();
    auto count = ConstantInt::get(LType::getInt64Ty(*context_), n);
    auto seq = builder_->CreateCall(rt_seq_alloc_, {count}, "seq");

    CType elem_type = CType::INT; // default
    for (size_t i = 0; i < n; i++) {
        auto tv = codegen(node->values[i]);
        if (!tv) return {};
        if (i == 0) elem_type = tv.type;
        auto idx = ConstantInt::get(LType::getInt64Ty(*context_), i);
        builder_->CreateCall(rt_seq_set_, {seq, idx, tv.val});
    }
    return {seq, CType::SEQ, {elem_type}};
}

TypedValue Codegen::codegen_set(SetExpr* node) {
    size_t n = node->values.size();
    auto count = ConstantInt::get(LType::getInt64Ty(*context_), n);
    auto set = builder_->CreateCall(rt_set_alloc_, {count}, "set");

    CType elem_type = CType::INT;
    for (size_t i = 0; i < n; i++) {
        auto tv = codegen(node->values[i]);
        if (!tv) return {};
        if (i == 0) elem_type = tv.type;
        auto idx = ConstantInt::get(LType::getInt64Ty(*context_), i);
        builder_->CreateCall(rt_set_put_, {set, idx, tv.val});
    }
    return {set, CType::SET, {elem_type}};
}

TypedValue Codegen::codegen_dict(DictExpr* node) {
    size_t n = node->values.size();
    auto count = ConstantInt::get(LType::getInt64Ty(*context_), n);
    auto dict = builder_->CreateCall(rt_dict_alloc_, {count}, "dict");

    CType key_type = CType::INT, val_type = CType::INT;
    for (size_t i = 0; i < n; i++) {
        auto key_tv = codegen(node->values[i].first);
        auto val_tv = codegen(node->values[i].second);
        if (!key_tv || !val_tv) return {};
        if (i == 0) { key_type = key_tv.type; val_type = val_tv.type; }
        auto idx = ConstantInt::get(LType::getInt64Ty(*context_), i);
        builder_->CreateCall(rt_dict_set_, {dict, idx, key_tv.val, val_tv.val});
    }
    return {dict, CType::DICT, {key_type, val_type}};
}

TypedValue Codegen::codegen_cons(ConsLeftExpr* node) {
    auto elem = codegen(node->left);
    auto seq = codegen(node->right);
    if (!elem || !seq) return {};
    return {builder_->CreateCall(rt_seq_cons_, {elem.val, seq.val}, "cons"), CType::SEQ, {elem.type}};
}

TypedValue Codegen::codegen_join(JoinExpr* node) {
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return {};

    if (left.type == CType::SEQ && right.type == CType::SEQ)
        return {builder_->CreateCall(rt_seq_join_, {left.val, right.val}), CType::SEQ};
    if (left.type == CType::STRING || right.type == CType::STRING)
        return {builder_->CreateCall(rt_string_concat_, {left.val, right.val}), CType::STRING};
    return {};
}

// ===== Extern Declarations =====

static CType yona_type_to_ctype(const types::Type& t) {
    if (std::holds_alternative<types::BuiltinType>(t)) {
        switch (std::get<types::BuiltinType>(t)) {
            case types::SignedInt64: case types::SignedInt32:
            case types::SignedInt16: case types::SignedInt128:
            case types::UnsignedInt64: case types::UnsignedInt32:
            case types::UnsignedInt16: case types::UnsignedInt128:
                return CType::INT;
            case types::Float64: case types::Float32: case types::Float128:
                return CType::FLOAT;
            case types::Bool: return CType::BOOL;
            case types::String: return CType::STRING;
            case types::Unit: return CType::UNIT;
            default: return CType::INT;
        }
    }
    return CType::INT;
}

// Auto-await: if a TypedValue is PROMISE, insert yona_rt_async_await to get the actual value
TypedValue Codegen::auto_await(TypedValue tv) {
    if (!tv || tv.type != CType::PROMISE) return tv;
    auto awaited = builder_->CreateCall(rt_async_await_, {tv.val}, "await");
    // The awaited value's type is stored in subtypes[0]
    CType inner = (!tv.subtypes.empty()) ? tv.subtypes[0] : CType::INT;
    return {awaited, inner};
}

TypedValue Codegen::codegen_extern_decl(ExternDeclExpr* node) {
    // Extract parameter types and return type from the declared type
    std::vector<LType*> param_types;
    std::vector<CType> param_ctypes;
    CType ret_ctype = CType::INT;

    auto current_type = node->declared_type;

    // Uncurry: A -> B -> C becomes params=[A, B], ret=C
    while (std::holds_alternative<std::shared_ptr<types::FunctionType>>(current_type)) {
        auto ft = std::get<std::shared_ptr<types::FunctionType>>(current_type);
        CType param_ct = yona_type_to_ctype(ft->argumentType);
        param_ctypes.push_back(param_ct);
        param_types.push_back(llvm_type(param_ct));
        current_type = ft->returnType;
    }
    ret_ctype = yona_type_to_ctype(current_type);
    auto ret_llvm = llvm_type(ret_ctype);

    // Declare the extern function
    auto fn_type = llvm::FunctionType::get(ret_llvm, param_types, false);
    auto* fn = module_->getFunction(node->name);
    if (!fn) {
        fn = Function::Create(fn_type, Function::ExternalLinkage,
                               node->name, module_.get());
    }

    // Register as a compiled function
    CompiledFunction cf;
    cf.fn = fn;
    cf.return_type = node->is_async ? CType::PROMISE : ret_ctype;
    cf.param_types = param_ctypes;
    compiled_functions_[node->name] = cf;
    named_values_[node->name] = {fn, CType::FUNCTION,
                                  node->is_async ? std::vector<CType>{ret_ctype} : std::vector<CType>{}};

    // Compile the body
    return codegen(node->body);
}

// ===== Imports =====

TypedValue Codegen::codegen_import(ImportExpr* node) {
    // Process each import clause: generate extern declarations for module functions
    for (auto* clause : node->clauses) {
        if (clause->get_type() == AST_FUNCTIONS_IMPORT) {
            auto* fi = static_cast<FunctionsImport*>(clause);

            // Build module FQN string
            std::string mod_fqn;
            if (fi->fromFqn->packageName.has_value()) {
                auto* pkg = fi->fromFqn->packageName.value();
                for (size_t i = 0; i < pkg->parts.size(); i++) {
                    if (i > 0) mod_fqn += "\\";
                    mod_fqn += pkg->parts[i]->value;
                }
                mod_fqn += "\\";
            }
            mod_fqn += fi->fromFqn->moduleName->value;

            // For each imported function, declare an extern with the mangled name
            for (auto* alias : fi->aliases) {
                std::string func_name = alias->name->value;
                std::string import_name = alias->alias ? alias->alias->value : func_name;
                std::string mangled = mangle_name(mod_fqn, func_name);

                // Check if already declared
                auto* existing = module_->getFunction(mangled);
                if (!existing) {
                    // We don't know the exact signature yet — default to i64(i64, i64)
                    // for 2-arg functions. For proper signature resolution, we'd need to
                    // parse the module's source or read a .yonai metadata file.
                    // For now, create a variadic-like i64 signature and let the linker resolve.

                    // Try common arities: check if it's used and infer from call site
                    // For now, declare with no args — the call site will cast as needed
                    // Actually, we can't know arity without metadata. Use a generic approach:
                    // declare as taking variable i64 args via a wrapper that takes ptr.

                    // Simplest: don't declare yet. When codegen_apply encounters this name,
                    // it will find it in named_values_ as a FUNCTION and handle it.
                }

                // Register the mangled name so codegen_apply can find it
                // Store a marker that this is an external module function
                if (!existing) {
                    // Placeholder — actual function created at call site with known arity
                    named_values_[import_name] = {nullptr, CType::FUNCTION, {/* subtypes: mangled name hack */}};
                }

                // Store the mangled name mapping for the apply codegen
                // We need a way to map import_name → mangled name
                // Use a simple side table
                extern_functions_[import_name] = mangled;
            }
        }
    }

    // Compile the body expression
    return codegen(node->expr);
}

} // namespace yona::compiler::codegen
