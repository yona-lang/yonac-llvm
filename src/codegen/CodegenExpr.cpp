//
// Codegen — Expression code generation
//
// Literals, arithmetic, comparisons, let bindings, if/do, identifiers,
// raise/try-catch.
//

#include "Codegen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <iostream>

namespace yona::compiler::codegen {

// Constants — match runtime defines
static constexpr int64_t ARENA_DEFAULT_SIZE = 4096;
using namespace llvm;
using LType = llvm::Type;

// Coerce a value to a target LLVM type for PHI node compatibility.
// Handles: i1→i64, ptr↔i64, struct→ptr (via alloca), different int widths.
static Value* coerce_for_phi(Value* val, LType* target, IRBuilder<>& builder, LLVMContext& ctx) {
    auto* src = val->getType();
    if (src == target) return val;

    // Integer widening (e.g., i1 → i64)
    if (src->isIntegerTy() && target->isIntegerTy())
        return builder.CreateZExtOrTrunc(val, target);

    // ptr → i64
    if (src->isPointerTy() && target->isIntegerTy())
        return builder.CreatePtrToInt(val, target);

    // i64 → ptr
    if (src->isIntegerTy() && target->isPointerTy())
        return builder.CreateIntToPtr(val, target);

    // struct → ptr (box into alloca)
    if (src->isStructTy() && target->isPointerTy()) {
        auto* alloca = builder.CreateAlloca(src);
        builder.CreateStore(val, alloca);
        return alloca;
    }

    // ptr → struct: can't safely unbox without knowing layout, return as-is
    // (this case shouldn't normally happen)
    return val;
}

// Determine the widest common LLVM type for PHI merging.
static LType* common_phi_type(LType* a, LType* b, LLVMContext& ctx) {
    if (a == b) return a;

    // If either is a struct, use ptr (boxed representation)
    if (a->isStructTy() || b->isStructTy())
        return PointerType::get(ctx, 0);

    // ptr wins over integers
    if (a->isPointerTy() || b->isPointerTy())
        return PointerType::get(ctx, 0);

    // Wider integer wins
    if (a->isIntegerTy() && b->isIntegerTy()) {
        unsigned wa = a->getIntegerBitWidth(), wb = b->getIntegerBitWidth();
        return wa >= wb ? a : b;
    }

    // Fallback: i64
    return LType::getInt64Ty(ctx);
}

// ===== Literals =====

TypedValue Codegen::codegen_integer(IntegerExpr* node) {
    set_debug_loc(node->source_context);
    return {ConstantInt::get(LType::getInt64Ty(*context_), node->value), CType::INT};
}
TypedValue Codegen::codegen_float(FloatExpr* node) {
    set_debug_loc(node->source_context);
    return {ConstantFP::get(LType::getDoubleTy(*context_), node->value), CType::FLOAT};
}
TypedValue Codegen::codegen_bool_true(TrueLiteralExpr* node) {
    set_debug_loc(node->source_context);
    return {ConstantInt::getTrue(*context_), CType::BOOL};
}
TypedValue Codegen::codegen_bool_false(FalseLiteralExpr* node) {
    set_debug_loc(node->source_context);
    return {ConstantInt::getFalse(*context_), CType::BOOL};
}
TypedValue Codegen::codegen_string(StringExpr* node) {
    set_debug_loc(node->source_context);
    return {builder_->CreateGlobalStringPtr(node->value), CType::STRING};
}
TypedValue Codegen::codegen_unit(UnitExpr* node) {
    set_debug_loc(node->source_context);
    return {ConstantInt::get(LType::getInt64Ty(*context_), 0), CType::UNIT};
}

TypedValue Codegen::codegen_symbol(SymbolExpr* node) {
    set_debug_loc(node->source_context);
    int64_t id = intern_symbol(node->value);
    return {ConstantInt::get(LType::getInt64Ty(*context_), id), CType::SYMBOL};
}

// ===== Arithmetic (type-directed) =====

TypedValue Codegen::codegen_binary(AstNode* left_node, AstNode* right_node, const std::string& op) {
    set_debug_loc(left_node->source_context);
    auto left = auto_await(codegen(left_node));
    auto right = auto_await(codegen(right_node));
    if (!left || !right) return {};

    // Type validation for arithmetic operators
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
        bool left_numeric = (left.type == CType::INT || left.type == CType::FLOAT);
        bool right_numeric = (right.type == CType::INT || right.type == CType::FLOAT);
        bool both_string = (left.type == CType::STRING && right.type == CType::STRING);
        if (!left_numeric && !right_numeric && !both_string) {
            report_error(left_node->source_context,
                "type error: operator '" + op + "' requires numeric or string operands");
            return {};
        }
    }

