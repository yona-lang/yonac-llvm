#ifndef YONA_TYPECHECKER_LINEARITY_CHECKER_H
#define YONA_TYPECHECKER_LINEARITY_CHECKER_H

/// Linearity checker for Yona.
///
/// Tracks values of type `Linear a` (a built-in ADT) through the program.
/// A Linear value must be pattern-matched exactly once:
///   - Error if used after consumption (use-after-consume)
///   - Warning if it goes out of scope without being consumed (resource leak)
///   - Error if branches disagree on consumption (branch inconsistency)
///
/// The `Linear` ADT is the mechanism — wrapping a resource handle in `Linear`
/// creates a compile-time obligation to unwrap it via pattern matching.
///
/// Producer functions (tcpConnect, spawn, etc.) return `Linear Int`.
/// The only way to extract the inner value is `case x of Linear fd -> ...`.
/// This pattern match is the consumption point.

#include "Diagnostic.h"
#include "ast.h"
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace yona::compiler::typechecker {

/// Tracks the lifecycle status of a linear variable.
enum class LinearStatus {
    Live,      ///< Created but not yet consumed
    Consumed,  ///< Consumed by pattern match or transfer
};

/// Environment tracking linear variable obligations.
struct LinearEnv {
    /// var_name → status
    std::unordered_map<std::string, LinearStatus> vars;

    /// var_name → source location where it was created
    std::unordered_map<std::string, SourceLocation> created_at;

    /// var_name → source location where it was consumed
    std::unordered_map<std::string, SourceLocation> consumed_at;

    /// Mark a variable as a live linear value.
    void create(const std::string& name, const SourceLocation& loc);

    /// Mark a variable as consumed. Returns false if already consumed.
    bool consume(const std::string& name, const SourceLocation& loc);

    /// Check if a variable is live (created but not consumed).
    bool is_live(const std::string& name) const;

    /// Check if a variable is consumed.
    bool is_consumed(const std::string& name) const;

    /// Check if a variable is tracked at all.
    bool is_tracked(const std::string& name) const;

    /// Get all live (unconsumed) variables.
    std::vector<std::string> live_vars() const;
};

class LinearityChecker {
public:
    explicit LinearityChecker(DiagnosticEngine& diag);

    /// Register a function as a "producer" — its return value is Linear.
    void register_producer(const std::string& fn_name);

    /// Register a function as a "tuple producer" — returns a tuple of linear
    /// values. When destructured via `let (a, b) = fn ... in ...`, all
    /// destructured names are tracked as linear obligations. Used for
    /// channel which returns (Sender, Receiver).
    void register_tuple_producer(const std::string& fn_name);

    /// Check an AST tree for linearity violations.
    void check(ast::AstNode* node);

    bool has_errors() const { return error_count_ > 0; }

private:
    void check_node(ast::AstNode* node, LinearEnv& env);
    void check_let(ast::LetExpr* node, LinearEnv& env);
    void check_case(ast::CaseExpr* node, LinearEnv& env);
    void check_if(ast::IfExpr* node, LinearEnv& env);
    void check_apply(ast::ApplyExpr* node, LinearEnv& env);

    /// Warn about any live linear variables going out of scope.
    void warn_unconsumed(const LinearEnv& env);

    /// Check if a constructor name indicates a Linear wrapper.
    static bool is_linear_constructor(const std::string& name) { return name == "Linear"; }

    DiagnosticEngine& diag_;
    int error_count_ = 0;

    /// Functions whose return values become linear obligations.
    std::unordered_set<std::string> producer_functions_;

    /// Functions returning tuples of linear values (e.g., channel).
    std::unordered_set<std::string> tuple_producer_functions_;
};

} // namespace yona::compiler::typechecker

#endif // YONA_TYPECHECKER_LINEARITY_CHECKER_H
