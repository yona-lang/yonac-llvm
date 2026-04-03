//
// Codegen — Function code generation
//
// Free variable analysis, function definition, lambda aliases,
// function compilation, apply (call), closure wrapping, auto-await.
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

// Forward declarations for type annotation support
static CType yona_type_to_ctype(const types::Type& t);
static std::pair<std::vector<CType>, CType> uncurry_type_signature(const types::Type& t);

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
        case AST_LOGICAL_AND_EXPR: case AST_LOGICAL_OR_EXPR:
        case AST_CONS_LEFT_EXPR: case AST_CONS_RIGHT_EXPR: case AST_JOIN_EXPR: {
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
            if (auto* nc = dynamic_cast<NameCall*>(e->call)) {
                if (!bound.count(nc->name->value)) free_vars.insert(nc->name->value);
            } else if (auto* ec = dynamic_cast<ExprCall*>(e->call)) {
                if (ec->expr) collect_free_vars(ec->expr, bound, free_vars);
            }
            for (auto& a : e->args) {
                if (std::holds_alternative<ExprNode*>(a))
                    collect_free_vars(std::get<ExprNode*>(a), bound, free_vars);
                else collect_free_vars(std::get<ValueExpr*>(a), bound, free_vars);
            }
            break;
        }
        case AST_FUNCTION_EXPR: {
            // Lambda expression: \x -> body — add params to bound, recurse into body
            auto* fn = static_cast<FunctionExpr*>(node);
            auto nb = bound;
            for (auto* pat : fn->patterns) {
                if (pat->get_type() == AST_PATTERN_VALUE) {
                    auto* pv = static_cast<PatternValue*>(pat);
                    if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                        nb.insert((*id)->name->value);
                }
            }
            for (auto* body : fn->bodies) {
                if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body))
                    collect_free_vars(bwg->expr, nb, free_vars);
            }
            break;
        }
        case AST_TUPLE_EXPR: {
            auto* e = static_cast<TupleExpr*>(node);
            for (auto* v : e->values) collect_free_vars(v, bound, free_vars);
            break;
        }
        case AST_VALUES_SEQUENCE_EXPR: {
            auto* e = static_cast<ValuesSequenceExpr*>(node);
            for (auto* v : e->values) collect_free_vars(v, bound, free_vars);
            break;
        }
        case AST_DO_EXPR: {
            auto* e = static_cast<DoExpr*>(node);
            for (auto* step : e->steps)
                collect_free_vars(step, bound, free_vars);
            break;
        }
        default: break;
    }
}

// ===== Functions (deferred compilation) =====

