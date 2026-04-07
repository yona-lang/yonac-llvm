/// Unit tests for the type checker:
/// TypeArena, UnionFind, Unifier, TypeEnv, builtins.

#include <doctest/doctest.h>
#include "typechecker/InferType.h"
#include "typechecker/TypeArena.h"
#include "typechecker/UnionFind.h"
#include "typechecker/Unification.h"
#include "typechecker/TypeEnv.h"
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

// ===== TypeEnv Tests =====

TEST_CASE("TypeEnv: bind and lookup") {
    TypeArena arena;
    auto env = std::make_shared<TypeEnv>();
    auto* int_t = arena.make_con(TyCon::Int);
    env->bind("x", int_t);
    auto result = env->lookup("x");
    REQUIRE(result.has_value());
    CHECK(result->body->tag == MonoType::Con);
    CHECK(result->body->con == TyCon::Int);
}

TEST_CASE("TypeEnv: lookup not found returns nullopt") {
    auto env = std::make_shared<TypeEnv>();
    CHECK(!env->lookup("nonexistent").has_value());
}

TEST_CASE("TypeEnv: child scope inherits parent bindings") {
    TypeArena arena;
    auto parent = std::make_shared<TypeEnv>();
    parent->bind("x", arena.make_con(TyCon::Int));
    auto child = parent->child();
    auto result = child->lookup("x");
    REQUIRE(result.has_value());
    CHECK(result->body->con == TyCon::Int);
}

TEST_CASE("TypeEnv: child scope shadows parent") {
    TypeArena arena;
    auto parent = std::make_shared<TypeEnv>();
    parent->bind("x", arena.make_con(TyCon::Int));
    auto child = parent->child();
    child->bind("x", arena.make_con(TyCon::String));
    // Child sees String
    CHECK(child->lookup("x")->body->con == TyCon::String);
    // Parent still sees Int
    CHECK(parent->lookup("x")->body->con == TyCon::Int);
}

TEST_CASE("TypeEnv: polymorphic scheme binding") {
    TypeArena arena;
    auto env = std::make_shared<TypeEnv>();
    // forall a. a -> a
    auto* a = arena.fresh_var(1);
    auto* id_type = arena.make_arrow(a, a);
    env->bind_scheme("id", TypeScheme({a->var_id}, id_type));
    auto result = env->lookup("id");
    REQUIRE(result.has_value());
    CHECK(result->quantified_vars.size() == 1);
    CHECK(result->body->tag == MonoType::Arrow);
}

TEST_CASE("register_builtins: arithmetic operators have correct types") {
    TypeArena arena;
    auto env = std::make_shared<TypeEnv>();
    register_builtins(*env, arena);

    // + : Int -> Int -> Int
    auto plus = env->lookup("+");
    REQUIRE(plus.has_value());
    CHECK(plus->body->tag == MonoType::Arrow);
    CHECK(plus->body->param_type->con == TyCon::Int);
    CHECK(plus->body->return_type->tag == MonoType::Arrow);
    CHECK(plus->body->return_type->return_type->con == TyCon::Int);
}

TEST_CASE("register_builtins: comparison operators return Bool") {
    TypeArena arena;
    auto env = std::make_shared<TypeEnv>();
    register_builtins(*env, arena);

    auto eq = env->lookup("==");
    REQUIRE(eq.has_value());
    CHECK(eq->body->return_type->return_type->con == TyCon::Bool);
}

TEST_CASE("register_builtins: cons is polymorphic") {
    TypeArena arena;
    auto env = std::make_shared<TypeEnv>();
    register_builtins(*env, arena);

    auto cons = env->lookup("::");
    REQUIRE(cons.has_value());
    CHECK(cons->quantified_vars.size() == 1);
    // :: : a -> Seq a -> Seq a
    CHECK(cons->body->tag == MonoType::Arrow);
}

TEST_CASE("register_builtins: pipe is polymorphic") {
    TypeArena arena;
    auto env = std::make_shared<TypeEnv>();
    register_builtins(*env, arena);

    auto pipe = env->lookup("|>");
    REQUIRE(pipe.has_value());
    CHECK(pipe->quantified_vars.size() == 2);
}

TEST_CASE("register_builtins: string concat") {
    TypeArena arena;
    auto env = std::make_shared<TypeEnv>();
    register_builtins(*env, arena);

    auto concat = env->lookup("++");
    REQUIRE(concat.has_value());
    CHECK(concat->body->param_type->con == TyCon::String);
}

TEST_CASE("register_builtins: type names available") {
    TypeArena arena;
    auto env = std::make_shared<TypeEnv>();
    register_builtins(*env, arena);

    CHECK(env->lookup("Int").has_value());
    CHECK(env->lookup("Float").has_value());
    CHECK(env->lookup("Bool").has_value());
    CHECK(env->lookup("String").has_value());
    CHECK(env->lookup("true").has_value());
    CHECK(env->lookup("false").has_value());
}