    // String concatenation
    if (left.type == CType::STRING && right.type == CType::STRING && op == "+") {
        return {builder_->CreateCall(rt_.string_concat_, {left.val, right.val}), CType::STRING};
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
    set_debug_loc(left_node->source_context);
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

// ===== Stream fusion: count identifier references in AST =====

int Codegen::count_identifier_refs(AstNode* node, const std::string& name) {
    if (!node) return 0;
    auto ty = node->get_type();

    if (ty == AST_IDENTIFIER_EXPR)
        return static_cast<IdentifierExpr*>(node)->name->value == name ? 1 : 0;

    // Binary ops: all inherit from BinaryOpExpr with left/right
    if (dynamic_cast<BinaryOpExpr*>(static_cast<AstNode*>(node))) {
        auto* b = static_cast<BinaryOpExpr*>(node);
        return count_identifier_refs(b->left, name) + count_identifier_refs(b->right, name);
    }

    if (ty == AST_IF_EXPR) {
        auto* e = static_cast<IfExpr*>(node);
        return count_identifier_refs(e->condition, name)
             + count_identifier_refs(e->thenExpr, name)
             + count_identifier_refs(e->elseExpr, name);
    }
    if (ty == AST_LET_EXPR) {
        auto* e = static_cast<LetExpr*>(node);
        int c = 0;
        for (auto* a : e->aliases) {
            if (auto* va = dynamic_cast<ValueAlias*>(a)) {
                c += count_identifier_refs(va->expr, name);
                if (va->identifier->name->value == name) return c; // shadowed
            } else if (auto* la = dynamic_cast<LambdaAlias*>(a)) {
                c += count_identifier_refs(la->lambda, name);
                if (la->name->value == name) return c;
            }
        }
        return c + count_identifier_refs(e->expr, name);
    }
    if (ty == AST_CASE_EXPR) {
        auto* e = static_cast<CaseExpr*>(node);
        int c = count_identifier_refs(e->expr, name);
        for (auto* clause : e->clauses)
            c += count_identifier_refs(clause->body, name);
        return c;
    }
    if (ty == AST_APPLY_EXPR) {
        auto* e = static_cast<ApplyExpr*>(node);
        int c = 0;
        if (auto* nc = dynamic_cast<NameCall*>(e->call))
            c += (nc->name->value == name) ? 1 : 0;
        for (auto& arg : e->args) {
            if (std::holds_alternative<ExprNode*>(arg))
                c += count_identifier_refs(std::get<ExprNode*>(arg), name);
            else
                c += count_identifier_refs(std::get<ValueExpr*>(arg), name);
        }
        return c;
    }
    if (ty == AST_TUPLE_EXPR) {
        auto* e = static_cast<TupleExpr*>(node);
        int c = 0;
        for (auto* v : e->values) c += count_identifier_refs(v, name);
        return c;
    }
    if (ty == AST_VALUES_SEQUENCE_EXPR) {
        auto* e = static_cast<ValuesSequenceExpr*>(node);
        int c = 0;
        for (auto* v : e->values) c += count_identifier_refs(v, name);
        return c;
    }
    if (ty == AST_SEQ_GENERATOR_EXPR) {
        auto* g = static_cast<SeqGeneratorExpr*>(node);
        auto* ext = static_cast<ValueCollectionExtractorExpr*>(g->collectionExtractor);
        int c = count_identifier_refs(ext->collection, name);
        // Binding variable shadows name inside reducer/condition
        std::string var;
        if (auto* id = std::get_if<IdentifierExpr*>(&ext->expr))
            var = (*id)->name->value;
        if (var != name) {
            c += count_identifier_refs(g->reducerExpr, name);
            if (ext->condition) c += count_identifier_refs(ext->condition, name);
        }
        return c;
    }
    if (ty == AST_FUNCTION_EXPR) {
        auto* f = static_cast<FunctionExpr*>(node);
        // Check if name is shadowed by a parameter
        for (auto& p : f->patterns) {
            if (p->get_type() == AST_PATTERN_VALUE) {
                auto* pv = static_cast<PatternValue*>(p);
                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                    if ((*id)->name->value == name) return 0; // shadowed by param
            }
        }
        int c = 0;
        for (auto* body : f->bodies)
            if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body))
                c += count_identifier_refs(bwg->expr, name);
        return c;
    }
    if (ty == AST_DO_EXPR) {
        auto* e = static_cast<DoExpr*>(node);
        int c = 0;
        for (auto* s : e->steps) c += count_identifier_refs(s, name);
        return c;
    }
    return 0; // literals, symbols, etc.
}

// ===== Let Bindings =====

