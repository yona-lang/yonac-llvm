/// TypeChecker — core HM inference for Yona.
///
/// Walks the AST and infers types for every node. Supports:
/// - Literals (Int, Float, String, Bool, Symbol, Unit)
/// - Identifiers (env lookup with instantiation)
/// - Let bindings (with let-polymorphism / generalization)
/// - Functions (parameter inference from usage)
/// - Application (unify callee with Arrow(arg, result))
/// - If expressions (condition must be Bool, branches must unify)
/// - Binary operators (dispatched via env lookup)
/// - Tuples, sequences, do-blocks

#include "typechecker/TypeChecker.h"

namespace yona::compiler::typechecker {
using namespace yona::ast;

TypeChecker::TypeChecker(DiagnosticEngine& diag)
    : unifier_(arena_, uf_, diag), diag_(diag) {
    root_env_ = std::make_shared<TypeEnv>();
    register_builtins(*root_env_, arena_);
}

MonoTypePtr TypeChecker::check(AstNode* node) {
    return infer(node, root_env_, 0);
}

MonoTypePtr TypeChecker::type_of(AstNode* node) const {
    auto it = type_map_.find(node);
    return (it != type_map_.end()) ? it->second : nullptr;
}

void TypeChecker::record(AstNode* node, MonoTypePtr type) {
    type_map_[node] = type;
}

MonoTypePtr TypeChecker::zonk(MonoTypePtr type) {
    return unifier_.resolve(type);
}

// ===== Main Dispatch =====

MonoTypePtr TypeChecker::infer(AstNode* node, std::shared_ptr<TypeEnv> env, int level) {
    if (!node) return arena_.make_con(TyCon::Unit);

    MonoTypePtr result = nullptr;
    auto ty = node->get_type();

    switch (ty) {
        case AST_INTEGER_EXPR:
            result = infer_integer(node); break;
        case AST_FLOAT_EXPR:
            result = infer_float(node); break;
        case AST_STRING_EXPR:
            result = infer_string(node); break;
        case AST_TRUE_LITERAL_EXPR:
        case AST_FALSE_LITERAL_EXPR:
            result = infer_bool(node); break;
        case AST_SYMBOL_EXPR:
            result = infer_symbol(node); break;
        case AST_LITERAL_EXPR: {
            // LiteralExpr<T> — determine type from the actual literal value type
            if (dynamic_cast<LiteralExpr<float>*>(node))
                result = infer_float(node);
            else if (dynamic_cast<LiteralExpr<bool>*>(node))
                result = infer_bool(node);
            else if (dynamic_cast<LiteralExpr<std::string>*>(node))
                result = infer_string(node);
            else if (dynamic_cast<LiteralExpr<int>*>(node))
                result = infer_integer(node);
            else
                result = arena_.make_con(TyCon::Unit);
            break;
        }
        case AST_UNIT_EXPR:
            result = arena_.make_con(TyCon::Unit); break;
        case AST_MAIN:
            result = infer(static_cast<MainNode*>(node)->node, env, level); break;
        case AST_IDENTIFIER_EXPR:
            result = infer_identifier(static_cast<IdentifierExpr*>(node), env, level); break;
        case AST_LET_EXPR:
            result = infer_let(static_cast<LetExpr*>(node), env, level); break;
        case AST_FUNCTION_EXPR:
            result = infer_function(static_cast<FunctionExpr*>(node), env, level); break;
        case AST_APPLY_EXPR:
            result = infer_apply(static_cast<ApplyExpr*>(node), env, level); break;
        case AST_IF_EXPR:
            result = infer_if(static_cast<IfExpr*>(node), env, level); break;
        case AST_TUPLE_EXPR:
            result = infer_tuple(static_cast<TupleExpr*>(node), env, level); break;
        case AST_VALUES_SEQUENCE_EXPR:
            result = infer_seq(static_cast<ValuesSequenceExpr*>(node), env, level); break;
        case AST_DO_EXPR:
            result = infer_do(static_cast<DoExpr*>(node), env, level); break;
        default:
            // Binary operators
            if (auto* binop = dynamic_cast<BinaryOpExpr*>(node)) {
                result = infer_binary(binop, env, level);
                break;
            }
            // Fallback: return a fresh variable
            result = arena_.fresh_var(level);
            uf_.add_var(result->var_id, level);
            break;
    }

    if (result) record(node, result);
    return result;
}

// ===== Literals =====

MonoTypePtr TypeChecker::infer_integer(AstNode*) { return arena_.make_con(TyCon::Int); }
MonoTypePtr TypeChecker::infer_float(AstNode*)   { return arena_.make_con(TyCon::Float); }
MonoTypePtr TypeChecker::infer_string(AstNode*)  { return arena_.make_con(TyCon::String); }
MonoTypePtr TypeChecker::infer_bool(AstNode*)    { return arena_.make_con(TyCon::Bool); }
MonoTypePtr TypeChecker::infer_symbol(AstNode*)  { return arena_.make_con(TyCon::Symbol); }

// ===== Identifier =====

MonoTypePtr TypeChecker::infer_identifier(IdentifierExpr* node,
                                           std::shared_ptr<TypeEnv> env, int level) {
    auto scheme = env->lookup(node->name->value);
    if (!scheme) {
        diag_.error(node->source_context, "undefined variable '" + node->name->value + "'");
        error_count_++;
        return arena_.fresh_var(level);
    }
    return instantiate(*scheme, level);
}

// ===== Let Binding =====

MonoTypePtr TypeChecker::infer_let(LetExpr* node, std::shared_ptr<TypeEnv> env, int level) {
    auto child_env = env->child();

    for (auto* alias : node->aliases) {
        if (auto* va = dynamic_cast<ValueAlias*>(alias)) {
            // Infer RHS at level+1 for generalization
            auto* rhs_type = infer(va->expr, child_env, level + 1);
            // Generalize: vars at level+1 become quantified
            auto scheme = generalize(rhs_type, level);
            child_env->bind_scheme(va->identifier->name->value, scheme);
        } else if (auto* la = dynamic_cast<LambdaAlias*>(alias)) {
            auto* fn_type = infer(la->lambda, child_env, level + 1);
            auto scheme = generalize(fn_type, level);
            child_env->bind_scheme(la->name->value, scheme);
        }
    }

    return infer(node->expr, child_env, level);
}

// ===== Function =====

MonoTypePtr TypeChecker::infer_function(FunctionExpr* node,
                                         std::shared_ptr<TypeEnv> env, int level) {
    auto fn_env = env->child();

    // Create fresh type variables for parameters
    std::vector<MonoTypePtr> param_types;
    for (auto* pat : node->patterns) {
        auto* param_var = arena_.fresh_var(level);
        uf_.add_var(param_var->var_id, level);
        param_types.push_back(param_var);

        // Bind pattern variable name
        if (pat->get_type() == AST_PATTERN_VALUE) {
            auto* pv = static_cast<PatternValue*>(pat);
            if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr))
                fn_env->bind((*id)->name->value, param_var);
        }
    }

    // Infer body type
    MonoTypePtr body_type = nullptr;
    for (auto* body : node->bodies) {
        if (auto* bwg = dynamic_cast<BodyWithoutGuards*>(body)) {
            auto* bt = infer(bwg->expr, fn_env, level);
            if (!body_type) body_type = bt;
            else unifier_.unify(body_type, bt, node->source_context, "in function body");
        }
    }
    if (!body_type) body_type = arena_.make_con(TyCon::Unit);

    // Build curried arrow type: a -> b -> c -> ret
    MonoTypePtr fn_type = body_type;
    for (int i = (int)param_types.size() - 1; i >= 0; i--)
        fn_type = arena_.make_arrow(param_types[i], fn_type);

    return fn_type;
}

