/// DeriveEngine — registry and dispatcher for auto-derive strategies.
///
/// Each derivable trait registers itself via a static initializer at the
/// bottom of this file. Adding a new derivable trait is self-contained:
/// write the generator function and add one register_strategy() call.

#include "DeriveEngine.h"
#include <sstream>

namespace yona::compiler::codegen {

// ===== Registry =====

std::unordered_map<std::string, DeriveStrategyInfo>& DeriveEngine::registry() {
    static std::unordered_map<std::string, DeriveStrategyInfo> reg;
    return reg;
}

int DeriveEngine::register_strategy(const std::string& trait_name,
                                     std::vector<std::string> method_names,
                                     DeriveGeneratorFn generator) {
    registry()[trait_name] = {trait_name, std::move(method_names), std::move(generator)};
    return 0;
}

bool DeriveEngine::is_derivable(const std::string& trait_name) {
    return registry().count(trait_name) > 0;
}

const DeriveStrategyInfo* DeriveEngine::get_strategy(const std::string& trait_name) {
    auto it = registry().find(trait_name);
    return (it != registry().end()) ? &it->second : nullptr;
}

std::vector<std::string> DeriveEngine::all_derivable_traits() {
    std::vector<std::string> result;
    for (auto& [name, _] : registry()) result.push_back(name);
    return result;
}

// ===== Strategy: Show =====

static std::string derive_show(const DeriveAdtInfo& adt) {
    std::ostringstream os;
    os << "show x = case x of\n";

    for (auto& ctor : adt.constructors) {
        if (ctor.arity == 0) {
            os << "    " << ctor.name << " -> \"" << ctor.name << "\"\n";
        } else {
            os << "    " << ctor.name;
            for (int i = 0; i < ctor.arity; i++)
                os << " _f" << i;
            os << " -> \"" << ctor.name << "(\"";
            for (int i = 0; i < ctor.arity; i++) {
                if (i > 0) os << " ++ \", \"";
                os << " ++ show _f" << i;
            }
            os << " ++ \")\"\n";
        }
    }

    os << "end\n";
    return os.str();
}

static auto _reg_show = DeriveEngine::register_strategy("Show", {"show"}, derive_show);

// ===== Strategy: Eq =====

static std::string derive_eq(const DeriveAdtInfo& adt) {
    std::ostringstream os;
    os << "eq _a _b = case _a of\n";

    for (auto& ctor : adt.constructors) {
        os << "    " << ctor.name;
        for (int i = 0; i < ctor.arity; i++)
            os << " _x" << i;

        os << " -> case _b of\n";
        os << "        " << ctor.name;
        for (int i = 0; i < ctor.arity; i++)
            os << " _y" << i;
        os << " -> ";

        if (ctor.arity == 0) {
            os << "true";
        } else {
            for (int i = 0; i < ctor.arity; i++) {
                if (i > 0) os << " && ";
                os << "eq _x" << i << " _y" << i;
            }
        }
        os << "\n";
        os << "        _ -> false\n";
        os << "    end\n";
    }

    os << "end\n";
    return os.str();
}

static auto _reg_eq = DeriveEngine::register_strategy("Eq", {"eq"}, derive_eq);

// ===== Strategy: Ord =====

static std::string derive_ord(const DeriveAdtInfo& adt) {
    std::ostringstream os;

    if (adt.constructors.size() == 1) {
        auto& ctor = adt.constructors[0];
        os << "compare _a _b = case _a of\n";
        os << "    " << ctor.name;
        for (int i = 0; i < ctor.arity; i++) os << " _x" << i;
        os << " -> case _b of\n";
        os << "        " << ctor.name;
        for (int i = 0; i < ctor.arity; i++) os << " _y" << i;
        os << " -> ";
        if (ctor.arity == 0) {
            os << "0";
        } else {
            for (int i = 0; i < ctor.arity; i++) {
                if (i == ctor.arity - 1) {
                    os << "compare _x" << i << " _y" << i;
                } else {
                    os << "let _c" << i << " = compare _x" << i << " _y" << i
                       << " in if _c" << i << " != 0 then _c" << i << " else ";
                }
            }
        }
        os << "\n    end\nend\n";
        return os.str();
    }

    os << "compare _a _b = case _a of\n";
    for (size_t ci = 0; ci < adt.constructors.size(); ci++) {
        auto& ctor = adt.constructors[ci];
        os << "    " << ctor.name;
        for (int i = 0; i < ctor.arity; i++) os << " _x" << i;
        os << " -> case _b of\n";

        os << "        " << ctor.name;
        for (int i = 0; i < ctor.arity; i++) os << " _y" << i;
        os << " -> ";
        if (ctor.arity == 0) {
            os << "0";
        } else {
            for (int i = 0; i < ctor.arity; i++) {
                if (i == ctor.arity - 1) {
                    os << "compare _x" << i << " _y" << i;
                } else {
                    os << "let _c" << i << " = compare _x" << i << " _y" << i
                       << " in if _c" << i << " != 0 then _c" << i << " else ";
                }
            }
        }
        os << "\n";

        // Different constructors: earlier declaration = smaller
        os << "        _ -> " << (int)ci << " - " << (int)adt.constructors.size() << "\n";
        os << "    end\n";
    }
    os << "end\n";

    return os.str();
}

static auto _reg_ord = DeriveEngine::register_strategy("Ord", {"compare"}, derive_ord);

// ===== Strategy: Hash =====

static std::string derive_hash(const DeriveAdtInfo& adt) {
    std::ostringstream os;
    os << "hash x = case x of\n";

    for (auto& ctor : adt.constructors) {
        os << "    " << ctor.name;
        for (int i = 0; i < ctor.arity; i++)
            os << " _f" << i;
        os << " -> ";

        if (ctor.arity == 0) {
            os << ctor.tag;
        } else {
            os << "let _h = " << ctor.tag;
            for (int i = 0; i < ctor.arity; i++) {
                os << ", _h = _h * 31 + hash _f" << i;
            }
            os << " in _h";
        }
        os << "\n";
    }

    os << "end\n";
    return os.str();
}

static auto _reg_hash = DeriveEngine::register_strategy("Hash", {"hash"}, derive_hash);

} // namespace yona::compiler::codegen
