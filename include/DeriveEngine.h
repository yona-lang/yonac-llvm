#ifndef YONA_DERIVE_ENGINE_H
#define YONA_DERIVE_ENGINE_H

/// DeriveEngine — auto-derive trait instances for ADTs.
///
/// Self-describing strategies register themselves via static initializers.
/// Adding a new derivable trait requires only a new .cpp file that calls
/// DeriveEngine::register_strategy(). The engine is generic — no hardcoded
/// trait names anywhere in the integration code.

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace yona::compiler {
class DiagnosticEngine;
}

namespace yona::compiler::codegen {

/// Metadata for a single ADT constructor.
struct DeriveCtorInfo {
    std::string name;
    int tag;
    int arity;
    std::vector<std::string> field_names;
    /// Per-field: type param name ("a", "b") if polymorphic, empty if concrete
    std::vector<std::string> field_type_refs;
    /// Per-field: concrete type name ("Int", "String", etc.) from FieldType
    std::vector<std::string> field_type_names;
};

/// Metadata for an ADT, collected for derive expansion.
struct DeriveAdtInfo {
    std::string type_name;
    std::vector<std::string> type_params;
    std::vector<DeriveCtorInfo> constructors;
    bool is_recursive = false;
};

/// Generator function: takes ADT metadata, returns method source text.
using DeriveGeneratorFn = std::function<std::string(const DeriveAdtInfo&)>;

/// A registered derivable trait strategy.
struct DeriveStrategyInfo {
    std::string trait_name;
    std::vector<std::string> method_names;
    DeriveGeneratorFn generator;
};

/// Registry and dispatcher for auto-derive strategies.
class DeriveEngine {
public:
    /// Register a derivable trait strategy. Called from static initializers.
    /// Returns a dummy int to enable static auto-registration.
    static int register_strategy(const std::string& trait_name,
                                  std::vector<std::string> method_names,
                                  DeriveGeneratorFn generator);

    /// Check if a trait is derivable.
    static bool is_derivable(const std::string& trait_name);

    /// Get the strategy for a trait (nullptr if not derivable).
    static const DeriveStrategyInfo* get_strategy(const std::string& trait_name);

    /// Get all registered strategy names (for error messages).
    static std::vector<std::string> all_derivable_traits();

private:
    static std::unordered_map<std::string, DeriveStrategyInfo>& registry();
};

} // namespace yona::compiler::codegen

#endif // YONA_DERIVE_ENGINE_H
