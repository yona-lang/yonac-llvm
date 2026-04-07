/// Unit tests for the type checker foundation:
/// TypeArena, UnionFind, Unifier.

#include <doctest/doctest.h>
#include "typechecker/InferType.h"
#include "typechecker/TypeArena.h"
#include "typechecker/UnionFind.h"
#include "typechecker/Unification.h"
#include "Diagnostic.h"

using namespace yona::compiler::typechecker;
using namespace yona::compiler;
using namespace yona;

TEST_SUITE("TypeChecker") {

TEST_CASE("TypeArena: fresh variables get unique IDs") {
    TypeArena arena;
    auto* v1 = arena.fresh_var(0);
    auto* v2 = arena.fresh_var(0);
    CHECK(v1->tag == MonoType::Var);
    CHECK(v2->tag == MonoType::Var);
    CHECK(v1->var_id != v2->var_id);
}

TEST_CASE("TypeArena: make_con returns correct types") {
    TypeArena arena;
    auto* int_t = arena.make_con(TyCon::Int);
    auto* str_t = arena.make_con(TyCon::String);
    CHECK(int_t->tag == MonoType::Con);
    CHECK(int_t->con == TyCon::Int);
    CHECK(str_t->con == TyCon::String);
}

TEST_CASE("TypeArena: make_arrow constructs function types") {
    TypeArena arena;
    auto* int_t = arena.make_con(TyCon::Int);
    auto* str_t = arena.make_con(TyCon::String);
    auto* fn = arena.make_arrow(int_t, str_t);
    CHECK(fn->tag == MonoType::Arrow);
    CHECK(fn->param_type == int_t);
    CHECK(fn->return_type == str_t);
}

TEST_CASE("UnionFind: unbound variable returns nullptr") {
    UnionFind uf;
    uf.add_var(0, 0);
    CHECK(uf.find(0) == nullptr);
    CHECK(!uf.is_bound(0));
}

TEST_CASE("UnionFind: bind and find") {
    TypeArena arena;
    UnionFind uf;
    uf.add_var(0, 0);
    auto* int_t = arena.make_con(TyCon::Int);
    uf.bind(0, int_t);
    CHECK(uf.find(0) == int_t);
    CHECK(uf.is_bound(0));
}

TEST_CASE("UnionFind: path compression") {
    TypeArena arena;
    UnionFind uf;
    auto* v0 = arena.fresh_var(0); uf.add_var(v0->var_id, 0);
    auto* v1 = arena.fresh_var(0); uf.add_var(v1->var_id, 0);
    auto* int_t = arena.make_con(TyCon::Int);
    // v0 -> v1 -> Int
    uf.bind(v0->var_id, v1);
    uf.bind(v1->var_id, int_t);
    CHECK(uf.find(v0->var_id) == int_t); // path compressed
}

TEST_CASE("Unifier: identical types unify") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* int_t = arena.make_con(TyCon::Int);
    CHECK(u.unify(int_t, int_t, SourceLocation::unknown()));
}

TEST_CASE("Unifier: different concrete types fail") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* int_t = arena.make_con(TyCon::Int);
    auto* str_t = arena.make_con(TyCon::String);
    CHECK(!u.unify(int_t, str_t, SourceLocation::unknown()));
}

TEST_CASE("Unifier: variable binds to concrete type") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* v = arena.fresh_var(0); uf.add_var(v->var_id, 0);
    auto* int_t = arena.make_con(TyCon::Int);
    CHECK(u.unify(v, int_t, SourceLocation::unknown()));
    CHECK(uf.find(v->var_id) == int_t);
}

TEST_CASE("Unifier: two variables unify") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* v1 = arena.fresh_var(0); uf.add_var(v1->var_id, 0);
    auto* v2 = arena.fresh_var(0); uf.add_var(v2->var_id, 0);
    CHECK(u.unify(v1, v2, SourceLocation::unknown()));
    // After binding v2 to something, v1 should resolve too
    auto* int_t = arena.make_con(TyCon::Int);
    uf.bind(v2->var_id, int_t);
    CHECK(u.resolve(v1) == int_t);
}

TEST_CASE("Unifier: arrow types unify structurally") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* int_t = arena.make_con(TyCon::Int);
    auto* a = arena.fresh_var(0); uf.add_var(a->var_id, 0);
    auto* fn1 = arena.make_arrow(int_t, a);
    auto* fn2 = arena.make_arrow(int_t, arena.make_con(TyCon::String));
    CHECK(u.unify(fn1, fn2, SourceLocation::unknown()));
    CHECK(u.resolve(a)->con == TyCon::String);
}

TEST_CASE("Unifier: occurs check prevents infinite types") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* a = arena.fresh_var(0); uf.add_var(a->var_id, 0);
    auto* seq_a = arena.make_app("Seq", {a});
    CHECK(!u.unify(a, seq_a, SourceLocation::unknown())); // a ~ Seq a -> infinite
}

TEST_CASE("Unifier: App types unify") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* a = arena.fresh_var(0); uf.add_var(a->var_id, 0);
    auto* opt_int = arena.make_app("Option", {arena.make_con(TyCon::Int)});
    auto* opt_a = arena.make_app("Option", {a});
    CHECK(u.unify(opt_int, opt_a, SourceLocation::unknown()));
    CHECK(u.resolve(a)->con == TyCon::Int);
}

TEST_CASE("Unifier: tuple types unify element-wise") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* a = arena.fresh_var(0); uf.add_var(a->var_id, 0);
    auto* t1 = arena.make_tuple({arena.make_con(TyCon::Int), a});
    auto* t2 = arena.make_tuple({arena.make_con(TyCon::Int), arena.make_con(TyCon::String)});
    CHECK(u.unify(t1, t2, SourceLocation::unknown()));
    CHECK(u.resolve(a)->con == TyCon::String);
}

TEST_CASE("Unifier: tuple size mismatch fails") {
    TypeArena arena;
    UnionFind uf;
    DiagnosticEngine diag;
    Unifier u(arena, uf, diag);
    auto* t1 = arena.make_tuple({arena.make_con(TyCon::Int)});
    auto* t2 = arena.make_tuple({arena.make_con(TyCon::Int), arena.make_con(TyCon::String)});
    CHECK(!u.unify(t1, t2, SourceLocation::unknown()));
}

TEST_CASE("pretty_print formats types correctly") {
    TypeArena arena;
    CHECK(pretty_print(arena.make_con(TyCon::Int)) == "Int");
    CHECK(pretty_print(arena.make_con(TyCon::String)) == "String");
    auto* fn = arena.make_arrow(arena.make_con(TyCon::Int), arena.make_con(TyCon::Bool));
    CHECK(pretty_print(fn) == "(Int -> Bool)");
    auto* opt = arena.make_app("Option", {arena.make_con(TyCon::Int)});
    CHECK(pretty_print(opt) == "Option Int");
    auto* tup = arena.make_tuple({arena.make_con(TyCon::Int), arena.make_con(TyCon::String)});
    CHECK(pretty_print(tup) == "(Int, String)");
}

} // TypeChecker