// Analyze which let-bound names don't escape the scope (for arena allocation).
std::unordered_set<std::string> Codegen::analyze_let_escaping(LetExpr* node) {
    std::unordered_set<std::string> local_non_escaping;
    if (node->aliases.size() < 2) return local_non_escaping;

    std::unordered_set<std::string> local_fns;
    for (auto& [name, _] : deferred_functions_) local_fns.insert(name);
    for (auto& [name, _] : compiled_functions_) local_fns.insert(name);

    std::unordered_set<std::string> let_names;
    for (auto* alias : node->aliases) {
        if (auto* va = dynamic_cast<ValueAlias*>(alias))
            let_names.insert(va->identifier->name->value);
        else if (auto* la = dynamic_cast<LambdaAlias*>(alias))
            let_names.insert(la->name->value);
    }

    std::unordered_set<std::string> escaping;
    std::function<void(AstNode*, bool)> check_escape = [&](AstNode* n, bool ret_pos) {
        if (!n) return;
        if (n->get_type() == AST_IDENTIFIER_EXPR) {
            auto* id = static_cast<IdentifierExpr*>(n);
            if (ret_pos && let_names.count(id->name->value))
                escaping.insert(id->name->value);
            return;
        }
        if (n->get_type() == AST_LET_EXPR) { check_escape(static_cast<LetExpr*>(n)->expr, ret_pos); return; }
        if (n->get_type() == AST_IF_EXPR) {
            auto* ie = static_cast<IfExpr*>(n);
            check_escape(ie->thenExpr, ret_pos);
            check_escape(ie->elseExpr, ret_pos);
            return;
        }
        if (n->get_type() == AST_CASE_EXPR) {
            for (auto* clause : static_cast<CaseExpr*>(n)->clauses) check_escape(clause->body, ret_pos);
            return;
        }
        if (n->get_type() == AST_DO_EXPR) {
            auto* de = static_cast<DoExpr*>(n);
            if (!de->steps.empty()) check_escape(de->steps.back(), ret_pos);
            return;
        }
        if (n->get_type() == AST_FUNCTION_EXPR) {
            auto* fe = static_cast<FunctionExpr*>(n);
            std::function<void(AstNode*)> walk = [&](AstNode* nd) {
                if (!nd) return;
                if (nd->get_type() == AST_IDENTIFIER_EXPR) {
                    if (let_names.count(static_cast<IdentifierExpr*>(nd)->name->value))
                        escaping.insert(static_cast<IdentifierExpr*>(nd)->name->value);
                    return;
                }
                if (nd->get_type() == AST_LET_EXPR) { walk(static_cast<LetExpr*>(nd)->expr); return; }
                if (nd->get_type() == AST_IF_EXPR) { auto* i = static_cast<IfExpr*>(nd); walk(i->condition); walk(i->thenExpr); walk(i->elseExpr); return; }
                if (nd->get_type() == AST_CASE_EXPR) { auto* c = static_cast<CaseExpr*>(nd); walk(c->expr); for (auto* cl : c->clauses) walk(cl->body); return; }
                if (nd->get_type() == AST_APPLY_EXPR) { auto* a = static_cast<ApplyExpr*>(nd); for (auto& arg : a->args) { if (std::holds_alternative<ExprNode*>(arg)) walk(std::get<ExprNode*>(arg)); else walk(std::get<ValueExpr*>(arg)); } return; }
            };
            for (auto* body : fe->bodies)
                if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body)) walk(bwg->expr);
            return;
        }
        if (n->get_type() == AST_APPLY_EXPR) {
            auto* ae = static_cast<ApplyExpr*>(n);
            std::string callee;
            if (auto* nc = dynamic_cast<NameCall*>(ae->call)) callee = nc->name->value;
            bool is_local = local_fns.count(callee) > 0;
            for (auto& arg : ae->args) {
                AstNode* a = std::holds_alternative<ExprNode*>(arg)
                    ? static_cast<AstNode*>(std::get<ExprNode*>(arg))
                    : static_cast<AstNode*>(std::get<ValueExpr*>(arg));
                check_escape(a, !is_local);
            }
        }
    };
    check_escape(node->expr, true);

    for (auto& name : let_names) {
        if (escaping.count(name)) continue;
        for (auto* alias : node->aliases) {
            if (auto* va = dynamic_cast<ValueAlias*>(alias)) {
                if (va->identifier->name->value != name) continue;
                auto ty = va->expr->get_type();
                if (ty == AST_VALUES_SEQUENCE_EXPR || ty == AST_TUPLE_EXPR ||
                    ty == AST_SET_EXPR || ty == AST_DICT_EXPR ||
                    ty == AST_SEQ_GENERATOR_EXPR || ty == AST_SET_GENERATOR_EXPR ||
                    ty == AST_DICT_GENERATOR_EXPR || ty == AST_FUNCTION_EXPR)
                    local_non_escaping.insert(name);
            }
        }
    }
    return local_non_escaping;
}

// Set up arena allocator for non-escaping bindings (if enough qualify).
llvm::Value* Codegen::setup_let_arena(const std::unordered_set<std::string>& non_escaping) {
    if (non_escaping.size() < 2) return nullptr;
    auto i64_ty = LType::getInt64Ty(*context_);
    return builder_->CreateCall(rt_.arena_create_,
        {ConstantInt::get(i64_ty, ARENA_DEFAULT_SIZE)}, "arena");
}