// ===== Application =====

MonoTypePtr TypeChecker::infer_apply(ApplyExpr* node, std::shared_ptr<TypeEnv> env, int level) {
    // Infer callee
    MonoTypePtr callee_type = nullptr;
    if (auto* nc = dynamic_cast<NameCall*>(node->call)) {
        auto scheme = env->lookup(nc->name->value);
        if (scheme)
            callee_type = instantiate(*scheme, level);
        else {
            diag_.error(node->source_context, "undefined function '" + nc->name->value + "'");
            error_count_++;
            return arena_.fresh_var(level);
        }
    } else if (auto* ec = dynamic_cast<ExprCall*>(node->call)) {
        callee_type = infer(ec->expr, env, level);
    } else {
        callee_type = arena_.fresh_var(level);
        uf_.add_var(callee_type->var_id, level);
    }

    // Apply each argument
    MonoTypePtr result_type = callee_type;
    for (auto& arg_variant : node->args) {
        AstNode* arg_node = std::holds_alternative<ExprNode*>(arg_variant)
            ? static_cast<AstNode*>(std::get<ExprNode*>(arg_variant))
            : static_cast<AstNode*>(std::get<ValueExpr*>(arg_variant));

        auto* arg_type = infer(arg_node, env, level);

        auto* result_var = arena_.fresh_var(level);
        uf_.add_var(result_var->var_id, level);
        auto* expected_fn = arena_.make_arrow(arg_type, result_var);

        if (!unifier_.unify(result_type, expected_fn, node->source_context,
                            "in function application"))
            return result_var;

        result_type = result_var;
    }

    return unifier_.resolve(result_type);
}