// ===== Core Inference Tests =====
// These tests parse Yona code and run the type checker.

} // close TypeChecker suite temporarily

#include "typechecker/TypeChecker.h"
#include "Parser.h"
#include <sstream>

static std::string check_expr_str(const std::string& source) {
    yona::parser::Parser parser;
    std::istringstream stream(source);
    auto result = parser.parse_input(stream);
    if (!result.node) return "PARSE_ERROR";

    yona::compiler::DiagnosticEngine diag;
    yona::compiler::typechecker::TypeChecker checker(diag);
    auto* t = checker.check(result.node.get());
    if (!t) return "?";
    return yona::compiler::typechecker::pretty_print(checker.zonk(t));
}

TEST_SUITE("TypeChecker") { // reopen suite

TEST_CASE("Inference: integer literal") {
    CHECK(check_expr_str("42") == "Int");
}

TEST_CASE("Inference: float literal") {
    CHECK(check_expr_str("3.14") == "Float");
}

TEST_CASE("Inference: string literal") {
    CHECK(check_expr_str("\"hello\"") == "String");
}

TEST_CASE("Inference: boolean literals") {
    CHECK(check_expr_str("true") == "Bool");
    CHECK(check_expr_str("false") == "Bool");
}

TEST_CASE("Inference: arithmetic produces Int") {
    CHECK(check_expr_str("1 + 2") == "Int");
    CHECK(check_expr_str("10 - 3") == "Int");
    CHECK(check_expr_str("4 * 5") == "Int");
}

TEST_CASE("Inference: comparison produces Bool") {
    CHECK(check_expr_str("1 < 2") == "Bool");
    CHECK(check_expr_str("3 == 4") == "Bool");
}

TEST_CASE("Inference: if expression unifies branches") {
    CHECK(check_expr_str("if true then 1 else 2") == "Int");
    CHECK(check_expr_str("if true then \"a\" else \"b\"") == "String");
}

TEST_CASE("Inference: let binding") {
    CHECK(check_expr_str("let x = 42 in x") == "Int");
    CHECK(check_expr_str("let x = 1 in x + 2") == "Int");
}

TEST_CASE("Inference: let with function") {
    CHECK(check_expr_str("let f x = x + 1 in f 5") == "Int");
}

TEST_CASE("Inference: tuple") {
    CHECK(check_expr_str("(1, \"hello\")") == "(Int, String)");
    CHECK(check_expr_str("(1, 2, 3)") == "(Int, Int, Int)");
}

TEST_CASE("Inference: sequence") {
    CHECK(check_expr_str("[1, 2, 3]") == "Seq Int");
}

TEST_CASE("Inference: string concat") {
    CHECK(check_expr_str("\"a\" ++ \"b\"") == "String");
}

TEST_CASE("Inference: nested let") {
    CHECK(check_expr_str("let x = 1 in let y = x + 1 in y") == "Int");
}

TEST_CASE("Inference: lambda") {
    CHECK(check_expr_str("let f = \\x -> x + 1 in f 5") == "Int");
}

// ===== Case Expression + Pattern Inference =====

TEST_CASE("Inference: case with integer patterns") {
    CHECK(check_expr_str("case 42 of 0 -> \"zero\"; _ -> \"other\" end") == "String");
}

TEST_CASE("Inference: case with identifier binding") {
    CHECK(check_expr_str("case 42 of x -> x + 1 end") == "Int");
}

TEST_CASE("Inference: case with head-tail pattern") {
    CHECK(check_expr_str("case [1, 2, 3] of [h|t] -> h end") == "Int");
}

TEST_CASE("Inference: case with empty seq pattern") {
    CHECK(check_expr_str("case [1, 2] of [] -> 0; [h|t] -> h end") == "Int");
}

TEST_CASE("Inference: case with tuple pattern") {
    CHECK(check_expr_str("case (1, \"hello\") of (a, b) -> a end") == "Int");
}

TEST_CASE("Inference: case branches must unify") {
    // Both branches return Int
    CHECK(check_expr_str("case 1 of 0 -> 10; _ -> 20 end") == "Int");
}

TEST_CASE("Inference: cons operator") {
    CHECK(check_expr_str("1 :: [2, 3]") == "Seq Int");
}

TEST_CASE("Inference: recursive sum via case") {
    CHECK(check_expr_str(
        "let foldl fn acc seq = case seq of [] -> acc; [h|t] -> foldl fn (fn acc h) t end in "
        "foldl (\\a b -> a + b) 0 [1, 2, 3]"
    ) == "Int");
}

} // TypeChecker