// Codegen all let aliases: ValueAlias, LambdaAlias, PatternAlias.
void Codegen::codegen_let_aliases(LetExpr* node, llvm::Value* arena,
                                   const std::unordered_set<std::string>& non_escaping,
                                   std::vector<TypedValue>& scope_bindings,
                                   std::vector<bool>& binding_is_arena) {
    auto saved_arena = current_arena_;

    for (auto* alias : node->aliases) {
        if (auto* va = dynamic_cast<ValueAlias*>(alias)) {
            std::string vname = va->identifier->name->value;

            // Stream fusion: defer single-use generator bindings
            if (va->expr->get_type() == AST_SEQ_GENERATOR_EXPR) {
                int refs = count_identifier_refs(node->expr, vname);
                if (refs == 1) {
                    deferred_generators_[vname] = static_cast<SeqGeneratorExpr*>(va->expr);
                    continue;
                }
            }

            bool use_arena = arena && non_escaping.count(vname);
            if (use_arena) current_arena_ = arena;
            auto tv = codegen(va->expr);
            if (use_arena) current_arena_ = saved_arena;

            if (tv) {
                named_values_[vname] = tv;

                // Seq protection: rc_inc to prevent unique-owner tail mutation
                if (tv.type == CType::SEQ && tv.val && !llvm::isa<llvm::Constant>(tv.val))
                    emit_rc_inc(tv.val, CType::SEQ);

                if (is_heap_type(tv.type) && tv.val) {
                    scope_bindings.push_back(tv);
                    binding_is_arena.push_back(use_arena);
                }

                if (debug_.enabled && debug_.scope && debug_.builder && tv.val) {
                    auto* alloca = builder_->CreateAlloca(tv.val->getType(), nullptr, vname);
                    builder_->CreateStore(tv.val, alloca);
                    auto* di_var = debug_.builder->createAutoVariable(
                        debug_.scope, vname, debug_.file, va->source_context.line, di_type_for(tv.type));
                    debug_.builder->insertDeclare(alloca, di_var, debug_.builder->createExpression(),
                        DILocation::get(*context_, va->source_context.line,
                                        va->source_context.column, debug_.scope),
                        builder_->GetInsertBlock());
                    tv.val = builder_->CreateLoad(tv.val->getType(), alloca, vname);
                    named_values_[vname] = tv;
                }
            }
        } else if (auto* la = dynamic_cast<LambdaAlias*>(alias)) {
            auto tv = codegen_lambda_alias(la);
            if (tv && is_heap_type(tv.type) && tv.val) {
                scope_bindings.push_back(tv);
                binding_is_arena.push_back(false);
            }
        } else if (auto* pa = dynamic_cast<PatternAlias*>(alias)) {
            auto tv = codegen(pa->expr);
            if (tv && tv.type == CType::TUPLE) {
                auto* tp = dynamic_cast<TuplePattern*>(pa->pattern);
                if (tp) {
                    auto i64_local = LType::getInt64Ty(*context_);
                    Value* tuple_ptr = tv.val;
                    if (tuple_ptr->getType()->isIntegerTy())
                        tuple_ptr = builder_->CreateIntToPtr(tuple_ptr, PointerType::get(*context_, 0));
                    for (size_t i = 0; i < tp->patterns.size(); i++) {
                        Value* elem;
                        if (tuple_ptr->getType()->isPointerTy()) {
                            auto* gep = builder_->CreateGEP(i64_local, tuple_ptr,
                                {ConstantInt::get(i64_local, i + 2)}, "let_tuple_gep");
                            elem = builder_->CreateLoad(i64_local, gep, "let_tuple_elem");
                        } else {
                            elem = builder_->CreateExtractValue(tuple_ptr, {(unsigned)i});
                        }
                        CType et = (i < tv.subtypes.size()) ? tv.subtypes[i] : CType::INT;
                        auto* sub = tp->patterns[i];
                        if (sub->get_type() == AST_PATTERN_VALUE) {
                            auto* pv = static_cast<PatternValue*>(sub);
                            if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                                named_values_[(*id)->name->value] = {elem, et};
                                if (is_heap_type(et)) {
                                    scope_bindings.push_back({elem, et});
                                    binding_is_arena.push_back(false);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    current_arena_ = saved_arena;
}

// Clean up scope: RC decrement bindings, destroy arena, clean deferred generators.
void Codegen::cleanup_let_scope(const std::vector<TypedValue>& scope_bindings,
                                 const std::vector<bool>& binding_is_arena,
                                 const TypedValue& result, llvm::Value* arena) {
    if (!scope_bindings.empty() && result && result.val) {
        emit_rc_inc(result.val, result.type);
        for (size_t i = 0; i < scope_bindings.size(); i++) {
            if (i < binding_is_arena.size() && binding_is_arena[i])
                continue;
            emit_rc_dec(scope_bindings[i].val, scope_bindings[i].type);
        }
    }
    if (arena)
        builder_->CreateCall(rt_.arena_destroy_, {arena});
}

TypedValue Codegen::codegen_let(LetExpr* node) {
    set_debug_loc(node->source_context);

    // 1. Escape analysis
    auto non_escaping = analyze_let_escaping(node);

    // 2. Arena setup
    auto saved_arena = current_arena_;
    auto* arena = setup_let_arena(non_escaping);

    // 3. Structured concurrency: create a task group for async let bindings.
    //    The group ensures all async children complete before the scope exits,
    //    with error propagation and cancellation.
    auto saved_group = current_group_;
    bool has_group = false;
    // Detect if any alias might produce async work (we'll create the group
    // eagerly — if no async work happens, the group is cheap to create/destroy)
    if (node->aliases.size() > 1) {
        current_group_ = builder_->CreateCall(rt_.group_begin_, {}, "let_group");
        has_group = true;
    }

    // 4. Codegen all aliases
    std::vector<TypedValue> scope_bindings;
    std::vector<bool> binding_is_arena;
    codegen_let_aliases(node, arena, non_escaping, scope_bindings, binding_is_arena);

    // 5. Codegen body
    auto result = codegen(node->expr);

    // 6. Structured concurrency: await all group children before cleanup
    if (has_group) {
        builder_->CreateCall(rt_.group_await_all_, {current_group_});
        builder_->CreateCall(rt_.group_end_, {current_group_});
        current_group_ = saved_group;
    }

    // 7. Cleanup scope
    cleanup_let_scope(scope_bindings, binding_is_arena, result, arena);
    current_arena_ = saved_arena;

    // 6. Clean up deferred generators not consumed by fusion
    for (auto* alias : node->aliases)
        if (auto* va = dynamic_cast<ValueAlias*>(alias))
            deferred_generators_.erase(va->identifier->name->value);

    return result;
}

// ===== If Expression =====

TypedValue Codegen::codegen_if(IfExpr* node) {
    set_debug_loc(node->source_context);
    auto cond = auto_await(codegen(node->condition));
    if (!cond) return {};
    // Ensure condition is i1 for branch — closures return i64 even for bool results
    if (cond.val->getType() != LType::getInt1Ty(*context_)) {
        if (cond.val->getType()->isIntegerTy())
            cond.val = builder_->CreateICmpNE(cond.val, ConstantInt::get(cond.val->getType(), 0));
        else if (cond.val->getType()->isPointerTy())
            cond.val = builder_->CreateICmpNE(
                builder_->CreatePtrToInt(cond.val, LType::getInt64Ty(*context_)),
                ConstantInt::get(LType::getInt64Ty(*context_), 0));
    }

    auto fn = builder_->GetInsertBlock()->getParent();
    auto then_bb = BasicBlock::Create(*context_, "then", fn);
    auto else_bb = BasicBlock::Create(*context_, "else");
    auto merge_bb = BasicBlock::Create(*context_, "ifcont");
    builder_->CreateCondBr(cond.val, then_bb, else_bb);

    builder_->SetInsertPoint(then_bb);
    auto then_tv = codegen(node->thenExpr);
    if (!then_tv) return {};
    bool then_terminated = builder_->GetInsertBlock()->getTerminator() != nullptr;
    BasicBlock* then_end = nullptr;
    if (!then_terminated) {
        builder_->CreateBr(merge_bb);
        then_end = builder_->GetInsertBlock();
    }

    fn->insert(fn->end(), else_bb);
    builder_->SetInsertPoint(else_bb);
    auto else_tv = codegen(node->elseExpr);
    if (!else_tv) return {};
    bool else_terminated = builder_->GetInsertBlock()->getTerminator() != nullptr;
    BasicBlock* else_end = nullptr;
    if (!else_terminated) {
        builder_->CreateBr(merge_bb);
        else_end = builder_->GetInsertBlock();
    }

    fn->insert(fn->end(), merge_bb);
    builder_->SetInsertPoint(merge_bb);
    unsigned phi_count = (then_end ? 1 : 0) + (else_end ? 1 : 0);
    if (phi_count == 0) {
        // Both branches terminate (e.g., both raise) — merge is dead
        return then_tv;
    }
    // Determine common type for PHI — branches may return different LLVM types
    // (e.g., ADT struct vs symbol i64, or i1 vs i64)
    LType* then_ty = then_end ? then_tv.val->getType() : nullptr;
    LType* else_ty = else_end ? else_tv.val->getType() : nullptr;
    LType* phi_type;
    if (then_ty && else_ty)
        phi_type = common_phi_type(then_ty, else_ty, *context_);
    else
        phi_type = then_ty ? then_ty : else_ty;

    auto phi = builder_->CreatePHI(phi_type, phi_count);
    if (then_end) {
        // Coerce then value if needed — insert before the branch terminator
        if (then_tv.val->getType() != phi_type) {
            auto saved = builder_->GetInsertPoint();
            builder_->SetInsertPoint(then_end->getTerminator());
            then_tv.val = coerce_for_phi(then_tv.val, phi_type, *builder_, *context_);
            builder_->SetInsertPoint(merge_bb);
        }
        phi->addIncoming(then_tv.val, then_end);
    }
    if (else_end) {
        if (else_tv.val->getType() != phi_type) {
            auto saved = builder_->GetInsertPoint();
            builder_->SetInsertPoint(else_end->getTerminator());
            else_tv.val = coerce_for_phi(else_tv.val, phi_type, *builder_, *context_);
            builder_->SetInsertPoint(merge_bb);
        }
        phi->addIncoming(else_tv.val, else_end);
    }
    return {phi, then_tv.type, then_tv.subtypes};
}

// ===== Identifier =====

TypedValue Codegen::codegen_identifier(IdentifierExpr* node) {
    set_debug_loc(node->source_context);
    // Materialize deferred generator if referenced outside fusion context
    {
        auto dg_it = deferred_generators_.find(node->name->value);
        if (dg_it != deferred_generators_.end()) {
            auto* gen = dg_it->second;
            deferred_generators_.erase(dg_it);
            auto tv = codegen_seq_generator(gen);
            if (tv) named_values_[node->name->value] = tv;
            return tv;
        }
    }

    auto it = named_values_.find(node->name->value);
    if (it != named_values_.end()) {
        // If it's a FUNCTION with nullptr val, set last_lambda_name_ for higher-order support
        if (it->second.type == CType::FUNCTION && !it->second.val) {
            last_lambda_name_ = node->name->value;
        }
        // Note: Perceus DUP at non-last uses requires matching callee DROP
        // at function exit. Callee DROP is blocked by the PHI problem:
        // when a function returns a param via a branch (if/case), the
        // return Value* is a PHI, not the param, so the DROP check
        // `param == return` fails and frees the returned value.
        // Full Perceus needs per-branch DROP insertion.
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
    auto adt_it = types_.adt_constructors.find(node->name->value);
    if (adt_it != types_.adt_constructors.end() && adt_it->second.arity == 0) {
        auto tag_ty = LType::getInt64Ty(*context_);
        auto i64_ty = LType::getInt64Ty(*context_);

        if (adt_it->second.is_recursive) {
            // Recursive ADT: heap-allocate via runtime
            auto* node_ptr = builder_->CreateCall(rt_.adt_alloc_,
                {ConstantInt::get(tag_ty, adt_it->second.tag),
                 ConstantInt::get(i64_ty, 0)}, "adt_node");
            TypedValue result{node_ptr, CType::ADT};
            result.adt_type_name = adt_it->second.type_name;
            return result;
        } else {
            // Non-recursive: flat struct {i8, i64*max_arity}
            std::vector<LType*> fields = {tag_ty};
            for (int f = 0; f < adt_it->second.max_arity; f++)
                fields.push_back(i64_ty);
            auto* struct_type = StructType::get(*context_, fields);
            Value* val = UndefValue::get(struct_type);
            val = builder_->CreateInsertValue(val, ConstantInt::get(tag_ty, adt_it->second.tag), {0});
            TypedValue result{val, CType::ADT};
            result.adt_type_name = adt_it->second.type_name;
            return result;
        }
    }
    // Check if it's a non-zero-arity ADT constructor (used as a function reference)
    if (adt_it != types_.adt_constructors.end()) {
        last_lambda_name_ = node->name->value;
        return {nullptr, CType::FUNCTION};
    }
    std::string msg = "undefined variable '" + node->name->value + "'";
    auto suggestion = suggest_similar(node->name->value);
    if (!suggestion.empty()) msg += "; did you mean '" + suggestion + "'?";
    report_error(node->source_context, msg);
    return {};
}

TypedValue Codegen::codegen_main_node(MainNode* node) { return codegen(node->node); }

// ===== Do Expression =====

TypedValue Codegen::codegen_do(DoExpr* node) {
    set_debug_loc(node->source_context);
    TypedValue last;
    for (auto* step : node->steps) last = codegen(step);
    return last ? last : TypedValue{ConstantInt::get(LType::getInt64Ty(*context_), 0), CType::INT};
}

// ===== Exception Handling =====

TypedValue Codegen::codegen_raise(RaiseExpr* node) {
    set_debug_loc(node->source_context);
    auto exc_val = codegen(node->value);
    if (!exc_val) return {};

    auto i64_ty = LType::getInt64Ty(*context_);
    Value* tag_val;
    Value* payload_val;

    if (exc_val.type == CType::ADT && exc_val.val->getType()->isStructTy()) {
        // Non-recursive ADT: extract tag and first payload field
        tag_val = builder_->CreateExtractValue(exc_val.val, {0});
        tag_val = builder_->CreateZExt(tag_val, i64_ty);
        if (cast<StructType>(exc_val.val->getType())->getNumElements() > 1) {
            payload_val = builder_->CreateExtractValue(exc_val.val, {1});
            // If payload is a pointer (string), keep as-is via inttoptr
            if (payload_val->getType()->isIntegerTy())
                payload_val = builder_->CreateIntToPtr(payload_val, PointerType::get(*context_, 0));
        } else {
            payload_val = ConstantPointerNull::get(PointerType::get(*context_, 0));
        }
    } else if (exc_val.type == CType::ADT && exc_val.val->getType()->isPointerTy()) {
        // Recursive ADT: use runtime accessors
        tag_val = builder_->CreateZExt(
            builder_->CreateCall(rt_.adt_get_tag_, {exc_val.val}), i64_ty);
        auto field = builder_->CreateCall(rt_.adt_get_field_, {exc_val.val, ConstantInt::get(i64_ty, 0)});
        payload_val = builder_->CreateIntToPtr(field, PointerType::get(*context_, 0));
    } else {
        // Fallback: treat as integer tag with no payload
        tag_val = exc_val.val;
        if (tag_val->getType() != i64_ty)
            tag_val = builder_->CreateZExtOrTrunc(tag_val, i64_ty);
        payload_val = ConstantPointerNull::get(PointerType::get(*context_, 0));
    }

    builder_->CreateCall(rt_.raise_, {tag_val, payload_val});
    builder_->CreateUnreachable();
    return {UndefValue::get(i64_ty), CType::UNIT};
}

TypedValue Codegen::codegen_try_catch(TryCatchExpr* node) {
    set_debug_loc(node->source_context);
    auto fn = builder_->GetInsertBlock()->getParent();
    auto i32_ty = LType::getInt32Ty(*context_);
    auto i64_ty = LType::getInt64Ty(*context_);

    auto try_bb = BasicBlock::Create(*context_, "try.body", fn);
    auto catch_bb = BasicBlock::Create(*context_, "catch.entry", fn);
    auto merge_bb = BasicBlock::Create(*context_, "try.merge");

    // Push jmp_buf slot, then call setjmp in THIS function's stack frame
    auto jmp_buf_ptr = builder_->CreateCall(rt_.try_begin_, {}, "jmp.buf");
    auto setjmp_fn = module_->getFunction("setjmp");
    auto try_result = builder_->CreateCall(setjmp_fn, {jmp_buf_ptr}, "try.setjmp");
    auto is_exc = builder_->CreateICmpNE(try_result, ConstantInt::get(i32_ty, 0));
    builder_->CreateCondBr(is_exc, catch_bb, try_bb);

    // Try body
    builder_->SetInsertPoint(try_bb);
    auto try_val = codegen(node->tryExpr);
    bool try_terminated = builder_->GetInsertBlock()->getTerminator() != nullptr;
    BasicBlock* try_end_bb = nullptr;
    if (!try_terminated) {
        builder_->CreateCall(rt_.try_end_, {});
        if (!try_val) try_val = {ConstantInt::get(i64_ty, 0), CType::INT};
        builder_->CreateBr(merge_bb);
        try_end_bb = builder_->GetInsertBlock();
    } else {
        if (!try_val) try_val = {ConstantInt::get(i64_ty, 0), CType::INT};
    }

    // Catch body: get exception tag and payload, pattern match
    builder_->SetInsertPoint(catch_bb);
    auto exc_tag = builder_->CreateCall(rt_.get_exc_sym_, {}, "exc.tag");
    auto exc_payload = builder_->CreateCall(rt_.get_exc_msg_, {}, "exc.payload");

    TypedValue catch_val = {ConstantInt::get(i64_ty, 0), CType::INT};
    std::vector<std::pair<TypedValue, BasicBlock*>> catch_results;

    if (node->catchExpr) {
        for (size_t ci = 0; ci < node->catchExpr->patterns.size(); ci++) {
            auto* cp = node->catchExpr->patterns[ci];
            auto body_bb = BasicBlock::Create(*context_, "catch.body." + std::to_string(ci), fn);
            BasicBlock* next_bb;
            if (ci + 1 < node->catchExpr->patterns.size())
                next_bb = BasicBlock::Create(*context_, "catch.next." + std::to_string(ci+1), fn);
            else
                next_bb = BasicBlock::Create(*context_, "catch.reraise", fn);

            auto* pat = cp->matchPattern;

            if (pat->get_type() == AST_CONSTRUCTOR_PATTERN) {
                // ADT constructor pattern: RuntimeError msg -> ...
                auto* cpat = static_cast<ConstructorPattern*>(pat);
                auto ctor_it = types_.adt_constructors.find(cpat->constructor_name);
                if (ctor_it != types_.adt_constructors.end()) {
                    auto tag_val = ConstantInt::get(i64_ty, ctor_it->second.tag);
                    auto cmp = builder_->CreateICmpEQ(exc_tag, tag_val);
                    builder_->CreateCondBr(cmp, body_bb, next_bb);
                    builder_->SetInsertPoint(body_bb);
                    // Bind sub-patterns: payload is the first field
                    for (size_t fi = 0; fi < cpat->sub_patterns.size(); fi++) {
                        auto* sub = cpat->sub_patterns[fi];
                        if (sub->get_type() == AST_PATTERN_VALUE) {
                            auto* pv = static_cast<PatternValue*>(sub);
                            if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                                CType ftype = (fi < ctor_it->second.field_types.size())
                                    ? ctor_it->second.field_types[fi] : CType::INT;
                                if (fi == 0) {
                                    named_values_[(*id)->name->value] = {exc_payload, ftype};
                                } else {
                                    named_values_[(*id)->name->value] = {
                                        ConstantInt::get(i64_ty, 0), CType::INT};
                                }
                            }
                        }
                    }
                } else {
                    builder_->CreateBr(body_bb);
                    builder_->SetInsertPoint(body_bb);
                }
            } else if (pat->get_type() == AST_UNDERSCORE_PATTERN) {
                builder_->CreateBr(body_bb);
                builder_->SetInsertPoint(body_bb);
            } else if (pat->get_type() == AST_PATTERN_VALUE) {
                auto* pv = static_cast<PatternValue*>(pat);
                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                    // Bind exception as a tuple (tag, payload)
                    auto* st = StructType::get(*context_, {i64_ty, llvm_type(CType::STRING)});
                    Value* tup = UndefValue::get(st);
                    tup = builder_->CreateInsertValue(tup, exc_tag, {0});
                    tup = builder_->CreateInsertValue(tup, exc_payload, {1});
                    named_values_[(*id)->name->value] = {tup, CType::TUPLE, {CType::INT, CType::STRING}};
                }
                builder_->CreateBr(body_bb);
                builder_->SetInsertPoint(body_bb);
            } else {
                builder_->CreateBr(body_bb);
                builder_->SetInsertPoint(body_bb);
            }

            // Compile handler body
            TypedValue handler_val;
            if (auto* pwog = std::get_if<PatternWithoutGuards*>(&cp->pattern))
                handler_val = codegen((*pwog)->expr);
            if (!handler_val) handler_val = {ConstantInt::get(i64_ty, 0), CType::INT};

            if (!builder_->GetInsertBlock()->getTerminator()) {
                builder_->CreateBr(merge_bb);
                catch_results.push_back({handler_val, builder_->GetInsertBlock()});
            }

            if (ci + 1 < node->catchExpr->patterns.size())
                builder_->SetInsertPoint(next_bb);
            else {
                builder_->SetInsertPoint(next_bb);
                builder_->CreateCall(rt_.raise_, {exc_tag, exc_payload});
                builder_->CreateUnreachable();
            }
        }
    }

    // Merge
    fn->insert(fn->end(), merge_bb);
    builder_->SetInsertPoint(merge_bb);

    if (catch_results.empty()) {
        return try_val;
    }

    // Determine result type: use catch type if try body always raises (terminated)
    CType result_type = try_end_bb ? try_val.type
        : (!catch_results.empty() ? catch_results[0].first.type : CType::INT);
    // Determine common PHI type across try body and all catch handlers
    LType* result_llvm = try_end_bb ? try_val.val->getType()
        : (!catch_results.empty() ? catch_results[0].first.val->getType() : LType::getInt64Ty(*context_));
    for (auto& [tv, bb] : catch_results)
        result_llvm = common_phi_type(result_llvm, tv.val->getType(), *context_);

    unsigned pred_count = (try_end_bb ? 1 : 0) + catch_results.size();
    if (pred_count == 0) return try_val;

    auto phi = builder_->CreatePHI(result_llvm, pred_count, "try.result");
    if (try_end_bb) {
        Value* incoming = try_val.val;
        if (incoming->getType() != result_llvm) {
            builder_->SetInsertPoint(try_end_bb->getTerminator());
            incoming = coerce_for_phi(incoming, result_llvm, *builder_, *context_);
            builder_->SetInsertPoint(merge_bb);
        }
        phi->addIncoming(incoming, try_end_bb);
    }
    for (auto& [tv, bb] : catch_results) {
        Value* incoming = tv.val;
        if (incoming->getType() != result_llvm) {
            builder_->SetInsertPoint(bb->getTerminator());
            incoming = coerce_for_phi(incoming, result_llvm, *builder_, *context_);
            builder_->SetInsertPoint(merge_bb);
        }
        phi->addIncoming(incoming, bb);
    }
    return {phi, result_type};
}

// ===== With Expression (resource management) =====
//
// with handle = openFile "data.txt" in readAll handle
//
// Desugars to: bind resource, try body, close on both success and failure.
// Guarantees close() is called regardless of exceptions.
// The resource type must have a Closeable instance (checked at compile time).

TypedValue Codegen::codegen_with(WithExpr* node) {
    set_debug_loc(node->source_context);
    auto fn = builder_->GetInsertBlock()->getParent();
    auto i32_ty = LType::getInt32Ty(*context_);
    auto i64_ty = LType::getInt64Ty(*context_);

    // 1. Evaluate resource expression and bind to name
    auto resource = codegen(node->contextExpr);
    if (!resource) return {};

    // 2. Resolve Closeable.close for the resource type via trait dispatch
    std::string adt_name = resource.adt_type_name;
    auto resolved = resolve_trait_method("close", resource.type, adt_name);
    if (resolved.empty()) {
        std::string type_name = ctype_to_type_name(resource.type);
        if (!adt_name.empty()) type_name = adt_name;
        report_error(node->source_context,
            "type '" + type_name + "' does not implement Closeable (required by 'with')");
        return {};
    }

    // Find the close function
    auto close_cf = compiled_functions_.find(resolved);
    if (close_cf == compiled_functions_.end()) {
        auto def_it = deferred_functions_.find(resolved);
        if (def_it != deferred_functions_.end()) {
            compile_function(resolved, def_it->second, {resource});
            close_cf = compiled_functions_.find(resolved);
        }
    }
    if (close_cf == compiled_functions_.end()) {
        // Try as extern
        auto* close_fn = module_->getFunction(resolved);
        if (!close_fn) {
            auto fn_type = llvm::FunctionType::get(LType::getVoidTy(*context_),
                {resource.val->getType()}, false);
            close_fn = Function::Create(fn_type, Function::ExternalLinkage, resolved, module_.get());
        }
        CompiledFunction cf;
        cf.fn = close_fn;
        cf.return_type = CType::UNIT;
        cf.param_types = {resource.type};
        compiled_functions_[resolved] = cf;
        close_cf = compiled_functions_.find(resolved);
    }

    auto* close_fn = close_cf->second.fn;

    std::string var_name = node->name->value;
    auto saved_values = named_values_;
    named_values_[var_name] = resource;

    // Helper to emit close call using the resolved trait method
    auto emit_close = [&]() {
        Value* arg = resource.val;
        if (close_fn->arg_size() > 0) {
            auto* expected_ty = close_fn->getArg(0)->getType();
            if (arg->getType() != expected_ty) {
                if (arg->getType()->isPointerTy() && expected_ty->isIntegerTy())
                    arg = builder_->CreatePtrToInt(arg, expected_ty);
                else if (arg->getType()->isIntegerTy() && expected_ty->isPointerTy())
                    arg = builder_->CreateIntToPtr(arg, expected_ty);
                else if (arg->getType()->isIntegerTy() && expected_ty->isIntegerTy())
                    arg = builder_->CreateZExtOrTrunc(arg, expected_ty);
            }
        }
        builder_->CreateCall(close_fn, {arg});
    };

    // 3. Evaluate body expression
    auto body_val = codegen(node->bodyExpr);
    if (!body_val) body_val = {ConstantInt::get(i64_ty, 0), CType::INT};

    // 4. Close resource (always, regardless of body result)
    if (!builder_->GetInsertBlock()->getTerminator())
        emit_close();

    named_values_ = saved_values;
    return body_val;
}

} // namespace yona::compiler::codegen