// ===== If =====

MonoTypePtr TypeChecker::infer_if(IfExpr* node, std::shared_ptr<TypeEnv> env, int level) {
    auto* cond_type = infer(node->condition, env, level);
    unifier_.unify(cond_type, arena_.make_con(TyCon::Bool), node->source_context,
                   "in if condition (expected Bool)");

    auto* then_type = infer(node->thenExpr, env, level);
    auto* else_type = infer(node->elseExpr, env, level);
    unifier_.unify(then_type, else_type, node->source_context,
                   "in if branches (then and else must have same type)");

    return unifier_.resolve(then_type);
}

// ===== Binary Operators =====

std::string TypeChecker::op_name(AstNodeType type) {
    switch (type) {
        case AST_ADD_EXPR: return "+";
        case AST_SUBTRACT_EXPR: return "-";
        case AST_MULTIPLY_EXPR: return "*";
        case AST_DIVIDE_EXPR: return "/";
        case AST_MODULO_EXPR: return "%";
        case AST_POWER_EXPR: return "**";
        case AST_EQ_EXPR: return "==";
        case AST_NEQ_EXPR: return "!=";
        case AST_LT_EXPR: return "<";
        case AST_GT_EXPR: return ">";
        case AST_LTE_EXPR: return "<=";
        case AST_GTE_EXPR: return ">=";
        case AST_LOGICAL_AND_EXPR: return "&&";
        case AST_LOGICAL_OR_EXPR: return "||";
        case AST_JOIN_EXPR: return "++";
        case AST_CONS_LEFT_EXPR: return "::";
        case AST_PIPE_LEFT_EXPR: return "|>";
        default: return "?";
    }
}

MonoTypePtr TypeChecker::infer_binary(BinaryOpExpr* node,
                                       std::shared_ptr<TypeEnv> env, int level) {
    auto* left_type = infer(node->left, env, level);
    auto* right_type = infer(node->right, env, level);

    std::string name = op_name(node->get_type());
    auto scheme = env->lookup(name);
    if (!scheme) {
        // Fallback: assume left_type -> left_type -> left_type
        unifier_.unify(left_type, right_type, node->source_context, "in operator " + name);
        return unifier_.resolve(left_type);
    }

    // Instantiate operator type and unify with arguments
    auto* op_type = instantiate(*scheme, level);
    auto* result_var = arena_.fresh_var(level);
    uf_.add_var(result_var->var_id, level);

    auto* expected = arena_.make_arrow(left_type, arena_.make_arrow(right_type, result_var));
    unifier_.unify(op_type, expected, node->source_context, "in operator " + name);

    return unifier_.resolve(result_var);
}

