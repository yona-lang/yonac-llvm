//
// Codegen — Case expression / pattern matching
//

#include "Codegen.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <iostream>

namespace yona::compiler::codegen {
using namespace llvm;
using LType = llvm::Type;

// ===== Pattern match helpers (extracted from codegen_case) =====

bool Codegen::codegen_pattern_value(PatternValue* pv, const TypedValue& scrutinee,
                                     BasicBlock* body_bb, BasicBlock* next_bb) {
    if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
        named_values_[(*id)->name->value] = scrutinee;
        builder_->CreateBr(body_bb);
    } else if (auto* sym = std::get_if<SymbolExpr*>(&pv->expr)) {
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
    return false;
}

bool Codegen::codegen_pattern_headtail(HeadTailsPattern* htp, CaseExpr* node,
                                        CaseClause* clause, const TypedValue& scrutinee,
                                        Value* seq_ptr,
                                        BasicBlock* body_bb, BasicBlock* next_bb) {
    auto i64_ty = LType::getInt64Ty(*context_);

    if (htp->heads.size() == 1) {
        auto* count_ptr = builder_->CreateGEP(i64_ty, seq_ptr,
            {ConstantInt::get(i64_ty, 0)});
        auto* count = builder_->CreateLoad(i64_ty, count_ptr, "seq_count");
        builder_->CreateCondBr(
            builder_->CreateICmpSGT(count, ConstantInt::get(i64_ty, 0)),
            body_bb, next_bb);
    } else {
        auto len = builder_->CreateCall(rt_.seq_length_, {seq_ptr});
        auto min_len = ConstantInt::get(i64_ty, htp->heads.size());
        builder_->CreateCondBr(builder_->CreateICmpSGE(len, min_len), body_bb, next_bb);
    }

    CType elem_type = (!scrutinee.subtypes.empty()) ? scrutinee.subtypes[0] : CType::INT;

    builder_->SetInsertPoint(body_bb);
    for (size_t hi = 0; hi < htp->heads.size(); hi++) {
        Value* hv;
        if (hi == 0 && htp->heads.size() == 1)
            hv = builder_->CreateCall(rt_.seq_head_, {seq_ptr}, "head");
        else
            hv = builder_->CreateCall(rt_.seq_get_, {seq_ptr, ConstantInt::get(i64_ty, hi)});
        Value* elem_val = hv;
        if (elem_type == CType::SEQ || elem_type == CType::STRING ||
            elem_type == CType::FUNCTION || elem_type == CType::ADT ||
            elem_type == CType::SET || elem_type == CType::DICT)
            elem_val = builder_->CreateIntToPtr(hv, PointerType::get(*context_, 0));
        auto* hp = htp->heads[hi];
        if (hp->get_type() == AST_PATTERN_VALUE) {
            auto* pv = static_cast<PatternValue*>(hp);
            if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                named_values_[(*id)->name->value] = {elem_val, elem_type};
        }
    }
    if (htp->tail && htp->tail->get_type() == AST_PATTERN_VALUE) {
        if (node->expr->get_type() == AST_IDENTIFIER_EXPR) {
            auto scrut_name = static_cast<IdentifierExpr*>(node->expr)->name->value;
            if (count_identifier_refs(clause->body, scrut_name) > 0)
                emit_rc_inc(seq_ptr, CType::SEQ);
        }
        auto tv = builder_->CreateCall(rt_.seq_tail_, {seq_ptr});
        auto* pv = static_cast<PatternValue*>(htp->tail);
        if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
            named_values_[(*id)->name->value] = {tv, CType::SEQ, scrutinee.subtypes};
    }
    return true;
}

bool Codegen::codegen_pattern_seq(SeqPattern* sp, const TypedValue& scrutinee,
                                   BasicBlock* body_bb, BasicBlock* next_bb) {
    if (sp->patterns.empty()) {
        Value* seq_val = scrutinee.val;
        if (!seq_val->getType()->isPointerTy())
            seq_val = builder_->CreateIntToPtr(seq_val, PointerType::get(*context_, 0));
        auto* count_ptr = builder_->CreateGEP(LType::getInt64Ty(*context_),
            seq_val, {ConstantInt::get(LType::getInt64Ty(*context_), 0)});
        auto* count = builder_->CreateLoad(LType::getInt64Ty(*context_), count_ptr, "seq_count");
        builder_->CreateCondBr(
            builder_->CreateICmpEQ(count, ConstantInt::get(LType::getInt64Ty(*context_), 0)),
            body_bb, next_bb);
    } else {
        builder_->CreateBr(body_bb);
    }
    return false;
}

bool Codegen::codegen_pattern_tuple(TuplePattern* tp, const TypedValue& scrutinee,
                                     BasicBlock* body_bb, BasicBlock* next_bb) {
    auto i64_ty = LType::getInt64Ty(*context_);
    auto* fn = builder_->GetInsertBlock()->getParent();
    Value* tuple_val = scrutinee.val;
    bool is_ptr = tuple_val->getType()->isPointerTy();

    if (tuple_val->getType()->isIntegerTy()) {
        tuple_val = builder_->CreateIntToPtr(tuple_val, PointerType::get(*context_, 0));
        is_ptr = true;
    }

    for (size_t ti = 0; ti < tp->patterns.size(); ti++) {
        Value* elem;
        if (is_ptr) {
            auto* gep = builder_->CreateGEP(i64_ty, tuple_val,
                {ConstantInt::get(i64_ty, ti + 2)}, "tuple_gep");
            elem = builder_->CreateLoad(i64_ty, gep, "tuple_elem");
        } else {
            elem = builder_->CreateExtractValue(tuple_val, {(unsigned)ti});
        }
        CType et = (ti < scrutinee.subtypes.size()) ? scrutinee.subtypes[ti] : CType::INT;
        auto* sub = tp->patterns[ti];
        if (sub->get_type() == AST_PATTERN_VALUE) {
            auto* pv = static_cast<PatternValue*>(sub);
            if (auto* sym = std::get_if<SymbolExpr*>(&pv->expr)) {
                int64_t sym_id = intern_symbol((*sym)->value);
                auto* sym_val = ConstantInt::get(i64_ty, sym_id);
                Value* cmp_val = elem;
                if (cmp_val->getType() != i64_ty) {
                    if (cmp_val->getType()->isPointerTy())
                        cmp_val = builder_->CreatePtrToInt(cmp_val, i64_ty);
                    else if (cmp_val->getType()->isIntegerTy())
                        cmp_val = builder_->CreateZExtOrTrunc(cmp_val, i64_ty);
                }
                auto* match_bb = BasicBlock::Create(*context_, "tuple.sym.match", fn);
                builder_->CreateCondBr(builder_->CreateICmpEQ(cmp_val, sym_val), match_bb, next_bb);
                builder_->SetInsertPoint(match_bb);
            } else if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                named_values_[(*id)->name->value] = {elem, et};
            } else if (auto* lit = std::get_if<LiteralExpr<void*>*>(&pv->expr)) {
                auto* an = reinterpret_cast<AstNode*>(*lit);
                if (an->get_type() == AST_INTEGER_EXPR) {
                    auto* ie = static_cast<IntegerExpr*>(an);
                    Value* cmp_val = elem;
                    if (cmp_val->getType() != i64_ty)
                        cmp_val = builder_->CreateZExtOrTrunc(cmp_val, i64_ty);
                    auto* match_bb = BasicBlock::Create(*context_, "tuple.lit.match", fn);
                    builder_->CreateCondBr(
                        builder_->CreateICmpEQ(cmp_val, ConstantInt::get(i64_ty, ie->value)),
                        match_bb, next_bb);
                    builder_->SetInsertPoint(match_bb);
                }
            }
        }
        // AST_UNDERSCORE_PATTERN: wildcard, no action needed
    }
    builder_->CreateBr(body_bb);
    return false;
}

