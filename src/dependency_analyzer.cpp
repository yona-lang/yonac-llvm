#include "dependency_analyzer.h"
#include <algorithm>
#include <queue>

namespace yona::compiler::async {

using namespace std;

DependencyAnalyzer::Graph DependencyAnalyzer::analyze(AstNode* expr) {
    Graph graph;

    if (!expr) return graph;

    // Special handling for let expressions
    if (expr->get_type() == AST_LET_EXPR) {
        return analyze_let(static_cast<LetExpr*>(expr));
    }

    // For other expressions, create a single node
    auto node = graph.add_node(expr);
    extract_dependencies(expr, node->reads, node->writes);
    node->can_parallelize = !has_side_effects(expr);

    return graph;
}

DependencyAnalyzer::Graph DependencyAnalyzer::analyze_let(LetExpr* let_expr) {
    Graph graph;

    if (!let_expr || let_expr->aliases.empty()) return graph;

    // Create nodes for each binding
    for (auto* alias : let_expr->aliases) {
        auto node = graph.add_node(alias);

        // Extract dependencies based on alias type
        if (auto value_alias = dynamic_cast<ValueAlias*>(alias)) {
            if (value_alias->expr) {
                extract_dependencies(value_alias->expr, node->reads, node->writes);
            }
            if (value_alias->identifier && value_alias->identifier->name) {
                node->writes.insert(value_alias->identifier->name->value);
            }
        } else if (auto lambda_alias = dynamic_cast<LambdaAlias*>(alias)) {
            // Lambdas don't execute immediately, so minimal dependencies
            if (lambda_alias->name && lambda_alias->name->name) {
                node->writes.insert(lambda_alias->name->name->value);
            }
            node->can_parallelize = true;
        } else if (auto func_alias = dynamic_cast<FunctionAlias*>(alias)) {
            if (func_alias->name && func_alias->name->name) {
                node->writes.insert(func_alias->name->name->value);
            }
            // Function definitions can be parallel
            node->can_parallelize = true;
        } else if (auto pattern_alias = dynamic_cast<PatternAlias*>(alias)) {
            if (pattern_alias->expr) {
                extract_dependencies(pattern_alias->expr, node->reads, node->writes);
            }
            // Extract writes from pattern
            extract_writes_from_pattern(pattern_alias->pattern, node->writes);
        }

        // Check for side effects
        if (has_side_effects(alias)) {
            node->can_parallelize = false;
        }
    }

    // Build dependency edges
    build_dependencies(graph);

    // Add body node if present
    if (let_expr->expr) {
        auto body_node = graph.add_node(let_expr->expr);
        extract_dependencies(let_expr->expr, body_node->reads, body_node->writes);

        // Body depends on all bindings that it reads
        for (auto& node : graph.nodes) {
            if (node.get() != body_node) {
                if (body_node->depends_on(node.get())) {
                    body_node->dependencies.push_back(node.get());
                }
            }
        }
    }

    return graph;
}

vector<vector<DependencyAnalyzer::Node*>> DependencyAnalyzer::get_parallel_groups(Graph& g) {
    vector<vector<Node*>> groups;

    if (g.nodes.empty()) return groups;

    // Get all nodes except the body (last node if it exists)
    vector<Node*> nodes_to_schedule;
    for (auto& node : g.nodes) {
        nodes_to_schedule.push_back(node.get());
    }

    // Topological sort to respect dependencies
    auto sorted = topological_sort(nodes_to_schedule);

    // Group nodes that can run in parallel
    set<Node*> scheduled;

    while (scheduled.size() < sorted.size()) {
        vector<Node*> current_group;

        for (auto* node : sorted) {
            if (scheduled.count(node) > 0) continue;

            // Check if all dependencies are scheduled
            bool can_schedule = true;
            for (auto* dep : node->dependencies) {
                if (scheduled.count(dep) == 0) {
                    can_schedule = false;
                    break;
                }
            }

            if (!can_schedule) continue;

            // Check if conflicts with anything in current group
            bool conflicts = false;
            for (auto* other : current_group) {
                if (node->conflicts_with(other)) {
                    conflicts = true;
                    break;
                }
            }

            if (!conflicts && node->can_parallelize) {
                current_group.push_back(node);
            }
        }

        // If no parallel nodes found, take the next sequential one
        if (current_group.empty()) {
            for (auto* node : sorted) {
                if (scheduled.count(node) == 0) {
                    bool can_schedule = true;
                    for (auto* dep : node->dependencies) {
                        if (scheduled.count(dep) == 0) {
                            can_schedule = false;
                            break;
                        }
                    }
                    if (can_schedule) {
                        current_group.push_back(node);
                        break;
                    }
                }
            }
        }

        if (!current_group.empty()) {
            groups.push_back(current_group);
            for (auto* node : current_group) {
                scheduled.insert(node);
            }
        } else {
            break; // Shouldn't happen with valid dependency graph
        }
    }

    return groups;
}

bool DependencyAnalyzer::can_parallelize_bindings(const vector<AliasExpr*>& bindings) {
    Graph g;

    for (auto* binding : bindings) {
        auto node = g.add_node(binding);

        // Quick check for obvious non-parallelizable cases
        if (has_side_effects(binding)) {
            return false;
        }
    }

    // Build and analyze dependency graph
    build_dependencies(g);

    // Check for any dependencies between bindings
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        for (size_t j = i + 1; j < g.nodes.size(); ++j) {
            if (g.nodes[i]->conflicts_with(g.nodes[j].get())) {
                return false;
            }
        }
    }

