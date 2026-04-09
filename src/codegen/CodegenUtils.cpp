//
// Codegen — Utility functions
//
// Type inference helpers: yona_type_to_ctype, uncurry_type_signature,
// infer_param_types, infer_type_from_pattern.
//

#include "Codegen.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <iostream>

namespace yona::compiler::codegen {
using namespace llvm;
using LType = llvm::Type;

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

        case AST_RECORD_PATTERN:
            return CType::ADT; // named field pattern

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
            } else if (auto* fn_expr = dynamic_cast<FunctionExpr*>(node)) {
                for (auto* body : fn_expr->bodies)
                    if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body))
                        analyze(bwg->expr);
            } else if (auto* fa = dynamic_cast<FieldAccessExpr*>(node)) {
                // Field access: if the object is a parameter, it must be an ADT
                if (auto* id = dynamic_cast<IdentifierExpr*>(fa->identifier)) {
                    auto it = param_index.find(id->name->value);
                    if (it != param_index.end() && result[it->second].type == CType::INT) {
                        result[it->second].type = CType::ADT;
                        result[it->second].source_pattern = nullptr;
                    }
                }
            }
        };
        if (body_expr) analyze(body_expr);
    }

    return result;
}

// ===== Reference Counting Helpers =====

bool Codegen::is_heap_type(CType ct) {
    return ct == CType::SEQ || ct == CType::SET || ct == CType::DICT ||
           ct == CType::ADT || ct == CType::FUNCTION || ct == CType::STRING ||
           ct == CType::BYTE_ARRAY || ct == CType::TUPLE || ct == CType::SUM ||
           ct == CType::RECORD || ct == CType::INT_ARRAY || ct == CType::FLOAT_ARRAY;
}

void Codegen::emit_rc_inc(Value* val, CType type) {
    if (!val || !is_heap_type(type)) return;
    if (isa<Constant>(val)) return;
    // Non-recursive ADTs are struct-typed (no RC header) — skip
    if (val->getType()->isStructTy()) return;
    Value* ptr_val = val;
    if (val->getType()->isIntegerTy())
        ptr_val = builder_->CreateIntToPtr(val, PointerType::get(*context_, 0));
    builder_->CreateCall(rt_.rc_inc_, {ptr_val});
}

void Codegen::emit_rc_dec(Value* val, CType type) {
    if (!val || !is_heap_type(type)) return;
    if (isa<Constant>(val)) return;
    if (val->getType()->isStructTy()) return;
    Value* ptr_val = val;
    if (val->getType()->isIntegerTy())
        ptr_val = builder_->CreateIntToPtr(val, PointerType::get(*context_, 0));
    builder_->CreateCall(rt_.rc_dec_, {ptr_val});
}

// ===== Return type inference =====
// Infers the LLVM type and CType of an expression without compiling it.
// Used to determine function return types before creating the LLVM function.

std::pair<llvm::Type*, CType> Codegen::infer_return_type(AstNode* node) {
    using namespace ast;
    auto i64_ty = LType::getInt64Ty(*context_);

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
        return {LType::getInt64Ty(*context_), CType::TUPLE};
    }
    if (ct == AST_APPLY_EXPR) {
        auto* app = static_cast<ApplyExpr*>(node);
        ApplyExpr* a = app;
        std::string root_name;
        while (a) {
            if (auto* nc = dynamic_cast<NameCall*>(a->call)) { root_name = nc->name->value; break; }
            else if (auto* ec = dynamic_cast<ExprCall*>(a->call)) {
                if (auto* inner = dynamic_cast<ApplyExpr*>(ec->expr)) a = inner;
                else break;
            } else break;
        }
        if (!root_name.empty()) {
            auto adt_it = types_.adt_constructors.find(root_name);
            if (adt_it != types_.adt_constructors.end()) {
                if (adt_it->second.is_recursive)
                    return {PointerType::get(*context_, 0), CType::ADT};
                auto tag_ty = i64_ty;
                std::vector<LType*> fields = {tag_ty};
                for (int f = 0; f < adt_it->second.max_arity; f++) fields.push_back(i64_ty);
                return {StructType::get(*context_, fields), CType::ADT};
            }
        }
    }
    if (ct == AST_CASE_EXPR) {
        auto* ce = static_cast<CaseExpr*>(node);
        if (!ce->clauses.empty()) return infer_return_type(ce->clauses[0]->body);
    }
    if (ct == AST_LET_EXPR) return infer_return_type(static_cast<LetExpr*>(node)->expr);
    if (ct == AST_IF_EXPR) return infer_return_type(static_cast<IfExpr*>(node)->thenExpr);
    if (ct == AST_DO_EXPR) {
        auto* de = static_cast<DoExpr*>(node);
        if (!de->steps.empty()) return infer_return_type(de->steps.back());
    }
    return {i64_ty, CType::INT};
}

// ===== Arena allocation helpers =====

llvm::Value* Codegen::emit_arena_alloc(int64_t type_tag, llvm::Value* payload_bytes) {
    if (!current_arena_) return nullptr;
    auto i64_ty = LType::getInt64Ty(*context_);
    return builder_->CreateCall(rt_.arena_alloc_,
        {current_arena_, ConstantInt::get(i64_ty, type_tag), payload_bytes},
        "arena_obj");
}

} // namespace yona::compiler::codegen