// ===== Tuple =====

MonoTypePtr TypeChecker::infer_tuple(TupleExpr* node,
                                      std::shared_ptr<TypeEnv> env, int level) {
    std::vector<MonoTypePtr> elem_types;
    for (auto* v : node->values)
        elem_types.push_back(infer(v, env, level));
    return arena_.make_tuple(elem_types);
}

// ===== Sequence =====

MonoTypePtr TypeChecker::infer_seq(ValuesSequenceExpr* node,
                                    std::shared_ptr<TypeEnv> env, int level) {
    if (node->values.empty())
        return arena_.make_app("Seq", {arena_.fresh_var(level)});

    auto* elem_type = infer(node->values[0], env, level);
    for (size_t i = 1; i < node->values.size(); i++) {
        auto* t = infer(node->values[i], env, level);
        unifier_.unify(elem_type, t, node->source_context,
                       "in sequence literal (all elements must have same type)");
    }
    return arena_.make_app("Seq", {unifier_.resolve(elem_type)});
}

// ===== Do Block =====

MonoTypePtr TypeChecker::infer_do(DoExpr* node, std::shared_ptr<TypeEnv> env, int level) {
    MonoTypePtr last_type = arena_.make_con(TyCon::Unit);
    for (auto* step : node->steps)
        last_type = infer(step, env, level);
    return last_type;
}

// ===== Generalization =====

void TypeChecker::collect_free_vars(MonoTypePtr type, int level, std::vector<TypeId>& vars) {
    type = unifier_.resolve(type);
    if (!type) return;
    if (type->tag == MonoType::Var) {
        if (uf_.level(type->var_id) > level) {
            // Check not already collected
            for (auto id : vars) if (id == type->var_id) return;
            vars.push_back(type->var_id);
        }
        return;
    }
    if (type->tag == MonoType::Arrow) {
        collect_free_vars(type->param_type, level, vars);
        collect_free_vars(type->return_type, level, vars);
    }
    if (type->tag == MonoType::App)
        for (auto* a : type->args) collect_free_vars(a, level, vars);
    if (type->tag == MonoType::MTuple)
        for (auto* e : type->elements) collect_free_vars(e, level, vars);
}

TypeScheme TypeChecker::generalize(MonoTypePtr type, int level) {
    std::vector<TypeId> free_vars;
    collect_free_vars(type, level, free_vars);
    return TypeScheme(free_vars, type);
}

MonoTypePtr TypeChecker::substitute(MonoTypePtr type,
                                     const std::unordered_map<TypeId, MonoTypePtr>& subst) {
    type = unifier_.resolve(type);
    if (!type) return nullptr;
    if (type->tag == MonoType::Var) {
        auto it = subst.find(type->var_id);
        if (it != subst.end()) return it->second;
        return type;
    }
    if (type->tag == MonoType::Con) return type;
    if (type->tag == MonoType::Arrow)
        return arena_.make_arrow(substitute(type->param_type, subst),
                                  substitute(type->return_type, subst));
    if (type->tag == MonoType::App) {
        std::vector<MonoTypePtr> new_args;
        for (auto* a : type->args) new_args.push_back(substitute(a, subst));
        return arena_.make_app(type->type_name, new_args);
    }
    if (type->tag == MonoType::MTuple) {
        std::vector<MonoTypePtr> new_elems;
        for (auto* e : type->elements) new_elems.push_back(substitute(e, subst));
        return arena_.make_tuple(new_elems);
    }
    return type;
}

MonoTypePtr TypeChecker::instantiate(const TypeScheme& scheme, int level) {
    if (scheme.quantified_vars.empty()) return scheme.body;

    std::unordered_map<TypeId, MonoTypePtr> subst;
    for (auto id : scheme.quantified_vars) {
        auto* fresh = arena_.fresh_var(level);
        uf_.add_var(fresh->var_id, level);
        subst[id] = fresh;
    }
    return substitute(scheme.body, subst);
}

} // namespace yona::compiler::typechecker
