#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "ast.h"
#include "runtime.h"

namespace yona::compiler::async {

using namespace std;
using namespace yona::ast;
using namespace yona::interp::runtime;

class DependencyAnalyzer {
public:
    struct Node {
        AstNode* expr;
        set<string> reads;   // Variables read
        set<string> writes;  // Variables written
        vector<Node*> dependencies;
        bool can_parallelize = true;

        // Check if this node depends on another
        bool depends_on(const Node* other) const {
            // This node depends on other if it reads any variable that other writes
            for (const auto& read : reads) {
                if (other->writes.count(read) > 0) {
                    return true;
                }
            }
            return false;
        }

        // Check if this node conflicts with another (mutual dependency)
        bool conflicts_with(const Node* other) const {
            // Conflict if both write to same variable
            for (const auto& write : writes) {
                if (other->writes.count(write) > 0) {
                    return true;
                }
            }
            // Or if there's a read-write dependency in either direction
            return depends_on(other) || other->depends_on(this);
        }
    };

    struct Graph {
        vector<unique_ptr<Node>> nodes;
        unordered_map<AstNode*, Node*> node_map;

        Node* get_node(AstNode* expr) {
            auto it = node_map.find(expr);
            return (it != node_map.end()) ? it->second : nullptr;
        }

        Node* add_node(AstNode* expr) {
            auto node = make_unique<Node>();
            node->expr = expr;
            Node* ptr = node.get();
            nodes.push_back(std::move(node));
            node_map[expr] = ptr;
            return ptr;
        }
    };

    // Analyze an expression and build dependency graph
    Graph analyze(AstNode* expr);

    // Analyze a let expression specifically for parallel bindings
    Graph analyze_let(LetExpr* let_expr);

    // Get groups of expressions that can run in parallel
    vector<vector<Node*>> get_parallel_groups(Graph& g);

    // Check if a set of bindings can be evaluated in parallel
    bool can_parallelize_bindings(const vector<AliasExpr*>& bindings);

private:
    // Extract variable dependencies from expressions
    void extract_dependencies(AstNode* expr, set<string>& reads, set<string>& writes);

    // Helper to extract reads from an expression
    void extract_reads(AstNode* expr, set<string>& reads);

    // Helper to extract writes from an expression
    void extract_writes(AstNode* expr, set<string>& writes);

    // Helper to extract writes from a pattern
    void extract_writes_from_pattern(PatternNode* pattern, set<string>& writes);

    // Check if an expression has side effects
    bool has_side_effects(AstNode* expr);

    // Check if an expression contains async operations
    bool contains_async_ops(AstNode* expr);

    // Build dependency edges between nodes
    void build_dependencies(Graph& g);

    // Topological sort for dependency ordering
    vector<Node*> topological_sort(const vector<Node*>& nodes);
};

// Analyzer for determining which functions can be auto-async
class AsyncAnalyzer {
public:
    struct FunctionInfo {
        string name;
        bool has_io = false;
        bool has_side_effects = false;
        bool can_be_async = false;
        bool should_be_async = false;
        set<string> calls; // Functions this function calls
    };

    // Analyze a module to determine async candidacy
    unordered_map<string, FunctionInfo> analyze_module(ModuleExpr* module);

    // Check if a function should be made async
    bool should_make_async(FunctionExpr* func);

    // Check if a call site should be async
    bool should_call_async(CallExpr* call);

private:
    // Analyze a single function
    FunctionInfo analyze_function(FunctionExpr* func);

    // Check for IO operations
    bool has_io_operations(AstNode* expr);

    // Estimate execution cost (for deciding async threshold)
    size_t estimate_cost(AstNode* expr);
};

// Runtime scheduler for parallel execution
class ParallelScheduler {
public:
    struct Task {
        function<RuntimeObjectPtr()> func;
        set<string> dependencies;
        size_t priority = 0;
    };

    // Schedule tasks based on dependencies
    vector<vector<Task>> schedule(const vector<Task>& tasks);

    // Execute tasks in parallel respecting dependencies
    vector<RuntimeObjectPtr> execute_parallel(const vector<Task>& tasks);

private:
    // Build execution order from dependency graph
    vector<vector<size_t>> build_execution_order(const vector<Task>& tasks);
};

} // namespace yona::compiler::async