    return true;
}

void DependencyAnalyzer::extract_dependencies(AstNode* expr, set<string>& reads, set<string>& writes) {
    if (!expr) return;

    extract_reads(expr, reads);
    extract_writes(expr, writes);
}

void DependencyAnalyzer::extract_reads(AstNode* expr, set<string>& reads) {
    if (!expr) return;

    switch (expr->get_type()) {
        case AST_IDENTIFIER_EXPR: {
            auto id = static_cast<IdentifierExpr*>(expr);
            if (id->name) {
                reads.insert(id->name->value);
            }
            break;
        }
        case AST_APPLY_EXPR: {
            auto apply = static_cast<ApplyExpr*>(expr);
            extract_reads(apply->call, reads);
            for (auto& arg : apply->args) {
                if (auto* expr_arg = get_if<ExprNode*>(&arg)) {
                    extract_reads(*expr_arg, reads);
                }
            }
            break;
        }
        case AST_BINARY_OP_EXPR: {
            auto bin = static_cast<BinaryOpExpr*>(expr);
            extract_reads(bin->left, reads);
            extract_reads(bin->right, reads);
            break;
        }
        case AST_IF_EXPR: {
            auto if_expr = static_cast<IfExpr*>(expr);
            extract_reads(if_expr->condition, reads);
            extract_reads(if_expr->thenExpr, reads);
            extract_reads(if_expr->elseExpr, reads);
            break;
        }
        case AST_LET_EXPR: {
            auto let = static_cast<LetExpr*>(expr);
            // Analyze bindings
            for (auto* alias : let->aliases) {
                if (auto value = dynamic_cast<ValueAlias*>(alias)) {
                    extract_reads(value->expr, reads);
                } else if (auto pattern = dynamic_cast<PatternAlias*>(alias)) {
                    extract_reads(pattern->expr, reads);
                }
            }
            extract_reads(let->expr, reads);
            break;
        }
        // Add more cases as needed
        default:
            break;
    }
}

void DependencyAnalyzer::extract_writes(AstNode* expr, set<string>& writes) {
    if (!expr) return;
    // This function now only handles regular AST nodes
    // For patterns, use extract_writes_from_pattern
}