bool Codegen::codegen_pattern_constructor(ConstructorPattern* cp, const TypedValue& scrutinee,
                                           BasicBlock* body_bb, BasicBlock* next_bb) {
    auto ctor_it = types_.adt_constructors.find(cp->constructor_name);
    if (ctor_it == types_.adt_constructors.end()) {
        builder_->CreateBr(body_bb);
        return false;
    }

    int8_t tag = static_cast<int8_t>(ctor_it->second.tag);
    auto tag_ty = LType::getInt64Ty(*context_);
    auto i64_ty = LType::getInt64Ty(*context_);

    if (ctor_it->second.is_recursive) {
        auto scr_tag = builder_->CreateCall(rt_.adt_get_tag_, {scrutinee.val});
        builder_->CreateCondBr(
            builder_->CreateICmpEQ(scr_tag, ConstantInt::get(i64_ty, tag)),
            body_bb, next_bb);
        builder_->SetInsertPoint(body_bb);
        for (size_t fi = 0; fi < cp->sub_patterns.size(); fi++) {
            auto field_val = builder_->CreateCall(rt_.adt_get_field_,
                {scrutinee.val, ConstantInt::get(i64_ty, fi)});
            auto* sub_pat = cp->sub_patterns[fi];
            if (sub_pat->get_type() == AST_PATTERN_VALUE) {
                auto* pv = static_cast<PatternValue*>(sub_pat);
                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                    CType ftype = (fi < ctor_it->second.field_types.size())
                        ? ctor_it->second.field_types[fi] : CType::INT;
                    Value* typed_val = field_val;
                    if (ftype == CType::ADT || ftype == CType::SEQ ||
                        ftype == CType::STRING || ftype == CType::FUNCTION ||
                        ftype == CType::SET || ftype == CType::DICT)
                        typed_val = builder_->CreateIntToPtr(field_val,
                            PointerType::get(*context_, 0));
                    named_values_[(*id)->name->value] = {typed_val, ftype};
                }
            }
        }
    } else {
        auto scr_tag = builder_->CreateExtractValue(scrutinee.val, {0});
        builder_->CreateCondBr(
            builder_->CreateICmpEQ(scr_tag, ConstantInt::get(tag_ty, tag)),
            body_bb, next_bb);
        builder_->SetInsertPoint(body_bb);
        for (size_t fi = 0; fi < cp->sub_patterns.size(); fi++) {
            auto field_val = builder_->CreateExtractValue(scrutinee.val, {(unsigned)(fi + 1)});
            auto* sub_pat = cp->sub_patterns[fi];
            if (sub_pat->get_type() == AST_PATTERN_VALUE) {
                auto* pv = static_cast<PatternValue*>(sub_pat);
                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                    CType ftype = (fi < ctor_it->second.field_types.size())
                        ? ctor_it->second.field_types[fi] : CType::INT;
                    Value* typed_val = field_val;
                    if (ftype == CType::FUNCTION || ftype == CType::SEQ ||
                        ftype == CType::STRING || ftype == CType::ADT)
                        typed_val = builder_->CreateIntToPtr(field_val,
                            PointerType::get(*context_, 0));
                    named_values_[(*id)->name->value] = {typed_val, ftype};
                }
            }
        }
    }
    return true;
}