TypedValue Codegen::codegen_function_def(FunctionExpr* node, const std::string& name) {
    set_debug_loc(node->source_context);
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
                // Extern/imported function: skip (resolved at link time)
                if (extern_functions_.count(v) > 0)
                    continue;
                // Null-valued function (e.g., not yet compiled): skip
                if (!it->second.val)
                    continue;
                // Runtime function pointer (parameter, closure): capture it
            }
            def.free_vars.push_back(v);
        }
    }

    if (!def.free_vars.empty()) {
        // Free vars exist: compile as closure with env-passing convention.
        // Function signature: (ptr env, regular_args...) -> ret
        // Captures are loaded from env[1], env[2], ... inside the function body.
        // A closure object {fn_ptr, cap0, cap1, ...} is created at the current site.
        auto ptr_ty = PointerType::get(*context_, 0);
        auto i64_ty = LType::getInt64Ty(*context_);

        // Collect capture values and types from current scope
        std::vector<TypedValue> capture_tvs;
        for (auto& fv : def.free_vars) {
            auto it = named_values_.find(fv);
            capture_tvs.push_back(it->second);
        }

        // Build param types: (ptr env, i64 args...)
        std::vector<LType*> param_types = {ptr_ty};
        for (size_t pi = 0; pi < def.param_names.size(); pi++)
            param_types.push_back(i64_ty);

        // Infer return type from body
        CType ret_ctype = CType::INT;
        LType* ret_llvm = i64_ty;
        if (!node->bodies.empty()) {
            auto* body = node->bodies[0];
            if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body)) {
                auto [lt, ct] = infer_return_type(bwg->expr);
                ret_llvm = lt;
                ret_ctype = ct;
            }
        }
        auto fn_type = llvm::FunctionType::get(ret_llvm, param_types, false);
        auto* fn = Function::Create(fn_type, Function::InternalLinkage, fn_name, module_.get());

        // Register preliminary for recursive calls (closure_env set below after entry block)
        CompiledFunction cf_pre;
        cf_pre.fn = fn;
        cf_pre.return_type = ret_ctype;
        for (size_t pi = 0; pi < def.param_names.size(); pi++)
            cf_pre.param_types.push_back(CType::INT);
        compiled_functions_[fn_name] = cf_pre;
        named_values_[fn_name] = {fn, CType::FUNCTION};

        // Save state
        auto saved_block = builder_->GetInsertBlock();
        auto saved_point = builder_->GetInsertPoint();
        auto saved_values = named_values_;
        auto saved_di_scope = di_scope_;
        auto saved_debug_loc = builder_->getCurrentDebugLocation();

        // Create debug info for closure function
        if (debug_info_ && di_builder_ && di_file_) {
            auto* di_func_ty = di_builder_->createSubroutineType(
                di_builder_->getOrCreateTypeArray({}));
            auto* di_sp = di_builder_->createFunction(
                di_file_, fn_name, fn_name, di_file_,
                node->source_context.line, di_func_ty, node->source_context.line,
                DINode::FlagZero, DISubprogram::SPFlagDefinition);
            fn->setSubprogram(di_sp);
            di_scope_ = di_sp;
        }

        auto* entry = BasicBlock::Create(*context_, "entry", fn);
        builder_->SetInsertPoint(entry);
        if (debug_info_ && di_scope_)
            builder_->SetCurrentDebugLocation(
                DILocation::get(*context_, node->source_context.line, 0, di_scope_));

        // Set up scope: keep function references from outer scope
        named_values_.clear();
        for (auto& [k, v] : saved_values) {
            if (v.type == CType::FUNCTION) named_values_[k] = v;
        }
        named_values_[fn_name] = {fn, CType::FUNCTION};

        auto arg_it = fn->arg_begin();
        Value* env = &*arg_it; env->setName("env");
        ++arg_it;

        // Update preliminary entry with env for recursive closure calls
        compiled_functions_[fn_name].closure_env = env;

        // Load captures from env
        for (size_t ci = 0; ci < def.free_vars.size(); ci++) {
            auto* cap_val = builder_->CreateCall(rt_closure_get_cap_,
                {env, ConstantInt::get(i64_ty, ci)}, def.free_vars[ci] + "_cap");
            CType cap_type = capture_tvs[ci].type;
            Value* typed_val = cap_val;
            if (cap_type == CType::FUNCTION || cap_type == CType::SEQ ||
                cap_type == CType::STRING || cap_type == CType::ADT ||
                cap_type == CType::SET || cap_type == CType::DICT)
                typed_val = builder_->CreateIntToPtr(cap_val, ptr_ty);
            else if (cap_type == CType::FLOAT)
                typed_val = builder_->CreateBitCast(cap_val, LType::getDoubleTy(*context_));
            else if (cap_type == CType::BOOL)
                typed_val = builder_->CreateTrunc(cap_val, LType::getInt1Ty(*context_));
            named_values_[def.free_vars[ci]] = {typed_val, cap_type, capture_tvs[ci].subtypes};
        }

        // Bind regular params
        for (size_t pi = 0; pi < def.param_names.size(); pi++) {
            Value* param = &*arg_it; param->setName(def.param_names[pi]);
            ++arg_it;
            named_values_[def.param_names[pi]] = {param, CType::INT};
        }

        // Compile body
        TypedValue body_tv;
        if (!node->bodies.empty()) {
            auto* body_node = node->bodies[0];
            if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body_node))
                body_tv = codegen(bwg->expr);
        }

        // Coerce body result to match the function's declared return type
        if (body_tv && body_tv.val) {
            Value* ret_val = body_tv.val;
            // Match declared return type
            if (ret_val->getType() != ret_llvm) {
                if (ret_val->getType()->isIntegerTy() && ret_llvm->isPointerTy())
                    ret_val = builder_->CreateIntToPtr(ret_val, ret_llvm);
                else if (ret_val->getType()->isPointerTy() && ret_llvm->isIntegerTy())
                    ret_val = builder_->CreatePtrToInt(ret_val, ret_llvm);
                else if (ret_val->getType()->isIntegerTy() && ret_llvm->isIntegerTy())
                    ret_val = builder_->CreateZExtOrTrunc(ret_val, ret_llvm);
            }
            if (!builder_->GetInsertBlock()->getTerminator())
                builder_->CreateRet(ret_val);
        } else {
            if (!builder_->GetInsertBlock()->getTerminator()) {
                auto* default_ret = Constant::getNullValue(ret_llvm);
                builder_->CreateRet(default_ret);
            }
        }

        // Restore state
        named_values_ = saved_values;
        di_scope_ = saved_di_scope;
        if (saved_block) builder_->SetInsertPoint(saved_block, saved_point);
        if (debug_info_) builder_->SetCurrentDebugLocation(saved_debug_loc);

        // Remove from compiled_functions_ — closure functions are called through
        // their closure pointer (indirect call path), not directly.
        // The preliminary entry was only needed during body compilation for
        // potential recursive self-calls.
        compiled_functions_.erase(fn_name);

        // Create closure object at the current insertion point
        // Store return CType tag and user-arg arity for call-site dispatch
        auto* closure = builder_->CreateCall(rt_closure_create_,
            {fn, ConstantInt::get(i64_ty, static_cast<int64_t>(ret_ctype)),
             ConstantInt::get(i64_ty, def.param_names.size()),
             ConstantInt::get(i64_ty, def.free_vars.size())}, fn_name + "_closure");

        // Store captured values — rc_inc each heap-typed capture since the
        // closure now holds an additional reference.
        // Exception: self-capture (recursive closure) uses a weak reference
        // to break the cycle. Without this, recursive closures leak forever.
        // Build heap_mask for recursive destruction: bit i is set if capture i
        // is heap-typed (pointer that needs rc_dec when the closure is freed).
        int64_t heap_mask = 0;
        for (size_t ci = 0; ci < capture_tvs.size(); ci++) {
            bool is_self = (ci < def.free_vars.size() && def.free_vars[ci] == fn_name);
            bool is_heap = is_heap_type(capture_tvs[ci].type) &&
                           !capture_tvs[ci].val->getType()->isStructTy();
            if (is_heap && !is_self) {
                emit_rc_inc(capture_tvs[ci].val, capture_tvs[ci].type);
                if (ci < 64) heap_mask |= ((int64_t)1 << ci);
            }
            Value* cap_i64 = capture_tvs[ci].val;
            if (cap_i64->getType()->isPointerTy())
                cap_i64 = builder_->CreatePtrToInt(cap_i64, i64_ty);
            else if (cap_i64->getType()->isDoubleTy())
                cap_i64 = builder_->CreateBitCast(cap_i64, i64_ty);
            else if (cap_i64->getType()->isIntegerTy() && cap_i64->getType() != i64_ty)
                cap_i64 = builder_->CreateZExtOrTrunc(cap_i64, i64_ty);
            builder_->CreateCall(rt_closure_set_cap_,
                {closure, ConstantInt::get(i64_ty, ci), cap_i64});
        }
        // Store heap_mask so rc_dec can recursively free captures
        if (heap_mask != 0)
            builder_->CreateCall(rt_closure_set_heap_mask_,
                {closure, ConstantInt::get(i64_ty, heap_mask)});

        last_lambda_name_ = fn_name;
        named_values_[fn_name] = {closure, CType::FUNCTION, {ret_ctype}};
        return {closure, CType::FUNCTION, {ret_ctype}};
    }

    deferred_functions_[fn_name] = def;
    last_lambda_name_ = fn_name;
    return {nullptr, CType::FUNCTION}; // no value yet, compiled at call site
}

TypedValue Codegen::codegen_lambda_alias(LambdaAlias* node) {
    auto fn_name = node->name->value;
    auto tv = codegen_function_def(node->lambda, fn_name);
    // For closures, tv has the closure pointer; for deferred, tv is {nullptr, FUNCTION}
    named_values_[fn_name] = tv;
    last_lambda_name_ = fn_name;
    return tv;
}

