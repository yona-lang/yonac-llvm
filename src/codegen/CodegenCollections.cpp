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
    auto i64_ty = LType::getInt64Ty(*context_);
    std::vector<Value*> elems;
    std::vector<CType> subtypes;

    for (auto* v : node->values) {
        auto tv = codegen(v);
        if (!tv) return {};
        // Convert all elements to i64 for uniform layout
        Value* i64_val = tv.val;
        if (i64_val->getType() == i64_ty) {
            // already i64
        } else if (i64_val->getType()->isPointerTy()) {
            i64_val = builder_->CreatePtrToInt(i64_val, i64_ty);
        } else if (i64_val->getType()->isDoubleTy()) {
            i64_val = builder_->CreateBitCast(i64_val, i64_ty);
        } else if (i64_val->getType()->isIntegerTy()) {
            i64_val = builder_->CreateZExtOrTrunc(i64_val, i64_ty);
        }
        elems.push_back(i64_val);
        subtypes.push_back(tv.type);
    }

    // Heap-allocate tuple with metadata for recursive destruction
    auto* tuple_ptr = builder_->CreateCall(rt_tuple_alloc_,
        {ConstantInt::get(i64_ty, elems.size())}, "tuple");
    int64_t tuple_heap_mask = 0;
    for (size_t i = 0; i < elems.size(); i++) {
        builder_->CreateCall(rt_tuple_set_,
            {tuple_ptr, ConstantInt::get(i64_ty, i), elems[i]});
        if (is_heap_type(subtypes[i]) && i < 64)
            tuple_heap_mask |= ((int64_t)1 << i);
    }
    if (tuple_heap_mask != 0)
        builder_->CreateCall(rt_tuple_set_heap_mask_,
            {tuple_ptr, ConstantInt::get(i64_ty, tuple_heap_mask)});
    auto* tuple_i64 = builder_->CreatePtrToInt(tuple_ptr, i64_ty, "tuple_i64");
    return {tuple_i64, CType::TUPLE, subtypes};
}