// Coerce a value to a target LLVM type for PHI node compatibility.
static Value* coerce_for_phi(Value* val, LType* target, IRBuilder<>& builder, LLVMContext& ctx) {
    auto* src = val->getType();
    if (src == target) return val;
    if (src->isIntegerTy() && target->isIntegerTy())
        return builder.CreateZExtOrTrunc(val, target);
    if (src->isPointerTy() && target->isIntegerTy())
        return builder.CreatePtrToInt(val, target);
    if (src->isIntegerTy() && target->isPointerTy())
        return builder.CreateIntToPtr(val, target);
    if (src->isStructTy() && target->isPointerTy()) {
        auto* alloca = builder.CreateAlloca(src);
        builder.CreateStore(val, alloca);
        return alloca;
    }
    return val;
}

static LType* common_phi_type(LType* a, LType* b, LLVMContext& ctx) {
    if (a == b) return a;
    if (a->isStructTy() || b->isStructTy())
        return PointerType::get(ctx, 0);
    if (a->isPointerTy() || b->isPointerTy())
        return PointerType::get(ctx, 0);
    if (a->isIntegerTy() && b->isIntegerTy()) {
        unsigned wa = a->getIntegerBitWidth(), wb = b->getIntegerBitWidth();
        return wa >= wb ? a : b;
    }
    return LType::getInt64Ty(ctx);
}