void DependencyAnalyzer::extract_writes_from_pattern(PatternNode* pattern, set<string>& writes) {
    if (!pattern) return;

    // Pattern matching extracts variable bindings as writes
    switch (pattern->get_type()) {
        case AST_IDENTIFIER_EXPR: {
            // In patterns, identifiers are bindings
            if (auto id = dynamic_cast<IdentifierExpr*>(pattern)) {
                if (id->name && id->name->value != "_") {
                    writes.insert(id->name->value);
                }
            }
            break;
        }
        case AST_SEQ_PATTERN: {
            auto seq = static_cast<SeqPattern*>(pattern);
            for (auto* p : seq->patterns) {
                extract_writes_from_pattern(p, writes);
            }
            break;
        }
        case AST_HEAD_TAILS_PATTERN: {
            auto ht = static_cast<HeadTailsPattern*>(pattern);
            for (auto* head : ht->heads) {
                extract_writes_from_pattern(head, writes);
            }
            if (ht->tail) {
                extract_writes_from_pattern(ht->tail, writes);
            }
            break;
        }
        case AST_TUPLE_PATTERN: {
            auto tuple = static_cast<TuplePattern*>(pattern);
            for (auto* elem : tuple->items) {
                extract_writes_from_pattern(elem, writes);
            }
            break;
        }
        case AST_RECORD_PATTERN: {
            auto record = static_cast<RecordPattern*>(pattern);
            for (const auto& item : record->items) {
                extract_writes_from_pattern(item.second, writes);
            }
            break;
        }
        // Add more pattern cases
        default:
            break;
    }
}

bool DependencyAnalyzer::has_side_effects(AstNode* expr) {
    if (!expr) return false;

    // Check for IO operations, mutations, exceptions, etc.
    switch (expr->get_type()) {
        case AST_APPLY_EXPR: {
            auto apply = static_cast<ApplyExpr*>(expr);
            // Check if calling known side-effect functions
            if (apply->call && apply->call->get_type() == AST_IDENTIFIER_EXPR) {
                auto id = static_cast<IdentifierExpr*>(apply->call);
                // List of known side-effect functions
                static const set<string> side_effect_funcs = {
                    "print", "println", "read", "write", "open", "close",
                    "send", "receive", "throw", "raise", "mutate", "set"
                };
                if (id->name && side_effect_funcs.count(id->name->value) > 0) {
                    return true;
                }
            }
            // Recursively check arguments
            if (has_side_effects(apply->call)) return true;
            for (auto& arg : apply->args) {
                if (auto* expr_arg = get_if<ExprNode*>(&arg)) {
                    if (has_side_effects(*expr_arg)) return true;
                }
            }
            break;
        }
        case AST_RAISE_EXPR:
        case AST_DO_EXPR:
            return true;
        default:
            // Recursively check children for composite expressions
            break;
    }

    return false;
}

bool DependencyAnalyzer::contains_async_ops(AstNode* expr) {
    if (!expr) return false;

    // Check for async operations
    switch (expr->get_type()) {
        case AST_APPLY_EXPR: {
            auto apply = static_cast<ApplyExpr*>(expr);
            if (apply->call && apply->call->get_type() == AST_IDENTIFIER_EXPR) {
                auto id = static_cast<IdentifierExpr*>(apply->call);
                // List of async operations
                static const set<string> async_ops = {
                    "async", "await", "spawn", "fork", "parallel",
                    "http_get", "http_post", "file_read", "file_write"
                };
                if (id->name && async_ops.count(id->name->value) > 0) {
                    return true;
                }
            }
            break;
        }
        default:
            break;
    }

    return false;
}

void DependencyAnalyzer::build_dependencies(Graph& g) {
    // Build dependencies between nodes
    for (size_t i = 0; i < g.nodes.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (g.nodes[i]->depends_on(g.nodes[j].get())) {
                g.nodes[i]->dependencies.push_back(g.nodes[j].get());
            }
        }
    }
}

