#ifndef YONA_TYPECHECKER_UNION_FIND_H
#define YONA_TYPECHECKER_UNION_FIND_H

/// Union-find (disjoint set) for type variable unification.
///
/// Each type variable starts as its own representative.
/// unify_var(id, type) binds a variable to a concrete type.
/// find(id) returns the current binding (chasing links with path compression).

#include "InferType.h"
#include <unordered_map>

namespace yona::compiler::typechecker {

class UnionFind {
public:
    /// Register a fresh type variable.
    void add_var(TypeId id, int level);

    /// Find the representative type for a variable.
    /// If unbound, returns nullptr.
    /// Uses path compression for amortized O(α(n)).
    MonoTypePtr find(TypeId id);

    /// Bind a variable to a type.
    void bind(TypeId id, MonoTypePtr type);

    /// Get the level of a variable (for generalization).
    int level(TypeId id) const;

    /// Set the level of a variable (for level adjustment during unification).
    void set_level(TypeId id, int new_level);

    /// Check if a variable is bound.
    bool is_bound(TypeId id) const;

private:
    struct Entry {
        MonoTypePtr bound_to = nullptr; ///< nullptr = unbound root
        int level = 0;
    };
    std::unordered_map<TypeId, Entry> entries_;
};

} // namespace yona::compiler::typechecker

#endif // YONA_TYPECHECKER_UNION_FIND_H