TypedValue Codegen::codegen_seq(ValuesSequenceExpr* node) {
    set_debug_loc(node->source_context);
    size_t n = node->values.size();
    auto i64_ty = LType::getInt64Ty(*context_);
    auto count = ConstantInt::get(i64_ty, n);

    Value* seq;
    if (current_arena_) {
        // Arena allocation: allocate payload and set length manually
        auto payload_bytes = ConstantInt::get(i64_ty, (n + 2) * sizeof(int64_t)); // +2 for count+heap_flag
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
    auto i64_ty = LType::getInt64Ty(*context_);
    auto zero = ConstantInt::get(i64_ty, 0);
    Value* dict = builder_->CreateCall(rt_dict_alloc_, {zero}, "dict");

    CType key_type = CType::INT, val_type = CType::INT;
    for (size_t i = 0; i < n; i++) {
        auto key_tv = codegen(node->values[i].first);
        auto val_tv = codegen(node->values[i].second);
        if (!key_tv || !val_tv) return {};
        if (i == 0) { key_type = key_tv.type; val_type = val_tv.type; }
        // Persistent put: dict = dict_put(dict, key, val)
        Value* key_val = key_tv.val;
        Value* val_val = val_tv.val;
        if (key_val->getType()->isPointerTy())
            key_val = builder_->CreatePtrToInt(key_val, i64_ty);
        if (val_val->getType()->isPointerTy())
            val_val = builder_->CreatePtrToInt(val_val, i64_ty);
        dict = builder_->CreateCall(rt_dict_put_, {dict, key_val, val_val}, "dict");
    }
    return {dict, CType::DICT, {key_type, val_type}};
}

TypedValue Codegen::codegen_cons(ConsLeftExpr* node) {
    set_debug_loc(node->source_context);
    auto elem = codegen(node->left);
    auto seq = codegen(node->right);
    if (!elem || !seq) return {};
    Value* seq_ptr = seq.val;
    Value* elem_val = elem.val;
    // Ensure correct types for rt_seq_cons(i64, ptr)
    if (elem_val->getType()->isPointerTy())
        elem_val = builder_->CreatePtrToInt(elem_val, LType::getInt64Ty(*context_));
    if (seq_ptr->getType()->isIntegerTy())
        seq_ptr = builder_->CreateIntToPtr(seq_ptr, PointerType::get(*context_, 0));
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
        // Simple case: no guard, result has same length as source.
        // Use head/tail iteration instead of indexed get for O(1) per
        // element (indexed get is O(n/32) for chunked seqs).
        auto* result = builder_->CreateCall(rt_seq_alloc_, {src_len}, "gen_result");
        auto ptr_ty = PointerType::get(*context_, 0);

        auto* loop_bb = BasicBlock::Create(*context_, "gen.loop", func);
        auto* body_bb = BasicBlock::Create(*context_, "gen.body", func);
        auto* done_bb = BasicBlock::Create(*context_, "gen.done", func);

        auto* zero = ConstantInt::get(i64_ty, 0);
        builder_->CreateBr(loop_bb);

        // Loop header: phi for index (i) and current seq cursor
        builder_->SetInsertPoint(loop_bb);
        auto* i_phi = builder_->CreatePHI(i64_ty, 2, "i");
        i_phi->addIncoming(zero, loop_bb->getSinglePredecessor());
        auto* cur_phi = builder_->CreatePHI(ptr_ty, 2, "cur");
        cur_phi->addIncoming(src_ptr, loop_bb->getSinglePredecessor());

        auto* is_empty = builder_->CreateCall(rt_seq_is_empty_, {cur_phi}, "gen.empty");
        auto* cond = builder_->CreateICmpEQ(is_empty, zero, "gen.cond");
        builder_->CreateCondBr(cond, body_bb, done_bb);

        // Body: x = head(cur); cur = tail(cur); result[i] = reducer(x)
        builder_->SetInsertPoint(body_bb);
        auto* elem = builder_->CreateCall(rt_seq_head_, {cur_phi}, "elem");
        auto* next_cur = builder_->CreateCall(rt_seq_tail_, {cur_phi}, "cur.next");

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
        cur_phi->addIncoming(next_cur, builder_->GetInsertBlock());
        builder_->CreateBr(loop_bb);

        named_values_ = saved;

        builder_->SetInsertPoint(done_bb);
        return {result, CType::SEQ};
    } else {
        // Guard case: single-pass indexed with over-allocation.
        // Indexed get is O(1) for flat seqs, O(n/32) for chunked — much
        // better than head/tail which is O(n) per call on flat seqs due
        // to memmove. Over-allocate at source length, fill matching
        // elements, then adjust count.
        auto* zero = ConstantInt::get(i64_ty, 0);
        auto* one = ConstantInt::get(i64_ty, 1);

        auto* result = builder_->CreateCall(rt_seq_alloc_, {src_len}, "gen_result");

        auto* loop_bb = BasicBlock::Create(*context_, "gen.loop", func);
        auto* body_bb = BasicBlock::Create(*context_, "gen.body", func);
        auto* guard_bb = BasicBlock::Create(*context_, "gen.guard", func);
        auto* next_bb = BasicBlock::Create(*context_, "gen.next", func);
        auto* done_bb = BasicBlock::Create(*context_, "gen.done", func);

        builder_->CreateBr(loop_bb);

        builder_->SetInsertPoint(loop_bb);
        auto* i_phi = builder_->CreatePHI(i64_ty, 2, "i");
        auto* wi_phi = builder_->CreatePHI(i64_ty, 2, "wi");
        i_phi->addIncoming(zero, loop_bb->getSinglePredecessor());
        wi_phi->addIncoming(zero, loop_bb->getSinglePredecessor());

        auto* cond = builder_->CreateICmpSLT(i_phi, src_len);
        builder_->CreateCondBr(cond, body_bb, done_bb);

        builder_->SetInsertPoint(body_bb);
        auto* elem = builder_->CreateCall(rt_seq_get_, {src_ptr, i_phi}, "elem");

        auto saved = named_values_;
        named_values_[var_name] = {elem, CType::INT};
        auto guard_val = codegen(ext->condition);
        Value* guard_bool = guard_val.val;
        if (guard_bool->getType() == i64_ty)
            guard_bool = builder_->CreateICmpNE(guard_bool, zero);
        else if (guard_bool->getType() != LType::getInt1Ty(*context_))
            guard_bool = builder_->CreateICmpNE(
                builder_->CreateZExtOrTrunc(guard_bool, i64_ty), zero);
        builder_->CreateCondBr(guard_bool, guard_bb, next_bb);

        builder_->SetInsertPoint(guard_bb);
        named_values_[var_name] = {elem, CType::INT};
        auto body_val = codegen(node->reducerExpr);
        Value* store_val = body_val.val;
        if (store_val->getType()->isPointerTy())
            store_val = builder_->CreatePtrToInt(store_val, i64_ty);
        else if (store_val->getType()->isDoubleTy())
            store_val = builder_->CreateBitCast(store_val, i64_ty);
        builder_->CreateCall(rt_seq_set_, {result, wi_phi, store_val});
        auto* wi_inc = builder_->CreateAdd(wi_phi, one, "wi.inc");
        builder_->CreateBr(next_bb);

        builder_->SetInsertPoint(next_bb);
        auto* wi_merged = builder_->CreatePHI(i64_ty, 2, "wi.m");
        wi_merged->addIncoming(wi_phi, body_bb);
        wi_merged->addIncoming(wi_inc, guard_bb);
        auto* i_next = builder_->CreateAdd(i_phi, one);
        i_phi->addIncoming(i_next, next_bb);
        wi_phi->addIncoming(wi_merged, next_bb);
        builder_->CreateBr(loop_bb);

        named_values_ = saved;

        // Adjust count to actual number of matches (seq[0] = count)
        builder_->SetInsertPoint(done_bb);
        builder_->CreateStore(wi_phi, result);
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

    std::string var_name = extractor_var_name(node->collectionExtractor);
    auto* func = builder_->GetInsertBlock()->getParent();
    auto* zero = ConstantInt::get(i64_ty, 0);

    // Dict generator uses HAMT dict_put (persistent insert).
    // Indexed iteration avoids O(n²) memmove cost of head/tail on flat seqs.
    auto* dict = builder_->CreateCall(rt_dict_alloc_, {zero}, "gen_dict");
    auto* one = ConstantInt::get(i64_ty, 1);

    auto* loop_bb = BasicBlock::Create(*context_, "dictgen.loop", func);
    auto* body_bb = BasicBlock::Create(*context_, "dictgen.body", func);
    auto* done_bb = BasicBlock::Create(*context_, "dictgen.done", func);

    builder_->CreateBr(loop_bb);

    builder_->SetInsertPoint(loop_bb);
    auto* i_phi = builder_->CreatePHI(i64_ty, 2, "i");
    auto* dict_phi = builder_->CreatePHI(ptr_ty, 2, "dict");
    i_phi->addIncoming(zero, loop_bb->getSinglePredecessor());
    dict_phi->addIncoming(dict, loop_bb->getSinglePredecessor());

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

    auto* new_dict = builder_->CreateCall(rt_dict_put_, {dict_phi, key_i64, val_i64}, "dict.put");

    auto* i_next = builder_->CreateAdd(i_phi, one);
    i_phi->addIncoming(i_next, builder_->GetInsertBlock());
    dict_phi->addIncoming(new_dict, builder_->GetInsertBlock());
    builder_->CreateBr(loop_bb);

    named_values_ = saved;

    builder_->SetInsertPoint(done_bb);
    return {dict_phi, CType::DICT};
}

} // namespace yona::compiler::codegen