vector<DependencyAnalyzer::Node*> DependencyAnalyzer::topological_sort(const vector<Node*>& nodes) {
    vector<Node*> result;
    unordered_map<Node*, int> in_degree;

    // Calculate in-degrees
    for (auto* node : nodes) {
        in_degree[node] = 0;
    }
    for (auto* node : nodes) {
        for (auto* dep : node->dependencies) {
            in_degree[node]++;
        }
    }

    // Queue for nodes with no dependencies
    queue<Node*> q;
    for (auto* node : nodes) {
        if (in_degree[node] == 0) {
            q.push(node);
        }
    }

    // Process nodes
    while (!q.empty()) {
        auto* node = q.front();
        q.pop();
        result.push_back(node);

        // Update dependent nodes
        for (auto* other : nodes) {
            bool depends = false;
            for (auto* dep : other->dependencies) {
                if (dep == node) {
                    depends = true;
                    break;
                }
            }
            if (depends) {
                in_degree[other]--;
                if (in_degree[other] == 0) {
                    q.push(other);
                }
            }
        }
    }

    return result;
}

// AsyncAnalyzer implementation

unordered_map<string, AsyncAnalyzer::FunctionInfo> AsyncAnalyzer::analyze_module(ModuleExpr* module) {
    unordered_map<string, FunctionInfo> result;

    if (!module || !module->definitions) return result;

    // First pass: collect all functions
    for (auto* def : module->definitions->items) {
        if (def->get_type() == AST_FUNCTION_EXPR) {
            auto func = static_cast<FunctionExpr*>(def);
            auto info = analyze_function(func);
            result[func->name] = info;
        }
    }

    // Second pass: determine async candidacy
    for (auto& [name, info] : result) {
        // Function should be async if it has IO or is expensive
        info.should_be_async = info.has_io || info.has_side_effects;
    }

    return result;
}

bool AsyncAnalyzer::should_make_async(FunctionExpr* func) {
    if (!func) return false;

    auto info = analyze_function(func);
    return info.should_be_async;
}

bool AsyncAnalyzer::should_call_async(CallExpr* call) {
    if (!call || !call->function) return false;

    // Check if calling a known async function
    if (call->function->get_type() == AST_IDENTIFIER) {
        auto id = static_cast<IdentifierExpr*>(call->function);
        static const set<string> async_funcs = {
            "file_open", "file_read", "file_write",
            "http_get", "http_post", "socket_connect"
        };
        return async_funcs.count(id->name) > 0;
    }

    return false;
}

AsyncAnalyzer::FunctionInfo AsyncAnalyzer::analyze_function(FunctionExpr* func) {
    FunctionInfo info;
    info.name = func->name;

    if (func->body) {
        info.has_io = has_io_operations(func->body);
        info.has_side_effects = has_io_operations(func->body);
        info.can_be_async = true; // Most functions can be made async

        // Estimate if worth making async
        size_t cost = estimate_cost(func->body);
        info.should_be_async = cost > 100 || info.has_io;
    }

    return info;
}

bool AsyncAnalyzer::has_io_operations(AstNode* expr) {
    if (!expr) return false;

    if (expr->get_type() == AST_CALL_EXPR) {
        auto call = static_cast<CallExpr*>(expr);
        if (call->function && call->function->get_type() == AST_IDENTIFIER) {
            auto id = static_cast<IdentifierExpr*>(call->function);
            static const set<string> io_ops = {
                "file_open", "file_read", "file_write", "file_close",
                "socket_connect", "socket_send", "socket_receive",
                "http_get", "http_post", "http_put", "http_delete",
                "print", "println", "read_line"
            };
            if (io_ops.count(id->name) > 0) {
                return true;
            }
        }
    }

    // Recursively check children
    // (Would need to implement proper visitor pattern here)

    return false;
}

size_t AsyncAnalyzer::estimate_cost(AstNode* expr) {
    if (!expr) return 0;

    size_t cost = 1; // Base cost for any expression

    switch (expr->get_type()) {
        case AST_CALL_EXPR:
            cost += 10; // Function calls are expensive
            break;
        case AST_LAMBDA_EXPR:
        case AST_FUNCTION_EXPR:
            cost += 5; // Creating functions has overhead
            break;
        case AST_LET_EXPR:
            cost += 2; // Variable binding
            break;
        case AST_IF_EXPR:
        case AST_CASE_EXPR:
            cost += 3; // Branching
            break;
        default:
            break;
    }

    // Would recursively sum costs of children here

    return cost;
}

} // namespace yona::compiler::async