TypedValue Codegen::codegen_case(CaseExpr* node) {
    set_debug_loc(node->source_context);
    auto scrutinee = auto_await(codegen(node->expr));
    if (!scrutinee) return {};

    // If scrutinee is INT but patterns are ADT constructors, the value is
    // an ADT encoded as i64 (e.g., from a closure returning a heap-allocated ADT).
    // Convert it to the ADT type for pattern matching.
    if (scrutinee.type == CType::INT && !node->clauses.empty()) {
        auto* first_pat = node->clauses[0]->pattern;
        if (first_pat->get_type() == AST_CONSTRUCTOR_PATTERN) {
            auto* cp = static_cast<ConstructorPattern*>(first_pat);
            auto ctor_it = types_.adt_constructors.find(cp->constructor_name);
            if (ctor_it != types_.adt_constructors.end() && ctor_it->second.is_recursive) {
                // Convert i64 → ptr for recursive ADT
                scrutinee.val = builder_->CreateIntToPtr(scrutinee.val,
                    PointerType::get(*context_, 0));
                scrutinee.type = CType::ADT;
            }
        }
    }

    // Exhaustiveness check for ADT scrutinees
    if (scrutinee.type == CType::ADT) {
        // Find the ADT type name from the first constructor pattern
        std::string adt_type_name;
        bool has_wildcard = false;
        std::unordered_set<std::string> covered_ctors;

        for (auto* clause : node->clauses) {
            auto* pat = clause->pattern;
            if (pat->get_type() == AST_CONSTRUCTOR_PATTERN) {
                auto* cp = static_cast<ConstructorPattern*>(pat);
                covered_ctors.insert(cp->constructor_name);
                if (adt_type_name.empty()) {
                    auto it = types_.adt_constructors.find(cp->constructor_name);
                    if (it != types_.adt_constructors.end())
                        adt_type_name = it->second.type_name;
                }
            } else if (pat->get_type() == AST_UNDERSCORE_PATTERN ||
                       pat->get_type() == AST_PATTERN_VALUE) {
                has_wildcard = true;
            }
        }

        if (!adt_type_name.empty() && !has_wildcard) {
            // Check all constructors of this ADT are covered
            for (auto& [name, info] : types_.adt_constructors) {
                if (info.type_name == adt_type_name && covered_ctors.count(name) == 0) {
                    std::cerr << "Warning: non-exhaustive pattern match on " << adt_type_name
                              << " — missing constructor " << name << "\n";
                }
            }
        }
    }

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
            body_inline = codegen_pattern_value(static_cast<PatternValue*>(pat),
                                                 scrutinee, body_bb, next_bb);
        } else if (pat->get_type() == AST_HEAD_TAILS_PATTERN) {
            Value* seq_ptr = scrutinee.val;
            if (!seq_ptr->getType()->isPointerTy())
                seq_ptr = builder_->CreateIntToPtr(seq_ptr, PointerType::get(*context_, 0));
            body_inline = codegen_pattern_headtail(static_cast<HeadTailsPattern*>(pat),
                                                    node, clause, scrutinee, seq_ptr,
                                                    body_bb, next_bb);
        } else if (pat->get_type() == AST_SEQ_PATTERN) {
            body_inline = codegen_pattern_seq(static_cast<SeqPattern*>(pat),
                                               scrutinee, body_bb, next_bb);
        } else if (pat->get_type() == AST_TUPLE_PATTERN) {
            body_inline = codegen_pattern_tuple(static_cast<TuplePattern*>(pat),
                                                 scrutinee, body_bb, next_bb);
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
            body_inline = codegen_pattern_constructor(static_cast<ConstructorPattern*>(pat),
                                                       scrutinee, body_bb, next_bb);
        } else if (pat->get_type() == AST_RECORD_PATTERN) {
            // Named field pattern: Person { name = n, age = a }
            auto* rp = static_cast<RecordPattern*>(pat);
            auto ctor_it = types_.adt_constructors.find(rp->recordType);
            if (ctor_it != types_.adt_constructors.end()) {
                int8_t tag = static_cast<int8_t>(ctor_it->second.tag);
                auto tag_ty = LType::getInt64Ty(*context_);
                auto i64_ty = LType::getInt64Ty(*context_);

                if (ctor_it->second.is_recursive) {
                    auto scr_tag = builder_->CreateCall(rt_.adt_get_tag_, {scrutinee.val});
                    builder_->CreateCondBr(builder_->CreateICmpEQ(scr_tag, ConstantInt::get(tag_ty, tag)),
                                           body_bb, next_bb);
                    builder_->SetInsertPoint(body_bb);
                    for (auto& [name_expr, pattern] : rp->items) {
                        if (!name_expr) continue;
                        for (size_t fi = 0; fi < ctor_it->second.field_names.size(); fi++) {
                            if (ctor_it->second.field_names[fi] == name_expr->value) {
                                auto val = builder_->CreateCall(rt_.adt_get_field_,
                                    {scrutinee.val, ConstantInt::get(i64_ty, fi)});
                                if (pattern->get_type() == AST_PATTERN_VALUE) {
                                    auto* pv = static_cast<PatternValue*>(pattern);
                                    if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                                        named_values_[(*id)->name->value] = {val, CType::INT};
                                }
                                break;
                            }
                        }
                    }
                } else {
                    auto scr_tag = builder_->CreateExtractValue(scrutinee.val, {0});
                    builder_->CreateCondBr(builder_->CreateICmpEQ(scr_tag, ConstantInt::get(tag_ty, tag)),
                                           body_bb, next_bb);
                    builder_->SetInsertPoint(body_bb);
                    for (auto& [name_expr, pattern] : rp->items) {
                        if (!name_expr) continue;
                        for (size_t fi = 0; fi < ctor_it->second.field_names.size(); fi++) {
                            if (ctor_it->second.field_names[fi] == name_expr->value) {
                                CType ftype = (fi < ctor_it->second.field_types.size())
                                    ? ctor_it->second.field_types[fi] : CType::INT;
                                auto val = builder_->CreateExtractValue(scrutinee.val, {(unsigned)(fi + 1)});
                                if (ftype == CType::STRING || ftype == CType::SEQ || ftype == CType::ADT)
                                    val = builder_->CreateIntToPtr(val, PointerType::get(*context_, 0));
                                if (pattern->get_type() == AST_PATTERN_VALUE) {
                                    auto* pv = static_cast<PatternValue*>(pattern);
                                    if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                                        named_values_[(*id)->name->value] = {val, ftype};
                                }
                                break;
                            }
                        }
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

        // Guard expression: pattern | guard -> body
        if (clause->guard) {
            auto guard_val = codegen(clause->guard);
            if (!guard_val) return {};
            Value* cond = guard_val.val;
            if (guard_val.type == CType::INT)
                cond = builder_->CreateICmpNE(cond, ConstantInt::get(LType::getInt64Ty(*context_), 0));
            auto guarded_bb = BasicBlock::Create(*context_, "case.guarded." + std::to_string(i), fn);
            builder_->CreateCondBr(cond, guarded_bb, next_bb);
            builder_->SetInsertPoint(guarded_bb);
        }

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

    // Determine common PHI type across all branches
    LType* phi_type = results[0].first.val->getType();
    for (size_t ri = 1; ri < results.size(); ri++)
        phi_type = common_phi_type(phi_type, results[ri].first.val->getType(), *context_);

    auto phi = builder_->CreatePHI(phi_type, pred_count);
    for (auto& [tv, bb] : results) {
        Value* incoming = tv.val;
        if (incoming->getType() != phi_type) {
            // Insert coercion before the branch terminator in the source block
            builder_->SetInsertPoint(bb->getTerminator());
            incoming = coerce_for_phi(incoming, phi_type, *builder_, *context_);
            builder_->SetInsertPoint(merge_bb);
        }
        phi->addIncoming(incoming, bb);
    }
    for (auto it = pred_begin(merge_bb); it != pred_end(merge_bb); ++it) {
        bool found = false;
        for (auto& [tv, bb] : results) if (bb == *it) { found = true; break; }
        if (!found) phi->addIncoming(Constant::getNullValue(phi_type), *it);
    }
    return {phi, results[0].first.type, results[0].first.subtypes};
}

} // namespace yona::compiler::codegen
