//
// Codegen — Collection code generation
//
// Tuples, sequences, sets, dicts, cons, join.
//

#include "Codegen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <iostream>

namespace yona::compiler::codegen {
using namespace llvm;
using LType = llvm::Type;

static std::string ctype_to_string(CType ct) {
    switch (ct) {
        case CType::INT: return "INT";
        case CType::FLOAT: return "FLOAT";
        case CType::BOOL: return "BOOL";
        case CType::STRING: return "STRING";
        case CType::SEQ: return "SEQ";
        case CType::TUPLE: return "TUPLE";
        case CType::UNIT: return "UNIT";
        case CType::FUNCTION: return "FUNCTION";
        case CType::SYMBOL: return "SYMBOL";
        case CType::PROMISE: return "PROMISE";
        case CType::SET: return "SET";
        case CType::DICT: return "DICT";
        case CType::ADT: return "ADT";
        case CType::BYTES: return "BYTES";
    }
    return "INT";
}

TypedValue Codegen::codegen_tuple(TupleExpr* node) {
    set_debug_loc(node->source_context);
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
    set_debug_loc(node->source_context);
    size_t n = node->values.size();
    auto i64_ty = LType::getInt64Ty(*context_);
    auto count = ConstantInt::get(i64_ty, n);

    Value* seq;
    if (current_arena_) {
        // Arena allocation: allocate payload and set length manually
        auto payload_bytes = ConstantInt::get(i64_ty, (n + 1) * sizeof(int64_t));
        seq = emit_arena_alloc(1 /* RC_TYPE_SEQ */, payload_bytes);
        // Set length at seq[0]
        builder_->CreateStore(count, seq);
    } else {
        seq = builder_->CreateCall(rt_seq_alloc_, {count}, "seq");
    }

    CType elem_type = CType::INT;
    for (size_t i = 0; i < n; i++) {
        auto tv = codegen(node->values[i]);
        if (!tv) return {};
        if (i == 0) elem_type = tv.type;
        else if (tv.type != elem_type)
            report_error(node->values[i]->source_context,
                "type error: heterogeneous sequence — expected " + ctype_to_string(elem_type)
                + " but got " + ctype_to_string(tv.type));
        auto idx = ConstantInt::get(i64_ty, i);
        Value* store_val = tv.val;
        if (store_val->getType()->isStructTy()) {
            // Box: heap-allocate the struct, store pointer as i64
            auto* alloca = builder_->CreateAlloca(store_val->getType());
            builder_->CreateStore(store_val, alloca);
            uint64_t sz = module_->getDataLayout().getTypeAllocSize(store_val->getType());
            store_val = builder_->CreateCall(rt_box_, {alloca, ConstantInt::get(i64_ty, sz)});
            store_val = builder_->CreatePtrToInt(store_val, i64_ty);
        } else if (store_val->getType()->isPointerTy()) {
            store_val = builder_->CreatePtrToInt(store_val, i64_ty);
        } else if (store_val->getType()->isDoubleTy()) {
            store_val = builder_->CreateBitCast(store_val, i64_ty);
        } else if (store_val->getType() != i64_ty) {
            store_val = builder_->CreateZExtOrTrunc(store_val, i64_ty);
        }
        builder_->CreateCall(rt_seq_set_, {seq, idx, store_val});
    }
    return {seq, CType::SEQ, {elem_type}};
}

TypedValue Codegen::codegen_set(SetExpr* node) {
    set_debug_loc(node->source_context);
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
    set_debug_loc(node->source_context);
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
    set_debug_loc(node->source_context);
    auto elem = codegen(node->left);
    auto seq = codegen(node->right);
    if (!elem || !seq) return {};
    // Coerce types for rt_seq_cons
    Value* seq_ptr = seq.val;
    if (seq_ptr->getType()->isIntegerTy())
        seq_ptr = builder_->CreateIntToPtr(seq_ptr, PointerType::get(*context_, 0));
    Value* elem_val = elem.val;
    if (elem_val->getType()->isPointerTy())
        elem_val = builder_->CreatePtrToInt(elem_val, LType::getInt64Ty(*context_));
    return {builder_->CreateCall(rt_seq_cons_, {elem_val, seq_ptr}, "cons"), CType::SEQ, {elem.type}};
}

TypedValue Codegen::codegen_join(JoinExpr* node) {
    set_debug_loc(node->source_context);
    auto left = codegen(node->left);
    auto right = codegen(node->right);
    if (!left || !right) return {};

    if (left.type == CType::STRING || right.type == CType::STRING)
        return {builder_->CreateCall(rt_string_concat_, {left.val, right.val}), CType::STRING};
    // Join (++) always produces a sequence. Both operands must be sequences.
    // If an operand is typed as INT (element type not propagated from sequence
    // destructuring), the i64 value is actually a pointer to a sequence —
    // cast to ptr. This is semantically correct: ++ only operates on sequences.
    auto as_seq = [&](const TypedValue& tv) -> Value* {
        if (tv.val->getType()->isPointerTy()) return tv.val;
        return builder_->CreateIntToPtr(tv.val, PointerType::get(*context_, 0));
    };
    return {builder_->CreateCall(rt_seq_join_, {as_seq(left), as_seq(right)}), CType::SEQ};
}

// ===== Generator / Comprehension codegen =====
//
// Generators compile to counted loops over the source collection.
// [expr | x = src]       → alloc result[len], loop i=0..len, x=src[i], result[i]=expr
// [expr | x = src, if g] → two-pass: count matches, then fill
// {expr | x = src}       → alloc set[len], loop with set_put
// {k:v | x = src}        → alloc dict[len], loop with dict_set

// Helper: extract the binding variable name from a collection extractor
static std::string extractor_var_name(CollectionExtractorExpr* ext) {
    if (ext->get_type() == AST_VALUE_COLLECTION_EXTRACTOR_EXPR) {
        auto* ve = static_cast<ValueCollectionExtractorExpr*>(ext);
        if (auto* id = std::get_if<IdentifierExpr*>(&ve->expr))
            return (*id)->name->value;
    }
    return "_";
}

TypedValue Codegen::codegen_seq_generator(SeqGeneratorExpr* node) {
    set_debug_loc(node->source_context);
    auto* ext = static_cast<ValueCollectionExtractorExpr*>(node->collectionExtractor);
    if (!ext || !ext->collection) {
        report_error(node->source_context, "sequence generator missing source collection");
        return {};
    }

    auto src = codegen(ext->collection);
    if (!src) return {};

    auto i64_ty = LType::getInt64Ty(*context_);
    auto ptr_ty = PointerType::get(*context_, 0);

    // Ensure source is a pointer (seq)
    Value* src_ptr = src.val;
    if (!src_ptr->getType()->isPointerTy())
        src_ptr = builder_->CreateIntToPtr(src_ptr, ptr_ty);

    auto* src_len = builder_->CreateCall(rt_seq_length_, {src_ptr}, "src_len");

    bool has_guard = ext->condition != nullptr;
    std::string var_name = extractor_var_name(node->collectionExtractor);

    auto* func = builder_->GetInsertBlock()->getParent();

    if (!has_guard) {
        // Simple case: no guard, result has same length as source
        auto* result = builder_->CreateCall(rt_seq_alloc_, {src_len}, "gen_result");

        // Loop: for i = 0 .. src_len
        auto* loop_bb = BasicBlock::Create(*context_, "gen.loop", func);
        auto* body_bb = BasicBlock::Create(*context_, "gen.body", func);
        auto* done_bb = BasicBlock::Create(*context_, "gen.done", func);

        // Entry: i = 0
        auto* zero = ConstantInt::get(i64_ty, 0);
        builder_->CreateBr(loop_bb);

        // Loop header: phi for i
        builder_->SetInsertPoint(loop_bb);
        auto* i_phi = builder_->CreatePHI(i64_ty, 2, "i");
        i_phi->addIncoming(zero, loop_bb->getSinglePredecessor());
        auto* cond = builder_->CreateICmpSLT(i_phi, src_len, "gen.cond");
        builder_->CreateCondBr(cond, body_bb, done_bb);

        // Body: x = src[i]; result[i] = reducer(x)
        builder_->SetInsertPoint(body_bb);
        auto* elem = builder_->CreateCall(rt_seq_get_, {src_ptr, i_phi}, "elem");

        auto saved = named_values_;
        named_values_[var_name] = {elem, CType::INT};

        auto body_val = codegen(node->reducerExpr);

        Value* store_val = body_val.val;
        if (store_val->getType()->isPointerTy())
            store_val = builder_->CreatePtrToInt(store_val, i64_ty);
        else if (store_val->getType()->isDoubleTy())
            store_val = builder_->CreateBitCast(store_val, i64_ty);

        builder_->CreateCall(rt_seq_set_, {result, i_phi, store_val});

        auto* i_next = builder_->CreateAdd(i_phi, ConstantInt::get(i64_ty, 1), "i.next");
        i_phi->addIncoming(i_next, builder_->GetInsertBlock());
        builder_->CreateBr(loop_bb);

        named_values_ = saved;

        builder_->SetInsertPoint(done_bb);
        return {result, CType::SEQ};
    } else {
        // Guard case: two-pass
        // Pass 1: count matching elements
        auto* count_loop_bb = BasicBlock::Create(*context_, "gen.count.loop", func);
        auto* count_body_bb = BasicBlock::Create(*context_, "gen.count.body", func);
        auto* count_inc_bb = BasicBlock::Create(*context_, "gen.count.inc", func);
        auto* count_next_bb = BasicBlock::Create(*context_, "gen.count.next", func);
        auto* alloc_bb = BasicBlock::Create(*context_, "gen.alloc", func);

        auto* zero = ConstantInt::get(i64_ty, 0);
        auto* one = ConstantInt::get(i64_ty, 1);
        builder_->CreateBr(count_loop_bb);

        builder_->SetInsertPoint(count_loop_bb);
        auto* ci_phi = builder_->CreatePHI(i64_ty, 2, "ci");
        auto* cnt_phi = builder_->CreatePHI(i64_ty, 2, "cnt");
        ci_phi->addIncoming(zero, count_loop_bb->getSinglePredecessor());
        cnt_phi->addIncoming(zero, count_loop_bb->getSinglePredecessor());
        auto* c_cond = builder_->CreateICmpSLT(ci_phi, src_len);
        builder_->CreateCondBr(c_cond, count_body_bb, alloc_bb);

        builder_->SetInsertPoint(count_body_bb);
        auto* c_elem = builder_->CreateCall(rt_seq_get_, {src_ptr, ci_phi}, "c_elem");
        auto saved = named_values_;
        named_values_[var_name] = {c_elem, CType::INT};
        auto guard_val = codegen(ext->condition);
        named_values_ = saved;
        // Guard should be bool (i1) or int (i64 != 0)
        Value* guard_bool = guard_val.val;
        if (guard_bool->getType() == i64_ty)
            guard_bool = builder_->CreateICmpNE(guard_bool, zero);
        builder_->CreateCondBr(guard_bool, count_inc_bb, count_next_bb);

        builder_->SetInsertPoint(count_inc_bb);
        auto* cnt_inc = builder_->CreateAdd(cnt_phi, one);
        builder_->CreateBr(count_next_bb);

        builder_->SetInsertPoint(count_next_bb);
        auto* cnt_merged = builder_->CreatePHI(i64_ty, 2, "cnt.m");
        cnt_merged->addIncoming(cnt_phi, count_body_bb);
        cnt_merged->addIncoming(cnt_inc, count_inc_bb);
        auto* ci_next = builder_->CreateAdd(ci_phi, one);
        ci_phi->addIncoming(ci_next, count_next_bb);
        cnt_phi->addIncoming(cnt_merged, count_next_bb);
        builder_->CreateBr(count_loop_bb);

        // Pass 2: allocate and fill
        builder_->SetInsertPoint(alloc_bb);
        auto* result = builder_->CreateCall(rt_seq_alloc_, {cnt_phi}, "gen_result");

        auto* fill_loop_bb = BasicBlock::Create(*context_, "gen.fill.loop", func);
        auto* fill_body_bb = BasicBlock::Create(*context_, "gen.fill.body", func);
        auto* fill_store_bb = BasicBlock::Create(*context_, "gen.fill.store", func);
        auto* fill_next_bb = BasicBlock::Create(*context_, "gen.fill.next", func);
        auto* done_bb = BasicBlock::Create(*context_, "gen.done", func);

        builder_->CreateBr(fill_loop_bb);

        builder_->SetInsertPoint(fill_loop_bb);
        auto* fi_phi = builder_->CreatePHI(i64_ty, 2, "fi");
        auto* ri_phi = builder_->CreatePHI(i64_ty, 2, "ri");
        fi_phi->addIncoming(zero, alloc_bb);
        ri_phi->addIncoming(zero, alloc_bb);
        auto* f_cond = builder_->CreateICmpSLT(fi_phi, src_len);
        builder_->CreateCondBr(f_cond, fill_body_bb, done_bb);

        builder_->SetInsertPoint(fill_body_bb);
        auto* f_elem = builder_->CreateCall(rt_seq_get_, {src_ptr, fi_phi}, "f_elem");
        named_values_[var_name] = {f_elem, CType::INT};
        auto guard_val2 = codegen(ext->condition);
        Value* guard_bool2 = guard_val2.val;
        if (guard_bool2->getType() == i64_ty)
            guard_bool2 = builder_->CreateICmpNE(guard_bool2, zero);
        builder_->CreateCondBr(guard_bool2, fill_store_bb, fill_next_bb);

        builder_->SetInsertPoint(fill_store_bb);
        named_values_[var_name] = {f_elem, CType::INT};
        auto body_val = codegen(node->reducerExpr);
        Value* store_val = body_val.val;
        if (store_val->getType()->isPointerTy())
            store_val = builder_->CreatePtrToInt(store_val, i64_ty);
        else if (store_val->getType()->isDoubleTy())
            store_val = builder_->CreateBitCast(store_val, i64_ty);
        builder_->CreateCall(rt_seq_set_, {result, ri_phi, store_val});
        auto* ri_next = builder_->CreateAdd(ri_phi, one);
        builder_->CreateBr(fill_next_bb);

        builder_->SetInsertPoint(fill_next_bb);
        auto* ri_merged = builder_->CreatePHI(i64_ty, 2, "ri.m");
        ri_merged->addIncoming(ri_phi, fill_body_bb);
        ri_merged->addIncoming(ri_next, fill_store_bb);
        auto* fi_next = builder_->CreateAdd(fi_phi, one);
        fi_phi->addIncoming(fi_next, fill_next_bb);
        ri_phi->addIncoming(ri_merged, fill_next_bb);
        builder_->CreateBr(fill_loop_bb);

        named_values_ = saved;

        builder_->SetInsertPoint(done_bb);
        return {result, CType::SEQ};
    }
}

TypedValue Codegen::codegen_set_generator(SetGeneratorExpr* node) {
    set_debug_loc(node->source_context);
    auto* ext = static_cast<ValueCollectionExtractorExpr*>(node->collectionExtractor);
    if (!ext || !ext->collection) {
        report_error(node->source_context, "set generator missing source collection");
        return {};
    }

    auto src = codegen(ext->collection);
    if (!src) return {};

    auto i64_ty = LType::getInt64Ty(*context_);
    auto ptr_ty = PointerType::get(*context_, 0);

    Value* src_ptr = src.val;
    if (!src_ptr->getType()->isPointerTy())
        src_ptr = builder_->CreateIntToPtr(src_ptr, ptr_ty);

    auto* src_len = builder_->CreateCall(rt_seq_length_, {src_ptr}, "src_len");
    auto* result = builder_->CreateCall(rt_set_alloc_, {src_len}, "gen_set");

    std::string var_name = extractor_var_name(node->collectionExtractor);
    auto* func = builder_->GetInsertBlock()->getParent();
    auto* zero = ConstantInt::get(i64_ty, 0);
    auto* one = ConstantInt::get(i64_ty, 1);

    auto* loop_bb = BasicBlock::Create(*context_, "setgen.loop", func);
    auto* body_bb = BasicBlock::Create(*context_, "setgen.body", func);
    auto* done_bb = BasicBlock::Create(*context_, "setgen.done", func);

    builder_->CreateBr(loop_bb);

    builder_->SetInsertPoint(loop_bb);
    auto* i_phi = builder_->CreatePHI(i64_ty, 2, "i");
    i_phi->addIncoming(zero, loop_bb->getSinglePredecessor());
    auto* cond = builder_->CreateICmpSLT(i_phi, src_len);
    builder_->CreateCondBr(cond, body_bb, done_bb);

    builder_->SetInsertPoint(body_bb);
    auto* elem = builder_->CreateCall(rt_seq_get_, {src_ptr, i_phi}, "elem");

    auto saved = named_values_;
    named_values_[var_name] = {elem, CType::INT};

    auto body_val = codegen(node->reducerExpr);
    Value* store_val = body_val.val;
    if (store_val->getType()->isPointerTy())
        store_val = builder_->CreatePtrToInt(store_val, i64_ty);

    builder_->CreateCall(rt_set_put_, {result, i_phi, store_val});

    auto* i_next = builder_->CreateAdd(i_phi, one);
    i_phi->addIncoming(i_next, builder_->GetInsertBlock());
    builder_->CreateBr(loop_bb);

    named_values_ = saved;

    builder_->SetInsertPoint(done_bb);
    return {result, CType::SET};
}

TypedValue Codegen::codegen_dict_generator(DictGeneratorExpr* node) {
    set_debug_loc(node->source_context);
    auto* ext = static_cast<ValueCollectionExtractorExpr*>(node->collectionExtractor);
    if (!ext || !ext->collection) {
        report_error(node->source_context, "dict generator missing source collection");
        return {};
    }

    auto src = codegen(ext->collection);
    if (!src) return {};

    auto i64_ty = LType::getInt64Ty(*context_);
    auto ptr_ty = PointerType::get(*context_, 0);

    Value* src_ptr = src.val;
    if (!src_ptr->getType()->isPointerTy())
        src_ptr = builder_->CreateIntToPtr(src_ptr, ptr_ty);

    auto* src_len = builder_->CreateCall(rt_seq_length_, {src_ptr}, "src_len");
    auto* result = builder_->CreateCall(rt_dict_alloc_, {src_len}, "gen_dict");

    std::string var_name = extractor_var_name(node->collectionExtractor);
    auto* func = builder_->GetInsertBlock()->getParent();
    auto* zero = ConstantInt::get(i64_ty, 0);
    auto* one = ConstantInt::get(i64_ty, 1);

    auto* loop_bb = BasicBlock::Create(*context_, "dictgen.loop", func);
    auto* body_bb = BasicBlock::Create(*context_, "dictgen.body", func);
    auto* done_bb = BasicBlock::Create(*context_, "dictgen.done", func);

    builder_->CreateBr(loop_bb);

    builder_->SetInsertPoint(loop_bb);
    auto* i_phi = builder_->CreatePHI(i64_ty, 2, "i");
    i_phi->addIncoming(zero, loop_bb->getSinglePredecessor());
    auto* cond = builder_->CreateICmpSLT(i_phi, src_len);
    builder_->CreateCondBr(cond, body_bb, done_bb);

    builder_->SetInsertPoint(body_bb);
    auto* elem = builder_->CreateCall(rt_seq_get_, {src_ptr, i_phi}, "elem");

    auto saved = named_values_;
    named_values_[var_name] = {elem, CType::INT};

    auto key_val = codegen(node->reducerExpr->key);
    auto val_val = codegen(node->reducerExpr->value);

    Value* key_i64 = key_val.val;
    if (key_i64->getType()->isPointerTy())
        key_i64 = builder_->CreatePtrToInt(key_i64, i64_ty);
    Value* val_i64 = val_val.val;
    if (val_i64->getType()->isPointerTy())
        val_i64 = builder_->CreatePtrToInt(val_i64, i64_ty);

    builder_->CreateCall(rt_dict_set_, {result, i_phi, key_i64, val_i64});

    auto* i_next = builder_->CreateAdd(i_phi, one);
    i_phi->addIncoming(i_next, builder_->GetInsertBlock());
    builder_->CreateBr(loop_bb);

    named_values_ = saved;

    builder_->SetInsertPoint(done_bb);
    return {result, CType::DICT};
}

} // namespace yona::compiler::codegen
