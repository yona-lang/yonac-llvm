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
using namespace llvm;
using LType = llvm::Type;

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

// ===== Let Bindings =====

TypedValue Codegen::codegen_let(LetExpr* node) {
    set_debug_loc(node->source_context);

    // Escape analysis: determine which bindings don't escape this scope.
    // Analysis results stored for future arena optimization.
    std::unordered_set<std::string> local_non_escaping;
    if (false) { // Disabled: analysis-only, enable when arena allocation is ready
        std::unordered_set<std::string> local_fns;
        for (auto& [name, _] : deferred_functions_) local_fns.insert(name);
        for (auto& [name, _] : compiled_functions_) local_fns.insert(name);
        // Collect let-bound names
        std::unordered_set<std::string> let_names;
        for (auto* alias : node->aliases) {
            if (auto* va = dynamic_cast<ValueAlias*>(alias))
                let_names.insert(va->identifier->name->value);
            else if (auto* la = dynamic_cast<LambdaAlias*>(alias))
                let_names.insert(la->name->value);
        }
        // Check which escape
        std::unordered_set<std::string> escaping;
        // A binding escapes if it appears in the result expression
        std::function<void(AstNode*, bool)> check_escape = [&](AstNode* n, bool ret_pos) {
            if (!n) return;
            if (n->get_type() == AST_IDENTIFIER_EXPR) {
                auto* id = static_cast<IdentifierExpr*>(n);
                if (ret_pos && let_names.count(id->name->value))
                    escaping.insert(id->name->value);
                return;
            }
            if (n->get_type() == AST_LET_EXPR) {
                auto* le = static_cast<LetExpr*>(n);
                check_escape(le->expr, ret_pos);
                return;
            }
            if (n->get_type() == AST_IF_EXPR) {
                auto* ie = static_cast<IfExpr*>(n);
                check_escape(ie->thenExpr, ret_pos);
                check_escape(ie->elseExpr, ret_pos);
                return;
            }
            if (n->get_type() == AST_CASE_EXPR) {
                auto* ce = static_cast<CaseExpr*>(n);
                for (auto* clause : ce->clauses) check_escape(clause->body, ret_pos);
                return;
            }
            if (n->get_type() == AST_DO_EXPR) {
                auto* de = static_cast<DoExpr*>(n);
                if (!de->steps.empty())
                    check_escape(de->steps.back(), ret_pos);
                return;
            }
            if (n->get_type() == AST_FUNCTION_EXPR) {
                // Lambda captures — all referenced let_names escape
                auto* fe = static_cast<FunctionExpr*>(n);
                std::function<void(AstNode*)> walk = [&](AstNode* node) {
                    if (!node) return;
                    if (node->get_type() == AST_IDENTIFIER_EXPR) {
                        auto* id2 = static_cast<IdentifierExpr*>(node);
                        if (let_names.count(id2->name->value))
                            escaping.insert(id2->name->value);
                        return;
                    }
                    if (node->get_type() == AST_LET_EXPR) { walk(static_cast<LetExpr*>(node)->expr); return; }
                    if (node->get_type() == AST_IF_EXPR) { auto* i = static_cast<IfExpr*>(node); walk(i->condition); walk(i->thenExpr); walk(i->elseExpr); return; }
                    if (node->get_type() == AST_CASE_EXPR) { auto* c = static_cast<CaseExpr*>(node); walk(c->expr); for (auto* cl : c->clauses) walk(cl->body); return; }
                    if (node->get_type() == AST_APPLY_EXPR) { auto* a = static_cast<ApplyExpr*>(node); for (auto& arg : a->args) { if (std::holds_alternative<ExprNode*>(arg)) walk(std::get<ExprNode*>(arg)); else walk(std::get<ValueExpr*>(arg)); } return; }
                };
                for (auto* body : fe->bodies) {
                    if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body)) walk(bwg->expr);
                }
                return;
            }
            if (n->get_type() == AST_APPLY_EXPR) {
                auto* ae = static_cast<ApplyExpr*>(n);
                std::string callee;
                if (auto* nc = dynamic_cast<NameCall*>(ae->call)) callee = nc->name->value;
                bool is_local = local_fns.count(callee) > 0;
                for (auto& arg : ae->args) {
                    AstNode* a = std::holds_alternative<ExprNode*>(arg) ?
                        static_cast<AstNode*>(std::get<ExprNode*>(arg)) :
                        static_cast<AstNode*>(std::get<ValueExpr*>(arg));
                    // Args to opaque functions escape
                    check_escape(a, !is_local);
                }
                return;
            }
        };
        check_escape(node->expr, true);

        for (auto& name : let_names) {
            if (!escaping.count(name))
                local_non_escaping.insert(name);
        }
    }

    // Arena allocation for non-escaping values.
    // Currently analysis-only: the arena infrastructure is in place but
    // disabled by default until tuple-in-collection support is complete.
    // To enable: set arena != nullptr when local_non_escaping is non-empty.
    llvm::Value* arena = nullptr;
    auto saved_arena = current_arena_;

    // Track all heap-typed bindings for RC release at scope exit
    std::vector<TypedValue> scope_bindings;
    std::vector<bool> binding_is_arena; // parallel: true if arena-allocated

    for (auto* alias : node->aliases) {
        if (auto* va = dynamic_cast<ValueAlias*>(alias)) {
            std::string vname = va->identifier->name->value;
            // If non-escaping, set arena for this binding's allocation
            bool use_arena = arena && local_non_escaping.count(vname);
            if (use_arena) current_arena_ = arena;

            auto tv = codegen(va->expr);

            if (use_arena) current_arena_ = saved_arena;

            if (tv) {
                named_values_[vname] = tv;

                // Track for RC
                if (is_heap_type(tv.type) && tv.val) {
                    scope_bindings.push_back(tv);
                    binding_is_arena.push_back(use_arena);
                }

                if (debug_info_ && di_scope_ && di_builder_ && tv.val) {
                    auto* alloca = builder_->CreateAlloca(tv.val->getType(), nullptr, vname);
                    builder_->CreateStore(tv.val, alloca);
                    auto* di_var = di_builder_->createAutoVariable(
                        di_scope_, vname, di_file_, va->source_context.line, di_type_for(tv.type));
                    di_builder_->insertDeclare(alloca, di_var, di_builder_->createExpression(),
                        DILocation::get(*context_, va->source_context.line,
                                        va->source_context.column, di_scope_),
                        builder_->GetInsertBlock());
                    tv.val = builder_->CreateLoad(tv.val->getType(), alloca, vname);
                    named_values_[vname] = tv;
                }
            }
        } else if (auto* la = dynamic_cast<LambdaAlias*>(alias)) {
            auto tv = codegen_lambda_alias(la);
            if (tv && is_heap_type(tv.type) && tv.val) {
                scope_bindings.push_back(tv);
                binding_is_arena.push_back(false); // closures always heap
            }
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

    auto result = codegen(node->expr);

    // RC: release let-bound heap values going out of scope.
    // Inc the result first in case it references a binding.
    if (!scope_bindings.empty() && result && result.val) {
        emit_rc_inc(result.val, result.type);
        for (size_t i = 0; i < scope_bindings.size(); i++) {
            if (i < binding_is_arena.size() && binding_is_arena[i])
                continue; // arena-allocated: will be freed in bulk
            emit_rc_dec(scope_bindings[i].val, scope_bindings[i].type);
        }
    }

    // Destroy arena (frees all non-escaping allocations in bulk)
    if (arena)
        builder_->CreateCall(rt_arena_destroy_, {arena});

    current_arena_ = saved_arena;
    return result;
}

// ===== If Expression =====

TypedValue Codegen::codegen_if(IfExpr* node) {
    set_debug_loc(node->source_context);
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
    LType* phi_type = then_end ? then_tv.val->getType() : else_tv.val->getType();
    auto phi = builder_->CreatePHI(phi_type, phi_count);
    if (then_end) phi->addIncoming(then_tv.val, then_end);
    if (else_end) phi->addIncoming(else_tv.val, else_end);
    return {phi, then_tv.type, then_tv.subtypes};
}

// ===== Identifier =====

TypedValue Codegen::codegen_identifier(IdentifierExpr* node) {
    set_debug_loc(node->source_context);
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
        auto tag_ty = LType::getInt64Ty(*context_);
        auto i64_ty = LType::getInt64Ty(*context_);

        if (adt_it->second.is_recursive) {
            // Recursive ADT: heap-allocate via runtime
            auto* node_ptr = builder_->CreateCall(rt_adt_alloc_,
                {ConstantInt::get(tag_ty, adt_it->second.tag),
                 ConstantInt::get(i64_ty, 0)}, "adt_node");
            return {node_ptr, CType::ADT};
        } else {
            // Non-recursive: flat struct {i8, i64*max_arity}
            std::vector<LType*> fields = {tag_ty};
            for (int f = 0; f < adt_it->second.max_arity; f++)
                fields.push_back(i64_ty);
            auto* struct_type = StructType::get(*context_, fields);
            Value* val = UndefValue::get(struct_type);
            val = builder_->CreateInsertValue(val, ConstantInt::get(tag_ty, adt_it->second.tag), {0});
            return {val, CType::ADT};
        }
    }
    // Check if it's a non-zero-arity ADT constructor (used as a function reference)
    if (adt_it != adt_constructors_.end()) {
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
            builder_->CreateCall(rt_adt_get_tag_, {exc_val.val}), i64_ty);
        auto field = builder_->CreateCall(rt_adt_get_field_, {exc_val.val, ConstantInt::get(i64_ty, 0)});
        payload_val = builder_->CreateIntToPtr(field, PointerType::get(*context_, 0));
    } else {
        // Fallback: treat as integer tag with no payload
        tag_val = exc_val.val;
        if (tag_val->getType() != i64_ty)
            tag_val = builder_->CreateZExtOrTrunc(tag_val, i64_ty);
        payload_val = ConstantPointerNull::get(PointerType::get(*context_, 0));
    }

    builder_->CreateCall(rt_raise_, {tag_val, payload_val});
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
    auto jmp_buf_ptr = builder_->CreateCall(rt_try_begin_, {}, "jmp.buf");
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
        builder_->CreateCall(rt_try_end_, {});
        if (!try_val) try_val = {ConstantInt::get(i64_ty, 0), CType::INT};
        builder_->CreateBr(merge_bb);
        try_end_bb = builder_->GetInsertBlock();
    } else {
        if (!try_val) try_val = {ConstantInt::get(i64_ty, 0), CType::INT};
    }

    // Catch body: get exception tag and payload, pattern match
    builder_->SetInsertPoint(catch_bb);
    auto exc_tag = builder_->CreateCall(rt_get_exc_sym_, {}, "exc.tag");
    auto exc_payload = builder_->CreateCall(rt_get_exc_msg_, {}, "exc.payload");

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
                auto ctor_it = adt_constructors_.find(cpat->constructor_name);
                if (ctor_it != adt_constructors_.end()) {
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
                builder_->CreateCall(rt_raise_, {exc_tag, exc_payload});
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
    LType* result_llvm = try_end_bb ? try_val.val->getType()
        : (!catch_results.empty() ? catch_results[0].first.val->getType() : LType::getInt64Ty(*context_));

    unsigned pred_count = (try_end_bb ? 1 : 0) + catch_results.size();
    if (pred_count == 0) return try_val;

    auto phi = builder_->CreatePHI(result_llvm, pred_count, "try.result");
    if (try_end_bb) phi->addIncoming(try_val.val, try_end_bb);
    for (auto& [tv, bb] : catch_results) {
        Value* incoming = tv.val;
        if (incoming->getType() != result_llvm)
            incoming = Constant::getNullValue(result_llvm);
        phi->addIncoming(incoming, bb);
    }
    return {phi, result_type};
}

} // namespace yona::compiler::codegen