Codegen::CompiledFunction Codegen::compile_function(
    const std::string& name, const DeferredFunction& def, const std::vector<TypedValue>& args) {

    // If the function has a type annotation, use it to determine param types
    // instead of relying on caller's arg types (which may be wrong).
    std::vector<CType> annotated_ctypes;
    if (def.ast->type_signature.has_value()) {
        auto [ann_params, ann_ret] = uncurry_type_signature(*def.ast->type_signature);
        annotated_ctypes = ann_params;
    }

    // Build parameter types from type annotation or actual argument types.
    std::vector<LType*> param_types;
    std::vector<CType> param_ctypes;
    for (size_t i = 0; i < def.param_names.size(); i++) {
        CType ct = (!annotated_ctypes.empty() && i < annotated_ctypes.size())
            ? annotated_ctypes[i]
            : ((i < args.size()) ? args[i].type : CType::INT);
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

    // Pre-infer return LLVM type from body structure. Returns the actual
    // LLVM type (including struct layout for tuples) so the function is
    // created with the correct signature — no recreation needed.
    auto i64_ty = LType::getInt64Ty(*context_);
    auto tag_ty = LType::getInt64Ty(*context_);

    std::function<std::pair<LType*, CType>(AstNode*)> infer_ret = [&](AstNode* node) -> std::pair<LType*, CType> {
        if (!node) return {i64_ty, CType::INT};
        auto ct = node->get_type();
        if (ct == AST_VALUES_SEQUENCE_EXPR || ct == AST_CONS_LEFT_EXPR ||
            ct == AST_CONS_RIGHT_EXPR || ct == AST_JOIN_EXPR)
            return {llvm_type(CType::SEQ), CType::SEQ};
        if (ct == AST_SET_EXPR) return {llvm_type(CType::SET), CType::SET};
        if (ct == AST_DICT_EXPR) return {llvm_type(CType::DICT), CType::DICT};
        if (ct == AST_SYMBOL_EXPR) return {i64_ty, CType::SYMBOL};
        if (ct == AST_STRING_EXPR) return {llvm_type(CType::STRING), CType::STRING};
        if (ct == AST_INTEGER_EXPR) return {i64_ty, CType::INT};
        if (ct == AST_FLOAT_EXPR) return {llvm_type(CType::FLOAT), CType::FLOAT};
        if (ct == AST_TRUE_LITERAL_EXPR || ct == AST_FALSE_LITERAL_EXPR)
            return {llvm_type(CType::BOOL), CType::BOOL};
        if (ct == AST_FUNCTION_EXPR)
            return {PointerType::get(*context_, 0), CType::FUNCTION};
        if (ct == AST_TUPLE_EXPR) {
            // Tuples are boxed as i64 (ptrtoint'd heap pointer)
            return {i64_ty, CType::TUPLE};
        }
        if (ct == AST_FIELD_ACCESS_EXPR) {
            auto* fa = static_cast<FieldAccessExpr*>(node);
            std::string fname = fa->name->value;
            for (auto& [cname, info] : adt_constructors_) {
                for (size_t fi = 0; fi < info.field_names.size(); fi++) {
                    if (info.field_names[fi] == fname && fi < info.field_types.size())
                        return {llvm_type(info.field_types[fi]), info.field_types[fi]};
                }
            }
        }
        if (ct == AST_IDENTIFIER_EXPR) {
            auto* id = static_cast<IdentifierExpr*>(node);
            for (size_t pi = 0; pi < def.param_names.size(); pi++) {
                if (def.param_names[pi] == id->name->value && pi < args.size()) {
                    LType* lt = (args[pi].val && args[pi].val->getType() != llvm_type(args[pi].type))
                        ? args[pi].val->getType() : llvm_type(args[pi].type);
                    return {lt, args[pi].type};
                }
            }
            // Check ADT constructors (zero-arity)
            auto adt_it = adt_constructors_.find(id->name->value);
            if (adt_it != adt_constructors_.end()) {
                if (adt_it->second.is_recursive)
                    return {PointerType::get(*context_, 0), CType::ADT};
                std::vector<LType*> fields = {tag_ty};
                for (int f = 0; f < adt_it->second.max_arity; f++) fields.push_back(i64_ty);
                return {StructType::get(*context_, fields), CType::ADT};
            }
        }
        // ADT constructor application: Next n tail → returns the ADT type
        if (ct == AST_APPLY_EXPR) {
            auto* app = static_cast<ApplyExpr*>(node);
            // Walk the apply chain to find the root name
            ApplyExpr* a = app;
            std::string root_name;
            while (a) {
                if (auto* nc = dynamic_cast<NameCall*>(a->call)) {
                    root_name = nc->name->value; break;
                } else if (auto* ec = dynamic_cast<ExprCall*>(a->call)) {
                    if (auto* inner = dynamic_cast<ApplyExpr*>(ec->expr)) a = inner;
                    else break;
                } else break;
            }
            if (!root_name.empty()) {
                auto adt_it = adt_constructors_.find(root_name);
                if (adt_it != adt_constructors_.end()) {
                    if (adt_it->second.is_recursive)
                        return {PointerType::get(*context_, 0), CType::ADT};
                    std::vector<LType*> fields = {tag_ty};
                    for (int f = 0; f < adt_it->second.max_arity; f++) fields.push_back(i64_ty);
                    return {StructType::get(*context_, fields), CType::ADT};
                }
            }
        }
        // Recurse through wrappers
        if (ct == AST_CASE_EXPR) {
            auto* ce = static_cast<CaseExpr*>(node);
            if (!ce->clauses.empty()) return infer_ret(ce->clauses[0]->body);
        }
        if (ct == AST_LET_EXPR) return infer_ret(static_cast<LetExpr*>(node)->expr);
        if (ct == AST_IF_EXPR) return infer_ret(static_cast<IfExpr*>(node)->thenExpr);
        return {i64_ty, CType::INT};
    };

    CType preliminary_ret = CType::INT;
    LType* ret_type = i64_ty;
    if (!def.ast->bodies.empty()) {
        auto* body = def.ast->bodies[0];
        if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body)) {
            auto [lt, ct] = infer_ret(bwg->expr);
            ret_type = lt;
            preliminary_ret = ct;
        }
    }
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
    auto saved_di_scope = di_scope_;
    auto saved_debug_loc = builder_->getCurrentDebugLocation();
    auto saved_consumed = consumed_params_;
    consumed_params_.clear();

    // Create debug info for this function
    if (debug_info_ && di_builder_ && di_file_) {
        auto* di_func_ty = di_func_type(param_ctypes, preliminary_ret);
        auto* di_sp = di_builder_->createFunction(
            di_file_, name, name, di_file_,
            def.ast->source_context.line, di_func_ty, def.ast->source_context.line,
            DINode::FlagZero, DISubprogram::SPFlagDefinition);
        fn->setSubprogram(di_sp);
        di_scope_ = di_sp;
    }

    auto entry = BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(entry);

    // Set debug location to this function's scope (not the caller's)
    if (debug_info_ && di_scope_)
        builder_->SetCurrentDebugLocation(
            DILocation::get(*context_, def.ast->source_context.line, 0, di_scope_));

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
            // For FUNCTION args, look up return type and propagate
            // closure devirtualization info (known Function* for the param).
            if (ct == CType::FUNCTION && i < args.size() && args[i].val) {
                if (auto* fn_val = dyn_cast<Function>(args[i].val)) {
                    if (st.empty()) {
                        auto cf_it = compiled_functions_.find(fn_val->getName().str());
                        if (cf_it != compiled_functions_.end())
                            st = {cf_it->second.return_type};
                    }
                }
                // Propagate known closure → Function* to the param
                auto kf_it = closure_known_fn_.find(args[i].val);
                if (kf_it != closure_known_fn_.end())
                    closure_known_fn_[&arg] = kf_it->second;
            }
        } else {
            pname = def.free_vars[i - def.param_names.size()];
            size_t ci = i - def.param_names.size();
            ct = (ci < capture_values.size()) ? capture_values[ci].type : CType::INT;
        }
        arg.setName(pname);
        TypedValue param_tv{&arg, ct, st};
        // Propagate ADT type name from argument
        if (ct == CType::ADT && i < args.size() && !args[i].adt_type_name.empty()) {
            param_tv.adt_type_name = args[i].adt_type_name;
        }
        named_values_[pname] = param_tv;

        // Destructure complex patterns (tuples, sequences) into element variables
        if (i < def.ast->patterns.size()) {
            auto* pat = def.ast->patterns[i];
            if (ct == CType::TUPLE && pat->get_type() == AST_TUPLE_PATTERN) {
                auto* tp = static_cast<TuplePattern*>(pat);
                auto i64_local = LType::getInt64Ty(*context_);
                for (size_t ti = 0; ti < tp->patterns.size(); ti++) {
                    auto* elem_pat = tp->patterns[ti];
                    if (elem_pat->get_type() == AST_PATTERN_VALUE) {
                        auto* pv = static_cast<PatternValue*>(elem_pat);
                        if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                            CType et = (ti < st.size()) ? st[ti] : CType::INT;
                            Value* elem;
                            if (arg.getType()->isPointerTy()) {
                                auto* gep = builder_->CreateGEP(i64_local, &arg,
                                    {ConstantInt::get(i64_local, ti + 2)}, "param_tuple_gep"); // +2 for tuple header
                                elem = builder_->CreateLoad(i64_local, gep, "param_tuple_elem");
                            } else {
                                // i64 (ptrtoint'd tuple): inttoptr then GEP+load
                                auto* ptr = builder_->CreateIntToPtr(&arg, PointerType::get(*context_, 0));
                                auto* gep = builder_->CreateGEP(i64_local, ptr,
                                    {ConstantInt::get(i64_local, ti + 2)}, "param_tuple_gep"); // +2 for tuple header
                                elem = builder_->CreateLoad(i64_local, gep, "param_tuple_elem");
                            }
                            named_values_[(*id)->name->value] = {elem, et};
                        }
                    }
                }
            }
        }

        // Emit parameter debug info (only for user params, not captures)
        if (debug_info_ && di_scope_ && di_builder_ && i < def.param_names.size()) {
            auto* alloca = builder_->CreateAlloca(arg.getType(), nullptr, pname + ".dbg");
            builder_->CreateStore(&arg, alloca);
            auto* di_param = di_builder_->createParameterVariable(
                di_scope_, pname, i + 1, di_file_,
                def.ast->source_context.line, di_type_for(ct));
            di_builder_->insertDeclare(alloca, di_param, di_builder_->createExpression(),
                DILocation::get(*context_, def.ast->source_context.line, 0, di_scope_),
                builder_->GetInsertBlock());
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
        if (body_tv.val->getType() != ret_type) {
            // Remove the function and recreate with correct return type
            fn->eraseFromParent();
            auto new_fn_type = llvm::FunctionType::get(body_tv.val->getType(), param_types, false);
            fn = Function::Create(new_fn_type, Function::InternalLinkage, name, module_.get());

            // Re-attach debug info to the recreated function
            if (debug_info_ && di_builder_ && di_file_) {
                auto* di_func_ty = di_func_type(param_ctypes, preliminary_ret);
                auto* di_sp = di_builder_->createFunction(
                    di_file_, name, name, di_file_,
                    def.ast->source_context.line, di_func_ty, def.ast->source_context.line,
                    DINode::FlagZero, DISubprogram::SPFlagDefinition);
                fn->setSubprogram(di_sp);
                di_scope_ = di_sp;
            }

            // Recompile
            named_values_.clear();
            for (auto& [k, v] : saved_values) {
                if (v.type == CType::FUNCTION) named_values_[k] = v;
            }
            named_values_[name] = {fn, CType::FUNCTION};

            auto new_entry = BasicBlock::Create(*context_, "entry", fn);
            builder_->SetInsertPoint(new_entry);
            if (debug_info_ && di_scope_)
                builder_->SetCurrentDebugLocation(
                    DILocation::get(*context_, def.ast->source_context.line, 0, di_scope_));

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
                        auto i64_local = LType::getInt64Ty(*context_);
                        for (size_t ti = 0; ti < tp->patterns.size(); ti++) {
                            auto* elem_pat = tp->patterns[ti];
                            if (elem_pat->get_type() == AST_PATTERN_VALUE) {
                                auto* pv = static_cast<PatternValue*>(elem_pat);
                                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                                    CType et = (ti < st.size()) ? st[ti] : CType::INT;
                                    Value* elem;
                                    if (arg.getType()->isPointerTy()) {
                                        auto* gep = builder_->CreateGEP(i64_local, &arg,
                                            {ConstantInt::get(i64_local, ti + 2)}, "param_tuple_gep"); // +2 for tuple header
                                        elem = builder_->CreateLoad(i64_local, gep, "param_tuple_elem");
                                    } else {
                                        auto* ptr = builder_->CreateIntToPtr(&arg, PointerType::get(*context_, 0));
                                        auto* gep = builder_->CreateGEP(i64_local, ptr,
                                            {ConstantInt::get(i64_local, ti + 2)}, "param_tuple_gep"); // +2 for tuple header
                                        elem = builder_->CreateLoad(i64_local, gep, "param_tuple_elem");
                                    }
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
        if (!builder_->GetInsertBlock()->getTerminator()) {
            // Perceus callee-owns DROP for non-seq heap params.
            if (body_tv.val) {
                auto ptr_ty = PointerType::get(*context_, 0);
                for (size_t pi = 0; pi < def.param_names.size(); pi++) {
                    if (pi >= param_ctypes.size()) continue;
                    CType ct = param_ctypes[pi];
                    if (ct == CType::SEQ) continue;
                    if (!is_heap_type(ct)) continue;
                    if (pi >= fn->arg_size()) continue;
                    auto* param = fn->getArg(pi);
                    if (param->getType()->isStructTy()) continue;
                    if (consumed_params_.count(param)) continue;

                    Value* param_ptr = param;
                    Value* ret_ptr = body_tv.val;
                    if (param_ptr->getType()->isIntegerTy())
                        param_ptr = builder_->CreateIntToPtr(param_ptr, ptr_ty);
                    if (ret_ptr->getType()->isIntegerTy())
                        ret_ptr = builder_->CreateIntToPtr(ret_ptr, ptr_ty);
                    if (param_ptr->getType()->isPointerTy() && ret_ptr->getType()->isPointerTy()) {
                        auto* is_same = builder_->CreateICmpEQ(param_ptr, ret_ptr, "param_is_ret");
                        auto* drop_bb = BasicBlock::Create(*context_, "drop_param", fn);
                        auto* cont_bb = BasicBlock::Create(*context_, "cont", fn);
                        builder_->CreateCondBr(is_same, cont_bb, drop_bb);
                        builder_->SetInsertPoint(drop_bb);
                        emit_rc_dec(param, ct);
                        builder_->CreateBr(cont_bb);
                        builder_->SetInsertPoint(cont_bb);
                    } else {
                        emit_rc_dec(param, ct);
                    }
                }
            }
            builder_->CreateRet(body_tv.val);
        }
    } else {
        if (!builder_->GetInsertBlock()->getTerminator())
            builder_->CreateRet(Constant::getNullValue(ret_type));
    }

    // Restore
    named_values_ = saved_values;
    consumed_params_ = saved_consumed;
    di_scope_ = saved_di_scope;
    if (saved_block) builder_->SetInsertPoint(saved_block, saved_point);
    if (debug_info_) builder_->SetCurrentDebugLocation(saved_debug_loc);

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
    set_debug_loc(node->source_context);
    // Flatten juxtaposition chain: f x y → collect all args and root name
    std::vector<ApplyExpr*> chain;
    std::string fn_name;
    ApplyExpr* cur = node;
    std::string module_fqn; // set if this is a FQN call (Mod::func)
    while (cur) {
        chain.push_back(cur);
        if (auto* nc = dynamic_cast<NameCall*>(cur->call)) {
            fn_name = nc->name->value;
            break;
        } else if (auto* mc = dynamic_cast<ModuleCall*>(cur->call)) {
            // FQN call: Std\List::map — auto-load module interface
            fn_name = mc->funName->value;
            if (auto* fqn = std::get_if<FqnExpr*>(&mc->fqn)) {
                auto [fqn_str, fqn_path] = build_fqn_path(*fqn);
                module_fqn = fqn_str;
                load_module_interface(fqn_path);
                register_import(fqn_str, fn_name, fn_name);
            }
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

    // Wrap Function* values in closures for uniform calling convention.
    // This ensures HOF parameters always receive closures they can call with env-passing.
    for (size_t ai = 0; ai < all_args.size(); ai++) {
        if (all_args[ai].type == CType::FUNCTION && all_args[ai].val && isa<Function>(all_args[ai].val)) {
            // Ensure subtypes have return type info
            if (all_args[ai].subtypes.empty()) {
                auto cf_it = compiled_functions_.find(cast<Function>(all_args[ai].val)->getName().str());
                if (cf_it != compiled_functions_.end())
                    all_args[ai].subtypes = {cf_it->second.return_type};
            }
            CType fn_ret = (!all_args[ai].subtypes.empty()) ? all_args[ai].subtypes[0] : CType::INT;
            auto* underlying_fn = cast<Function>(all_args[ai].val);
            all_args[ai].val = wrap_in_closure(underlying_fn, fn_ret);
            // Remember the closure → wrapper function mapping for devirtualization.
            // The wrapper has the env-passing signature: (ptr env, user_args...).
            auto wrapper_name = underlying_fn->getName().str() + "_env";
            if (auto* wrapper_fn = module_->getFunction(wrapper_name))
                closure_known_fn_[all_args[ai].val] = wrapper_fn;
        }
    }

    // Check if it's an ADT constructor call
    auto adt_it = adt_constructors_.find(fn_name);
    if (adt_it != adt_constructors_.end() && adt_it->second.arity > 0) {
        auto& info = adt_it->second;
        auto tag_ty = LType::getInt64Ty(*context_);
        auto i64_ty = LType::getInt64Ty(*context_);

        // Helper: cast value to i64 for storage
        auto to_i64 = [&](Value* arg_val) -> Value* {
            if (arg_val->getType() == i64_ty) return arg_val;
            if (arg_val->getType()->isIntegerTy())
                return builder_->CreateZExtOrTrunc(arg_val, i64_ty);
            if (arg_val->getType()->isPointerTy())
                return builder_->CreatePtrToInt(arg_val, i64_ty);
            if (arg_val->getType()->isDoubleTy())
                return builder_->CreateBitCast(arg_val, i64_ty);
            if (arg_val->getType()->isStructTy()) {
                auto* alloca = builder_->CreateAlloca(arg_val->getType());
                builder_->CreateStore(arg_val, alloca);
                return builder_->CreateLoad(i64_ty, alloca);
            }
            return arg_val;
        };

        if (info.is_recursive) {
            // Recursive ADT: heap-allocate via runtime
            auto* node_ptr = builder_->CreateCall(rt_adt_alloc_,
                {ConstantInt::get(tag_ty, info.tag),
                 ConstantInt::get(i64_ty, info.arity)}, "adt_node");
            int64_t adt_heap_mask = 0;
            for (size_t ai = 0; ai < all_args.size() && ai < (size_t)info.arity; ai++) {
                Value* arg_val = to_i64(all_args[ai].val);
                builder_->CreateCall(rt_adt_set_field_,
                    {node_ptr, ConstantInt::get(i64_ty, ai), arg_val});
                if (is_heap_type(all_args[ai].type) && ai < 64)
                    adt_heap_mask |= ((int64_t)1 << ai);
            }
            if (adt_heap_mask != 0)
                builder_->CreateCall(rt_adt_set_heap_mask_,
                    {node_ptr, ConstantInt::get(i64_ty, adt_heap_mask)});
            TypedValue result{node_ptr, CType::ADT};
            result.adt_type_name = info.type_name;
            return result;
        } else {
            // Non-recursive: flat struct {i8, i64*max_arity}
            std::vector<LType*> fields = {tag_ty};
            for (int f = 0; f < info.max_arity; f++) fields.push_back(i64_ty);
            auto* struct_type = StructType::get(*context_, fields);
            Value* val = UndefValue::get(struct_type);
            val = builder_->CreateInsertValue(val, ConstantInt::get(tag_ty, info.tag), {0});
            for (size_t ai = 0; ai < all_args.size() && ai < (size_t)info.arity; ai++) {
                Value* arg_val = to_i64(all_args[ai].val);
                val = builder_->CreateInsertValue(val, arg_val, {(unsigned)(ai + 1)});
            }
            TypedValue result{val, CType::ADT};
            result.adt_type_name = info.type_name;
            return result;
        }
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

    // Trait method resolution: if not found yet, check if fn_name is a trait method
    if (cf_it == compiled_functions_.end()) {
        CType first_arg_type = all_args.empty() ? CType::INT : all_args[0].type;
        std::string first_adt_name = all_args.empty() ? "" : all_args[0].adt_type_name;
        auto resolved = resolve_trait_method(fn_name, first_arg_type, first_adt_name);
        if (!resolved.empty()) {
            // Try compiled functions first
            cf_it = compiled_functions_.find(resolved);
            if (cf_it == compiled_functions_.end()) {
                // Try deferred
                auto def_it = deferred_functions_.find(resolved);
                if (def_it != deferred_functions_.end()) {
                    compile_function(resolved, def_it->second, all_args);
                    cf_it = compiled_functions_.find(resolved);
                }
            }
        }
    }

    if (cf_it == compiled_functions_.end()) {
        // Check if fn_name is a FUNCTION-typed variable (higher-order: indirect call)
        auto var_it = named_values_.find(fn_name);
        if (var_it != named_values_.end() && var_it->second.type == CType::FUNCTION && var_it->second.val) {
            // Filter out Unit arguments (thunk calls: t () → call with 0 args)
            std::vector<LType*> arg_types;
            std::vector<Value*> vals;
            for (auto& a : all_args) {
                if (a.type == CType::UNIT) continue; // skip unit args for thunk calls
                arg_types.push_back(a.val->getType());
                vals.push_back(a.val);
            }
            CType ret_ctype = (!var_it->second.subtypes.empty()) ? var_it->second.subtypes[0]
                : (all_args.empty() ? CType::INT : all_args[0].type);
            auto ret_llvm = llvm_type(ret_ctype);
            auto var_val = var_it->second.val;

            if (isa<Function>(var_val)) {
                // Direct function pointer (e.g., let g = f): call as fn(args...)
                auto fn_type = llvm::FunctionType::get(ret_llvm, arg_types, false);
                auto result = builder_->CreateCall(fn_type, var_val, vals, "indirect_call");
                return {result, ret_ctype};
            } else {
                // Closure call. Check if we know the underlying Function*
                // (from closure devirtualization) for a direct call.
                auto kf_it = closure_known_fn_.find(var_val);
                if (kf_it != closure_known_fn_.end()) {
                    // Devirtualized: direct call to the known wrapper function.
                    // Coerce args to match wrapper's expected param types.
                    auto* known_fn = kf_it->second;
                    std::vector<Value*> call_vals = {var_val}; // env = closure
                    size_t param_idx = 1; // skip env param
                    for (auto& a : all_args) {
                        if (a.type == CType::UNIT) continue;
                        Value* arg_val = a.val;
                        if (param_idx < known_fn->arg_size()) {
                            auto* expected = known_fn->getArg(param_idx)->getType();
                            if (arg_val->getType() != expected) {
                                if (arg_val->getType()->isPointerTy() && expected->isIntegerTy())
                                    arg_val = builder_->CreatePtrToInt(arg_val, expected);
                                else if (arg_val->getType()->isIntegerTy() && expected->isPointerTy())
                                    arg_val = builder_->CreateIntToPtr(arg_val, expected);
                            }
                        }
                        call_vals.push_back(arg_val);
                        param_idx++;
                    }
                    auto* result = builder_->CreateCall(known_fn, call_vals, "devirt_call");
                    return {result, ret_ctype};
                }

                // Generic closure: load fn_ptr from closure[0]
                auto i64_ty = LType::getInt64Ty(*context_);
                auto ptr_ty = PointerType::get(*context_, 0);

                // Load arity from closure[2] to handle over-application (currying)
                auto* arity_gep = builder_->CreateGEP(i64_ty, var_val,
                    {ConstantInt::get(i64_ty, 2)}, "arity_ptr");
                auto* arity_val = builder_->CreateLoad(i64_ty, arity_gep, "arity");

                // Apply args, handling over-application by iterating
                Value* current_closure = var_val;
                size_t args_consumed = 0;

                while (args_consumed < vals.size()) {
                    auto* fn_i64 = builder_->CreateLoad(i64_ty, current_closure, "closure_fn_i64");
                    auto* fn_ptr = builder_->CreateIntToPtr(fn_i64, ptr_ty, "closure_fn_ptr");

                    // Load arity for current closure
                    auto* cur_arity_gep = builder_->CreateGEP(i64_ty, current_closure,
                        {ConstantInt::get(i64_ty, 2)}, "cur_arity_ptr");
                    auto* cur_arity = builder_->CreateLoad(i64_ty, cur_arity_gep, "cur_arity");

                    // For compile-time constant arity (common case), use it directly.
                    // Otherwise, call with all remaining args (non-curried).
                    int64_t static_arity = -1;
                    if (auto* ci = dyn_cast<ConstantInt>(cur_arity))
                        static_arity = ci->getZExtValue();

                    size_t n_args_this_call;
                    if (static_arity >= 0) {
                        n_args_this_call = std::min(static_cast<size_t>(static_arity),
                                                     vals.size() - args_consumed);
                    } else {
                        n_args_this_call = vals.size() - args_consumed;
                    }

                    // Build closure call: fn(env, args...)
                    std::vector<LType*> call_arg_types = {ptr_ty};
                    std::vector<Value*> call_vals = {current_closure};
                    for (size_t ai = 0; ai < n_args_this_call; ai++) {
                        call_arg_types.push_back(vals[args_consumed + ai]->getType());
                        call_vals.push_back(vals[args_consumed + ai]);
                    }

                    // Use the actual return type for the closure call
                    auto* closure_ret_ty = (args_consumed + n_args_this_call < vals.size())
                        ? (LType*)ptr_ty  // intermediate result is a closure (pointer)
                        : llvm_type(ret_ctype);  // final result uses inferred type
                    auto* call_type = llvm::FunctionType::get(closure_ret_ty, call_arg_types, false);
                    auto* result = builder_->CreateCall(call_type, fn_ptr, call_vals, "closure_call");

                    args_consumed += n_args_this_call;

                    if (args_consumed < vals.size()) {
                        current_closure = result; // already ptr type
                    } else {
                        return {result, ret_ctype};
                    }
                }

                // No args case (thunk: t ())
                auto* fn_i64 = builder_->CreateLoad(i64_ty, current_closure, "closure_fn_i64");
                auto* fn_ptr_val = builder_->CreateIntToPtr(fn_i64, ptr_ty, "closure_fn_ptr");
                auto* thunk_ret_ty = llvm_type(ret_ctype);
                auto* call_type = llvm::FunctionType::get(thunk_ret_ty, {ptr_ty}, false);
                auto* result = builder_->CreateCall(call_type, fn_ptr_val, {current_closure}, "closure_call");
                return {result, ret_ctype};
            }
        }

        // Check if it's an imported extern function
        auto ext_it = extern_functions_.find(fn_name);
        if (ext_it != extern_functions_.end()) {
            std::string mangled = ext_it->second;

            // On-demand GENFN re-parse: if the function has source available
            // and call-site arg types differ from the pre-compiled signature,
            // re-parse and compile locally (cross-module monomorphization).
            auto genfn_it = imported_function_sources_.find(mangled);
            bool types_differ = false;
            if (genfn_it != imported_function_sources_.end()) {
                auto meta_it2 = module_meta_.find(mangled);
                if (meta_it2 != module_meta_.end()) {
                    auto& meta = meta_it2->second;
                    for (size_t i = 0; i < all_args.size() && i < meta.param_types.size(); i++) {
                        if (all_args[i].type != meta.param_types[i]) {
                            types_differ = true;
                            break;
                        }
                    }
                }
            }
            if (genfn_it != imported_function_sources_.end() && types_differ) {
                auto& ifs = genfn_it->second;
                auto reparsed = reparse_genfn(ifs.local_name, ifs.source_text);
                if (reparsed && !reparsed->functions.empty()) {
                    auto* func_ast = reparsed->functions[0];
                    reparsed->functions.clear();
                    imported_ast_nodes_.push_back(std::unique_ptr<FunctionExpr>(func_ast));
                    codegen_function_def(func_ast, fn_name);
                    auto def_it2 = deferred_functions_.find(fn_name);
                    if (def_it2 != deferred_functions_.end()) {
                        compile_function(fn_name, def_it2->second, all_args);
                        auto cf_it2 = compiled_functions_.find(fn_name);
                        if (cf_it2 != compiled_functions_.end()) {
                            // Remove from extern so future calls use local copy
                            extern_functions_.erase(fn_name);
                            imported_function_sources_.erase(mangled);
                            auto& cf2 = cf_it2->second;
                            std::vector<Value*> vals;
                            for (auto& a : all_args) vals.push_back(a.val);
                            return {builder_->CreateCall(cf2.fn, vals, "genfn_call"), cf2.return_type};
                        }
                    }
                }
                // Fallthrough: if re-parse failed, call as extern
            }

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
            auto* ext_result = builder_->CreateCall(ext_fn, vals, "extern_call");
            return {ext_result, ret_ctype};
        }

        // Try as an LLVM function in the module
        auto* fn = module_->getFunction(fn_name);
        if (!fn) {
            std::string msg = "undefined function '" + fn_name + "'";
            auto suggestion = suggest_similar(fn_name);
            if (!suggestion.empty()) msg += "; did you mean '" + suggestion + "'?";
            report_error(node->source_context, msg);
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

    // Over-application (currying): more args than arity.
    // The function returns a function (closure), which is then applied to remaining args.
    // Example: let adder n = \x -> x + n in adder 5 10
    //   → call adder(5) → returns closure → call closure(10) → result
    if (all_args.size() > func_arity) {
        auto i64_ty = LType::getInt64Ty(*context_);
        auto ptr_ty = PointerType::get(*context_, 0);

        // Call with first func_arity args
        std::vector<Value*> first_args;
        for (size_t ai = 0; ai < func_arity; ai++) {
            Value* arg_val = all_args[ai].val;
            if (ai < cf.fn->arg_size()) {
                auto* expected_ty = cf.fn->getArg(ai)->getType();
                if (arg_val->getType() != expected_ty) {
                    if (arg_val->getType()->isIntegerTy() && expected_ty->isPointerTy())
                        arg_val = builder_->CreateIntToPtr(arg_val, expected_ty);
                    else if (arg_val->getType()->isPointerTy() && expected_ty->isIntegerTy())
                        arg_val = builder_->CreatePtrToInt(arg_val, expected_ty);
                }
            }
            first_args.push_back(arg_val);
        }
        for (auto& cap_name : cf.capture_names) {
            auto it = named_values_.find(cap_name);
            if (it != named_values_.end()) first_args.push_back(it->second.val);
        }

        Value* current_fn = builder_->CreateCall(cf.fn, first_args, "curry_call");

        // Apply remaining args one at a time via closure convention
        for (size_t ai = func_arity; ai < all_args.size(); ai++) {
            // current_fn is a closure ptr (or i64 encoding of one)
            if (current_fn->getType()->isIntegerTy())
                current_fn = builder_->CreateIntToPtr(current_fn, ptr_ty);

            auto* fn_i64 = builder_->CreateLoad(i64_ty, current_fn, "curry_fn_i64");
            auto* fn_ptr = builder_->CreateIntToPtr(fn_i64, ptr_ty, "curry_fn_ptr");

            std::vector<LType*> arg_types = {ptr_ty};
            std::vector<Value*> call_vals = {current_fn};

            if (all_args[ai].type != CType::UNIT) {
                arg_types.push_back(all_args[ai].val->getType());
                call_vals.push_back(all_args[ai].val);
            }

            auto* call_type = llvm::FunctionType::get(i64_ty, arg_types, false);
            current_fn = builder_->CreateCall(call_type, fn_ptr, call_vals, "curry_apply");
        }

        // Final result is i64
        return {current_fn, CType::INT};
    }

    // Perceus DUP for non-seq heap args. Seqs use callee-borrows because
    // DUP-induced rc>1 is incompatible with seq unique-owner optimization.
    for (size_t ai = 0; ai < all_args.size(); ai++) {
        CType ct = all_args[ai].type;
        if (ct == CType::SEQ) continue;
        if (!is_heap_type(ct)) continue;
        if (!all_args[ai].val || isa<Constant>(all_args[ai].val)) continue;
        if (all_args[ai].val->getType()->isStructTy()) continue;
        bool is_named = false;
        for (auto& [k, v] : named_values_)
            if (v.val == all_args[ai].val) { is_named = true; break; }
        if (is_named)
            emit_rc_inc(all_args[ai].val, ct);
    }

    std::vector<Value*> call_args;

    // For recursive closure calls, prepend the env pointer as first argument
    if (cf.closure_env)
        call_args.push_back(cf.closure_env);

    size_t fn_arg_offset = cf.closure_env ? 1 : 0;
    for (size_t ai = 0; ai < all_args.size(); ai++) {
        Value* arg_val = all_args[ai].val;
        // Coerce arg type if it doesn't match the function's expected param type.
        // This handles closures returning i64 when the callee expects ptr (ADT).
        if (ai + fn_arg_offset < cf.fn->arg_size()) {
            auto* expected_ty = cf.fn->getArg(ai + fn_arg_offset)->getType();
            if (arg_val->getType() != expected_ty) {
                if (arg_val->getType()->isIntegerTy() && expected_ty->isPointerTy())
                    arg_val = builder_->CreateIntToPtr(arg_val, expected_ty);
                else if (arg_val->getType()->isPointerTy() && expected_ty->isIntegerTy())
                    arg_val = builder_->CreatePtrToInt(arg_val, expected_ty);
            }
        }
        call_args.push_back(arg_val);
    }

    // Append capture values
    for (auto& cap_name : cf.capture_names) {
        auto it = named_values_.find(cap_name);
        if (it != named_values_.end()) call_args.push_back(it->second.val);
    }

    // io-async (AFN): call directly — function submits to io_uring and returns ID
    if (cf.return_type == CType::PROMISE && cf.is_io_async) {
        CType inner_ret = CType::INT;
        auto nv_it = named_values_.find(fn_name);
        if (nv_it != named_values_.end() && !nv_it->second.subtypes.empty())
            inner_ret = nv_it->second.subtypes[0];
        // Call the function directly — it returns the uring ID as i64
        auto* result = builder_->CreateCall(cf.fn, call_args, "io_submit");
        TypedValue tv{result, CType::PROMISE, {inner_ret}};
        tv.is_io_promise = true;
        return tv;
    }

    // Thread-pool async (extern async): wrap in thread pool call → returns PROMISE
    if (cf.return_type == CType::PROMISE) {
        CType inner_ret = CType::INT;
        auto nv_it = named_values_.find(fn_name);
        if (nv_it != named_values_.end() && !nv_it->second.subtypes.empty())
            inner_ret = nv_it->second.subtypes[0];

        auto i64_ty = LType::getInt64Ty(*context_);
        auto ptr_ty = PointerType::get(*context_, 0);

        // Helper: coerce a value to i64 for the async runtime interface
        auto to_i64 = [&](Value* v) -> Value* {
            if (v->getType() == i64_ty) return v;
            if (v->getType()->isPointerTy()) return builder_->CreatePtrToInt(v, i64_ty);
            if (v->getType()->isIntegerTy()) return builder_->CreateZExtOrTrunc(v, i64_ty);
            if (v->getType()->isDoubleTy()) return builder_->CreateBitCast(v, i64_ty);
            return v;
        };

        Value* promise;
        if (call_args.size() <= 1) {
            // 0 or 1 arg: use yona_rt_async_call(fn, arg)
            Value* arg = call_args.empty()
                ? ConstantInt::get(i64_ty, 0)
                : to_i64(call_args[0]);
            promise = builder_->CreateCall(rt_async_call_, {cf.fn, arg}, "async_call");
        } else {
            // Multi-arg: generate a thunk that captures all args via globals
            auto thunk_type = llvm::FunctionType::get(i64_ty, {}, false);
            auto thunk_name = fn_name + "_async_thunk_" + std::to_string(lambda_counter_++);
            auto thunk_fn = Function::Create(thunk_type, Function::InternalLinkage, thunk_name, module_.get());

            // Create globals for each dynamic arg before building the thunk body
            std::vector<GlobalVariable*> arg_globals;
            for (size_t ai = 0; ai < call_args.size(); ai++) {
                auto* gv = new GlobalVariable(*module_, call_args[ai]->getType(), false,
                    GlobalValue::InternalLinkage, Constant::getNullValue(call_args[ai]->getType()),
                    thunk_name + ".arg" + std::to_string(ai));
                arg_globals.push_back(gv);
            }

            // Save current state
            auto saved_block = builder_->GetInsertBlock();
            auto saved_point = builder_->GetInsertPoint();

            // Build thunk body
            auto thunk_entry = BasicBlock::Create(*context_, "entry", thunk_fn);
            builder_->SetInsertPoint(thunk_entry);

            std::vector<Value*> thunk_call_args;
            for (size_t ai = 0; ai < call_args.size(); ai++) {
                thunk_call_args.push_back(builder_->CreateLoad(call_args[ai]->getType(), arg_globals[ai]));
            }

            auto* thunk_result = builder_->CreateCall(cf.fn, thunk_call_args, "thunk_call");
            // Coerce return to i64 for the promise
            builder_->CreateRet(to_i64(thunk_result));

            // Restore insertion point
            builder_->SetInsertPoint(saved_block, saved_point);

            // Store args to globals at call site (before submitting to thread pool)
            for (size_t ai = 0; ai < call_args.size(); ai++) {
                builder_->CreateStore(call_args[ai], arg_globals[ai]);
            }

            promise = builder_->CreateCall(rt_async_call_thunk_, {thunk_fn}, "async_thunk_call");
        }
        return {promise, CType::PROMISE, {inner_ret}};
    }

    auto* call_inst = builder_->CreateCall(cf.fn, call_args, "calltmp");
    // Mark self-recursive calls as tail calls for TailCallElimination pass
    auto* current_fn = builder_->GetInsertBlock()->getParent();
    if (cf.fn == current_fn)
        call_inst->setTailCall(true);

    // Drop seq temps whose type differs from return (callee-borrows for seqs).
    for (size_t ai = 0; ai < all_args.size(); ai++) {
        if (all_args[ai].type != CType::SEQ) continue;
        if (!all_args[ai].val || isa<Constant>(all_args[ai].val)) continue;
        if (all_args[ai].val->getType()->isStructTy()) continue;
        bool is_named = false;
        for (auto& [k, v] : named_values_)
            if (v.val == all_args[ai].val) { is_named = true; break; }
        if (is_named) continue;
        if (all_args[ai].type == cf.return_type) continue;
        emit_rc_dec(all_args[ai].val, all_args[ai].type);
    }

    return {call_inst, cf.return_type};
}

// ===== Closure wrapping =====

Value* Codegen::wrap_in_closure(Function* fn, CType ret_type) {
    auto ptr_ty = PointerType::get(*context_, 0);
    auto i64_ty = LType::getInt64Ty(*context_);

    // Check if wrapper already exists
    std::string wrapper_name = fn->getName().str() + "_env";
    auto* existing = module_->getFunction(wrapper_name);
    if (!existing) {
        // Build wrapper: fn_env(ptr env, original_args...) -> i64
        std::vector<LType*> wrapper_params = {ptr_ty};
        for (auto& arg : fn->args())
            wrapper_params.push_back(arg.getType());

        auto* wrapper_type = llvm::FunctionType::get(i64_ty, wrapper_params, false);
        existing = Function::Create(wrapper_type, Function::InternalLinkage,
                                    wrapper_name, module_.get());

        auto saved_block = builder_->GetInsertBlock();
        auto saved_point = builder_->GetInsertPoint();

        auto* entry = BasicBlock::Create(*context_, "entry", existing);
        builder_->SetInsertPoint(entry);

        // Forward args (skip env)
        auto arg_it = existing->arg_begin();
        arg_it->setName("env");
        ++arg_it;
        std::vector<Value*> forward_args;
        for (; arg_it != existing->arg_end(); ++arg_it)
            forward_args.push_back(&*arg_it);

        auto* result = builder_->CreateCall(fn, forward_args, "fwd");

        // Coerce return to i64 (universal closure return)
        Value* ret_val = result;
        if (ret_val->getType() != i64_ty) {
            if (ret_val->getType()->isPointerTy())
                ret_val = builder_->CreatePtrToInt(ret_val, i64_ty);
            else if (ret_val->getType()->isDoubleTy())
                ret_val = builder_->CreateBitCast(ret_val, i64_ty);
            else if (ret_val->getType()->isIntegerTy())
                ret_val = builder_->CreateZExtOrTrunc(ret_val, i64_ty);
        }
        builder_->CreateRet(ret_val);

        if (saved_block) builder_->SetInsertPoint(saved_block, saved_point);
    }

    // Create trivial closure {wrapper_fn_ptr, ret_tag, arity, <no captures>}
    int64_t arity = fn->arg_size(); // user args (wrapper has env + original params)
    return builder_->CreateCall(rt_closure_create_,
        {existing, ConstantInt::get(i64_ty, static_cast<int64_t>(ret_type)),
         ConstantInt::get(i64_ty, arity),
         ConstantInt::get(i64_ty, 0)}, wrapper_name + "_closure");
}

TypedValue Codegen::auto_await(TypedValue tv) {
    if (!tv || tv.type != CType::PROMISE) return tv;

    // Dispatch: io_uring promise vs thread-pool promise
    Value* awaited;
    if (tv.is_io_promise) {
        awaited = builder_->CreateCall(rt_io_await_, {tv.val}, "io_await");
    } else {
        awaited = builder_->CreateCall(rt_async_await_, {tv.val}, "await");
    }

    // The awaited value's type is stored in subtypes[0]
    CType inner = (!tv.subtypes.empty()) ? tv.subtypes[0] : CType::INT;
    // The await returns i64 — coerce to the actual type
    Value* result = awaited;
    auto* expected = llvm_type(inner);
    if (expected->isPointerTy())
        result = builder_->CreateIntToPtr(awaited, expected);
    else if (expected == LType::getInt1Ty(*context_))
        result = builder_->CreateTrunc(awaited, expected);
    else if (expected->isDoubleTy())
        result = builder_->CreateBitCast(awaited, expected);
    return {result, inner};
}

// ===== Local static helpers for type annotations =====

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
            case types::Symbol: return CType::SYMBOL;
            case types::Unit: return CType::UNIT;
            case types::Seq: return CType::SEQ;
            case types::Set: return CType::SET;
            case types::Dict: return CType::DICT;
            default: return CType::INT;
        }
    }
    if (std::holds_alternative<std::shared_ptr<types::FunctionType>>(t))
        return CType::FUNCTION;
    if (std::holds_alternative<std::shared_ptr<types::SingleItemCollectionType>>(t)) {
        auto& col = std::get<std::shared_ptr<types::SingleItemCollectionType>>(t);
        return (col->kind == types::SingleItemCollectionType::Seq) ? CType::SEQ : CType::SET;
    }
    if (std::holds_alternative<std::shared_ptr<types::DictCollectionType>>(t))
        return CType::DICT;
    if (std::holds_alternative<std::shared_ptr<types::ProductType>>(t))
        return CType::TUPLE;
    if (std::holds_alternative<std::shared_ptr<types::NamedType>>(t))
        return CType::ADT;
    if (std::holds_alternative<std::shared_ptr<types::PromiseType>>(t))
        return CType::PROMISE;
    return CType::INT;
}

static std::pair<std::vector<CType>, CType> uncurry_type_signature(const types::Type& t) {
    std::vector<CType> params;
    const types::Type* current = &t;
    while (std::holds_alternative<std::shared_ptr<types::FunctionType>>(*current)) {
        auto& ft = std::get<std::shared_ptr<types::FunctionType>>(*current);
        params.push_back(yona_type_to_ctype(ft->argumentType));
        current = &ft->returnType;
    }
    return {params, yona_type_to_ctype(*current)};
}

} // namespace yona::compiler::codegen
