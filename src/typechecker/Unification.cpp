/// Type unification for Hindley-Milner inference.
///
/// Core algorithm:
///  1. Resolve both types via union-find
///  2. If identical → success
///  3. If one is a variable → occurs check → bind
///  4. If both constructors → match constructor, unify args
///  5. Error otherwise

#include "typechecker/Unification.h"
#include <sstream>

namespace yona::compiler::typechecker {

Unifier::Unifier(TypeArena& arena, UnionFind& uf, DiagnosticEngine& diag)
    : arena_(arena), uf_(uf), diag_(diag) {}

MonoTypePtr Unifier::resolve(MonoTypePtr type) {
    if (!type) return nullptr;
    if (type->tag == MonoType::Var) {
        auto bound = uf_.find(type->var_id);
        if (bound) return resolve(bound);
    }
    return type;
}

bool Unifier::unify(MonoTypePtr a, MonoTypePtr b, const SourceLocation& loc,
                     const std::string& context) {
    return unify_inner(resolve(a), resolve(b), loc, context);
}

bool Unifier::unify_inner(MonoTypePtr a, MonoTypePtr b, const SourceLocation& loc,
                           const std::string& context) {
    if (a == b) return true;
    if (!a || !b) return false;

    // Var on left: bind
    if (a->tag == MonoType::Var) {
        if (b->tag == MonoType::Var && a->var_id == b->var_id) return true;
        if (occurs_in(a->var_id, b)) {
            diag_.error(loc, "infinite type: cannot construct " +
                        pretty_print(a) + " ~ " + pretty_print(b) +
                        (context.empty() ? "" : " " + context));
            return false;
        }
        adjust_levels(b, uf_.level(a->var_id));
        uf_.bind(a->var_id, b);
        return true;
    }

    // Var on right: bind
    if (b->tag == MonoType::Var) {
        if (occurs_in(b->var_id, a)) {
            diag_.error(loc, "infinite type: cannot construct " +
                        pretty_print(b) + " ~ " + pretty_print(a) +
                        (context.empty() ? "" : " " + context));
            return false;
        }
        adjust_levels(a, uf_.level(b->var_id));
        uf_.bind(b->var_id, a);
        return true;
    }

    // Both concrete: must match structurally
    if (a->tag != b->tag) {
        diag_.error(loc, "type mismatch: expected " + pretty_print(a) +
                    " but found " + pretty_print(b) +
                    (context.empty() ? "" : " " + context));
        return false;
    }

    switch (a->tag) {
        case MonoType::Con:
            if (a->con != b->con) {
                diag_.error(loc, "type mismatch: expected " + pretty_print(a) +
                            " but found " + pretty_print(b) +
                            (context.empty() ? "" : " " + context));
                return false;
            }
            return true;

        case MonoType::Arrow:
            return unify(a->param_type, b->param_type, loc, context) &&
                   unify(a->return_type, b->return_type, loc, context);

        case MonoType::App:
            if (a->type_name != b->type_name || a->args.size() != b->args.size()) {
                diag_.error(loc, "type mismatch: " + pretty_print(a) +
                            " vs " + pretty_print(b) +
                            (context.empty() ? "" : " " + context));
                return false;
            }
            for (size_t i = 0; i < a->args.size(); i++) {
                if (!unify(a->args[i], b->args[i], loc, context)) return false;
            }
            return true;

        case MonoType::MTuple:
            if (a->elements.size() != b->elements.size()) {
                diag_.error(loc, "tuple size mismatch: " +
                            std::to_string(a->elements.size()) + " vs " +
                            std::to_string(b->elements.size()) +
                            (context.empty() ? "" : " " + context));
                return false;
            }
            for (size_t i = 0; i < a->elements.size(); i++) {
                if (!unify(a->elements[i], b->elements[i], loc, context)) return false;
            }
            return true;

        default:
            return false;
    }
}

bool Unifier::occurs_in(TypeId var_id, MonoTypePtr type) {
    type = resolve(type);
    if (!type) return false;
    if (type->tag == MonoType::Var) return type->var_id == var_id;
    if (type->tag == MonoType::Arrow)
        return occurs_in(var_id, type->param_type) || occurs_in(var_id, type->return_type);
    if (type->tag == MonoType::App) {
        for (auto* a : type->args) if (occurs_in(var_id, a)) return true;
        return false;
    }
    if (type->tag == MonoType::MTuple) {
        for (auto* e : type->elements) if (occurs_in(var_id, e)) return true;
        return false;
    }
    return false;
}

void Unifier::adjust_levels(MonoTypePtr type, int level) {
    type = resolve(type);
    if (!type) return;
    if (type->tag == MonoType::Var) {
        if (uf_.level(type->var_id) > level)
            uf_.set_level(type->var_id, level);
        return;
    }
    if (type->tag == MonoType::Arrow) {
        adjust_levels(type->param_type, level);
        adjust_levels(type->return_type, level);
    }
    if (type->tag == MonoType::App)
        for (auto* a : type->args) adjust_levels(a, level);
    if (type->tag == MonoType::MTuple)
        for (auto* e : type->elements) adjust_levels(e, level);
}

// Pretty-print a type for error messages
std::string pretty_print(MonoTypePtr type) {
    if (!type) return "?";
    switch (type->tag) {
        case MonoType::Var: return "t" + std::to_string(type->var_id);
        case MonoType::Con: {
            switch (type->con) {
                case TyCon::Int: return "Int";
                case TyCon::Float: return "Float";
                case TyCon::Bool: return "Bool";
                case TyCon::String: return "String";
                case TyCon::Char: return "Char";
                case TyCon::Byte: return "Byte";
                case TyCon::Symbol: return "Symbol";
                case TyCon::Unit: return "()";
                case TyCon::Seq: return "Seq";
                case TyCon::Set: return "Set";
                case TyCon::Dict: return "Dict";
                case TyCon::Tuple: return "Tuple";
                case TyCon::Function: return "Function";
                case TyCon::Promise: return "Promise";
                case TyCon::Bytes: return "Bytes";
            }
            return "?";
        }
        case MonoType::Arrow:
            return "(" + pretty_print(type->param_type) + " -> " +
                   pretty_print(type->return_type) + ")";
        case MonoType::App: {
            std::string s = type->type_name;
            for (auto* a : type->args) s += " " + pretty_print(a);
            return s;
        }
        case MonoType::MTuple: {
            std::string s = "(";
            for (size_t i = 0; i < type->elements.size(); i++) {
                if (i > 0) s += ", ";
                s += pretty_print(type->elements[i]);
            }
            return s + ")";
        }
        default: return "?";
    }
}

} // namespace yona::compiler::typechecker
