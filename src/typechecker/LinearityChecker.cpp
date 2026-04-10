/// LinearityChecker — tracks Linear ADT values through the program.
///
/// `type Linear a = Linear a` is a built-in ADT that wraps resource handles.
/// The checker ensures each Linear value is pattern-matched exactly once:
///   - Construction: `Linear fd` or producer function returning Linear
///   - Consumption: `case x of Linear fd -> ...` (pattern match)
///   - Error: using a consumed value, or branch inconsistency
///   - Warning: dropping a live value at scope exit

#include "typechecker/LinearityChecker.h"

namespace yona::compiler::typechecker {
using namespace yona::ast;

// ===== LinearEnv =====

void LinearEnv::create(const std::string& name, const SourceLocation& loc) {
    vars[name] = LinearStatus::Live;
    created_at[name] = loc;
}

bool LinearEnv::consume(const std::string& name, const SourceLocation& loc) {
    auto it = vars.find(name);
    if (it == vars.end()) return true; // not tracked
    if (it->second == LinearStatus::Consumed) return false; // already consumed
    it->second = LinearStatus::Consumed;
    consumed_at[name] = loc;
    return true;
}

bool LinearEnv::is_live(const std::string& name) const {
    auto it = vars.find(name);
    return it != vars.end() && it->second == LinearStatus::Live;
}

bool LinearEnv::is_consumed(const std::string& name) const {
    auto it = vars.find(name);
    return it != vars.end() && it->second == LinearStatus::Consumed;
}

bool LinearEnv::is_tracked(const std::string& name) const {
    return vars.count(name) > 0;
}

std::vector<std::string> LinearEnv::live_vars() const {
    std::vector<std::string> result;
    for (auto& [name, status] : vars)
        if (status == LinearStatus::Live)
            result.push_back(name);
    return result;
}

// ===== LinearityChecker =====

LinearityChecker::LinearityChecker(DiagnosticEngine& diag) : diag_(diag) {
    // Register stdlib producer functions (return Linear values)
    // Net module
    register_producer("tcpConnect");
    register_producer("tcpListen");
    register_producer("tcpAccept");
    register_producer("udpBind");
    // Process module
    register_producer("spawn");
    // File module
    register_producer("openFile");
    // Channel module — returns (Sender, Receiver) tuple of linear values
    register_tuple_producer("channel");
}

void LinearityChecker::register_producer(const std::string& fn_name) {
    producer_functions_.insert(fn_name);
}

void LinearityChecker::register_tuple_producer(const std::string& fn_name) {
    tuple_producer_functions_.insert(fn_name);
}

void LinearityChecker::check(AstNode* node) {
    LinearEnv env;
    check_node(node, env);
    warn_unconsumed(env);
}

void LinearityChecker::warn_unconsumed(const LinearEnv& env) {
    for (auto& name : env.live_vars()) {
        auto it = env.created_at.find(name);
        SourceLocation loc = (it != env.created_at.end()) ? it->second : SourceLocation::unknown();
        diag_.warning(loc,
                      "linear value '" + name + "' not consumed — possible resource leak; "
                      "use `case " + name + " of Linear fd -> close fd end` to release",
                      WarningFlag::UnhandledEffect); // reuse existing warning flag for now
    }
}

void LinearityChecker::check_node(AstNode* node, LinearEnv& env) {
    if (!node) return;

    switch (node->get_type()) {
        case AST_MAIN:
            check_node(static_cast<MainNode*>(node)->node, env);
            break;
        case AST_LET_EXPR:
            check_let(static_cast<LetExpr*>(node), env);
            break;
        case AST_CASE_EXPR:
            check_case(static_cast<CaseExpr*>(node), env);
            break;
        case AST_IF_EXPR:
            check_if(static_cast<IfExpr*>(node), env);
            break;
        case AST_APPLY_EXPR:
            check_apply(static_cast<ApplyExpr*>(node), env);
            break;
        case AST_DO_EXPR: {
            auto* doex = static_cast<DoExpr*>(node);
            for (auto* step : doex->steps)
                check_node(step, env);
            break;
        }
        default:
            if (auto* binop = dynamic_cast<BinaryOpExpr*>(node))  {
                check_node(binop->left, env);
                check_node(binop->right, env);
            }
            break;
    }
}

void LinearityChecker::check_let(LetExpr* node, LinearEnv& env) {
    for (auto* alias : node->aliases) {
        if (auto* va = dynamic_cast<ValueAlias*>(alias)) {
            check_node(va->expr, env);
            std::string name = va->identifier->name->value;

            // Check if RHS is a Linear constructor or producer function call.
            // Multi-arg calls produce nested ApplyExprs: Apply(Apply(f, a1), a2).
            // Walk to the innermost call to find the function name.
            if (va->expr->get_type() == AST_APPLY_EXPR) {
                auto* apply = static_cast<ApplyExpr*>(va->expr);
                // Find the root function name through nested applies
                std::string fn_name;
                auto* cur = apply;
                while (cur) {
                    if (auto* nc = dynamic_cast<NameCall*>(cur->call)) {
                        fn_name = nc->name->value;
                        break;
                    }
                    if (auto* ec = dynamic_cast<ExprCall*>(cur->call)) {
                        if (ec->expr->get_type() == AST_APPLY_EXPR)
                            cur = static_cast<ApplyExpr*>(ec->expr);
                        else {
                            if (ec->expr->get_type() == AST_IDENTIFIER_EXPR)
                                fn_name = static_cast<IdentifierExpr*>(ec->expr)->name->value;
                            break;
                        }
                    } else break;
                }
                if (!fn_name.empty()) {
                    if (is_linear_constructor(fn_name))
                        env.create(name, va->source_context);
                    if (producer_functions_.count(fn_name))
                        env.create(name, va->source_context);
                }
            }

            // Identifier alias: transfer linear obligation
            if (va->expr->get_type() == AST_IDENTIFIER_EXPR) {
                auto* id = static_cast<IdentifierExpr*>(va->expr);
                std::string src = id->name->value;
                if (env.is_live(src)) {
                    // Transfer: old name consumed, new name is live
                    env.consume(src, va->source_context);
                    env.create(name, va->source_context);
                } else if (env.is_consumed(src)) {
                    diag_.error(va->source_context, ErrorCode::E0600,
                                "linear value '" + src + "' was already consumed");
                    error_count_++;
                }
            }
        } else if (auto* la = dynamic_cast<LambdaAlias*>(alias)) {
            // Check if lambda body captures any linear values
            check_node(la->lambda, env);
        } else if (auto* pa = dynamic_cast<PatternAlias*>(alias)) {
            check_node(pa->expr, env);
            // Tuple destructuring: let (a, b) = expr in ...
            // If expr is a call to a tuple_producer function, mark all
            // pattern names as linear obligations.
            if (pa->expr->get_type() == AST_APPLY_EXPR) {
                auto* apply = static_cast<ApplyExpr*>(pa->expr);
                std::string fn_name;
                auto* cur = apply;
                while (cur) {
                    if (auto* nc = dynamic_cast<NameCall*>(cur->call)) {
                        fn_name = nc->name->value;
                        break;
                    }
                    if (auto* ec = dynamic_cast<ExprCall*>(cur->call)) {
                        if (ec->expr->get_type() == AST_APPLY_EXPR)
                            cur = static_cast<ApplyExpr*>(ec->expr);
                        else {
                            if (ec->expr->get_type() == AST_IDENTIFIER_EXPR)
                                fn_name = static_cast<IdentifierExpr*>(ec->expr)->name->value;
                            break;
                        }
                    } else break;
                }
                if (!fn_name.empty() && tuple_producer_functions_.count(fn_name)) {
                    // Walk the pattern and mark each identifier name as linear
                    if (pa->pattern && pa->pattern->get_type() == AST_TUPLE_PATTERN) {
                        auto* tp = static_cast<TuplePattern*>(pa->pattern);
                        for (auto* sub : tp->patterns) {
                            if (sub->get_type() == AST_PATTERN_VALUE) {
                                auto* pv = static_cast<PatternValue*>(sub);
                                if (auto* id = std::get_if<IdentifierExpr*>(&pv->expr)) {
                                    env.create((*id)->name->value, pa->source_context);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    check_node(node->expr, env);
}

void LinearityChecker::check_case(CaseExpr* node, LinearEnv& env) {
    check_node(node->expr, env);

    // Determine scrutinee variable name
    std::string scrut_name;
    if (node->expr->get_type() == AST_IDENTIFIER_EXPR)
        scrut_name = static_cast<IdentifierExpr*>(node->expr)->name->value;

    // Track which branches consume the scrutinee
    bool any_branch_consumes = false;
    bool any_branch_skips = false;

    for (auto* clause : node->clauses) {
        LinearEnv branch_env = env;

        if (!scrut_name.empty() && env.is_live(scrut_name)) {
            auto* pat = clause->pattern;

            // Constructor pattern `Linear fd` — this is the consumption point
            if (pat->get_type() == AST_CONSTRUCTOR_PATTERN) {
                auto* cp = static_cast<ConstructorPattern*>(pat);
                if (is_linear_constructor(cp->constructor_name)) {
                    branch_env.consume(scrut_name, clause->source_context);
                    any_branch_consumes = true;
                } else {
                    any_branch_skips = true;
                }
            } else {
                // Non-constructor patterns don't consume the linear value
                any_branch_skips = true;
            }
        }

        // Check for use-after-consume in branch body
        check_node(clause->body, branch_env);
    }

    // Branch consistency: if some branches consume and others don't, that's an issue
    // (but only if the scrutinee was linear)
    if (!scrut_name.empty() && env.is_live(scrut_name)) {
        if (any_branch_consumes && any_branch_skips) {
            diag_.error(node->source_context, ErrorCode::E0601,
                        "linear value '" + scrut_name +
                        "' consumed in some case branches but not all");
            error_count_++;
        }
        // If all branches consume, mark consumed in the outer env
        if (any_branch_consumes && !any_branch_skips) {
            env.consume(scrut_name, node->source_context);
        }
    }
}

void LinearityChecker::check_if(IfExpr* node, LinearEnv& env) {
    check_node(node->condition, env);

    LinearEnv then_env = env;
    LinearEnv else_env = env;

    check_node(node->thenExpr, then_env);
    check_node(node->elseExpr, else_env);

    // Check branch consistency for all tracked linear variables
    for (auto& [name, status] : env.vars) {
        if (status != LinearStatus::Live) continue;
        bool then_consumed = then_env.is_consumed(name);
        bool else_consumed = else_env.is_consumed(name);

        if (then_consumed && !else_consumed) {
            diag_.error(node->source_context, ErrorCode::E0601,
                        "linear value '" + name +
                        "' consumed in then-branch but not in else-branch");
            error_count_++;
        } else if (!then_consumed && else_consumed) {
            diag_.error(node->source_context, ErrorCode::E0601,
                        "linear value '" + name +
                        "' consumed in else-branch but not in then-branch");
            error_count_++;
        } else if (then_consumed && else_consumed) {
            env.consume(name, node->source_context);
        }
    }
}

void LinearityChecker::check_apply(ApplyExpr* node, LinearEnv& env) {
    // Check if any argument is a consumed linear variable
    for (auto& arg_variant : node->args) {
        AstNode* arg_node = std::holds_alternative<ExprNode*>(arg_variant)
            ? static_cast<AstNode*>(std::get<ExprNode*>(arg_variant))
            : static_cast<AstNode*>(std::get<ValueExpr*>(arg_variant));

        if (arg_node && arg_node->get_type() == AST_IDENTIFIER_EXPR) {
            auto* id = static_cast<IdentifierExpr*>(arg_node);
            std::string arg_name = id->name->value;
            if (env.is_consumed(arg_name)) {
                auto it = env.consumed_at.find(arg_name);
                std::string consumed_loc = (it != env.consumed_at.end())
                    ? " (consumed at " + it->second.to_string() + ")"
                    : "";
                diag_.error(node->source_context, ErrorCode::E0600,
                            "linear value '" + arg_name + "' was already consumed" + consumed_loc);
                error_count_++;
            }
        }
    }

    // Check for producer function calls handled in check_let
    // (direct apply without let binding — the linear value is immediately consumed)

    // Recurse into arguments
    for (auto& arg_variant : node->args) {
        AstNode* arg_node = std::holds_alternative<ExprNode*>(arg_variant)
            ? static_cast<AstNode*>(std::get<ExprNode*>(arg_variant))
            : static_cast<AstNode*>(std::get<ValueExpr*>(arg_variant));
        check_node(arg_node, env);
    }
}

} // namespace yona::compiler::typechecker
