#include <sstream>
#include <doctest/doctest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <cstdlib>
#include <filesystem>
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

TEST_SUITE("Yona Stdlib") {

static RuntimeObjectPtr eval(const string& src) {
    parser::Parser parser;
    auto interp = make_unique<Interpreter>();
    stringstream code(src);
    auto parse_result = parser.parse_input(code);
    REQUIRE(parse_result.success);
    REQUIRE(parse_result.node != nullptr);
    auto result = interp->visit(parse_result.node.get()).value;
    return result;
}

// ===== Option Module =====

TEST_CASE("Option::some wraps a value") {
    auto r = eval("import some, unwrapOr from Std\\Option in unwrapOr 0 (some 42)");
    CHECK(r->type == Int);
    CHECK(r->get<int>() == 42);
}

TEST_CASE("Option::none returns default on unwrap") {
    auto r = eval("import none, unwrapOr from Std\\Option in unwrapOr 99 none");
    CHECK(r->type == Int);
    CHECK(r->get<int>() == 99);
}

TEST_CASE("Option::isSome and isNone") {
    auto r = eval("import some, none, isSome, isNone from Std\\Option in (isSome (some 1), isNone none)");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == true);
}

TEST_CASE("Option::map transforms Some, preserves None") {
    auto r = eval("import some, none, map, unwrapOr from Std\\Option in (unwrapOr 0 (map (\\x -> x + 1) (some 10)), unwrapOr 0 (map (\\x -> x + 1) none))");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 11);
    CHECK(tup[1]->get<int>() == 0);
}

// ===== Result Module =====

TEST_CASE("Result::ok and err") {
    auto r = eval("import ok, err, isOk, isErr from Std\\Result in (isOk (ok 1), isErr (err \"x\"))");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == true);
}

TEST_CASE("Result::unwrapOr") {
    auto r = eval("import ok, err, unwrapOr from Std\\Result in (unwrapOr 0 (ok 42), unwrapOr 0 (err \"fail\"))");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 42);
    CHECK(tup[1]->get<int>() == 0);
}

TEST_CASE("Result::map") {
    auto r = eval("import ok, map, unwrapOr from Std\\Result in unwrapOr 0 (map (\\x -> x * 3) (ok 7))");
    CHECK(r->type == Int);
    CHECK(r->get<int>() == 21);
}

// ===== Tuple Module (uses native) =====

TEST_CASE("Tuple::fst and snd") {
    auto r = eval("import fst, snd from Std\\Tuple in let t = (10, 20) in (fst t, snd t)");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 10);
    CHECK(tup[1]->get<int>() == 20);
}

TEST_CASE("Tuple::swap") {
    auto r = eval("import swap from Std\\Tuple in let t = (1, 2) in swap t");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 2);
    CHECK(tup[1]->get<int>() == 1);
}

// ===== Range Module =====

TEST_CASE("Range::length and contains") {
    auto r = eval("import range, contains, length from Std\\Range in (length (range 1 10), contains 5 (range 1 10), contains 15 (range 1 10))");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 10);
    CHECK(tup[1]->get<bool>() == true);
    CHECK(tup[2]->get<bool>() == false);
}

// ===== List Module (uses native) =====

TEST_CASE("List::map") {
    auto r = eval("import map from Std\\List in map (\\x -> x * 2) [1, 2, 3]");
    CHECK(r->type == RuntimeObjectType::Seq);
    auto& seq = r->get<shared_ptr<SeqValue>>()->fields;
    CHECK(seq.size() == 3);
    CHECK(seq[0]->get<int>() == 2);
    CHECK(seq[1]->get<int>() == 4);
    CHECK(seq[2]->get<int>() == 6);
}

TEST_CASE("List::filter") {
    auto r = eval("import filter from Std\\List in filter (\\x -> x > 2) [1, 2, 3, 4, 5]");
    CHECK(r->type == RuntimeObjectType::Seq);
    auto& seq = r->get<shared_ptr<SeqValue>>()->fields;
    CHECK(seq.size() == 3);
    CHECK(seq[0]->get<int>() == 3);
}

TEST_CASE("List::fold sum") {
    auto r = eval("import fold from Std\\List in fold (\\acc x -> acc + x) 0 [1, 2, 3, 4, 5]");
    CHECK(r->type == Int);
    CHECK(r->get<int>() == 15);
}

TEST_CASE("List::reverse") {
    auto r = eval("import reverse from Std\\List in reverse [1, 2, 3]");
    CHECK(r->type == RuntimeObjectType::Seq);
    auto& seq = r->get<shared_ptr<SeqValue>>()->fields;
    CHECK(seq.size() == 3);
    CHECK(seq[0]->get<int>() == 3);
    CHECK(seq[1]->get<int>() == 2);
    CHECK(seq[2]->get<int>() == 1);
}

TEST_CASE("List::any and all") {
    auto r = eval("import any, all from Std\\List in (any (\\x -> x > 3) [1,2,3,4,5], all (\\x -> x > 0) [1,2,3])");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == true);
}

TEST_CASE("List::contains and isEmpty") {
    auto r = eval("import contains, isEmpty from Std\\List in (contains 3 [1,2,3], isEmpty [], isEmpty [1])");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == true);
    CHECK(tup[2]->get<bool>() == false);
}

// ===== Test Module =====

TEST_CASE("Test::assertEqual pass") {
    auto r = eval("import assertEqual from Std\\Test in assertEqual 42 42");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->type == RuntimeObjectType::Symbol);
    CHECK(tup[0]->get<shared_ptr<SymbolValue>>()->name == "pass");
}

TEST_CASE("Test::assertEqual fail") {
    auto r = eval("import assertEqual from Std\\Test in assertEqual 1 2");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->type == RuntimeObjectType::Symbol);
    CHECK(tup[0]->get<shared_ptr<SymbolValue>>()->name == "fail");
}

TEST_CASE("Test::suite and run") {
    auto r = eval(R"(
        import assertEqual, assertTrue, suite, run from Std\Test in
        run (suite "basic" [assertEqual 42 42, assertTrue true, assertEqual 1 2])
    )");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    // (:results, "basic", 2, 1)
    CHECK(tup[0]->type == RuntimeObjectType::Symbol);
    CHECK(tup[0]->get<shared_ptr<SymbolValue>>()->name == "results");
    CHECK(tup[2]->get<int>() == 2); // passed
    CHECK(tup[3]->get<int>() == 1); // failed
}

TEST_CASE("Test::assertContains") {
    auto r = eval("import assertContains from Std\\Test in (assertContains 3 [1,2,3], assertContains 5 [1,2,3])");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    auto& pass_result = tup[0]->get<shared_ptr<TupleValue>>()->fields;
    CHECK(pass_result[0]->get<shared_ptr<SymbolValue>>()->name == "pass");
    auto& fail_result = tup[1]->get<shared_ptr<TupleValue>>()->fields;
    CHECK(fail_result[0]->get<shared_ptr<SymbolValue>>()->name == "fail");
}

// ===== List Module (Yona source) =====

TEST_CASE("List::lookup") {
    auto r = eval("import lookup from Std\\List in (lookup 0 [10,20,30], lookup 2 [10,20,30])");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 10);
    CHECK(tup[1]->get<int>() == 30);
}

TEST_CASE("List::splitAt") {
    auto r = eval("import splitAt from Std\\List in splitAt 2 [1,2,3,4,5]");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    auto& before = tup[0]->get<shared_ptr<SeqValue>>()->fields;
    auto& after = tup[1]->get<shared_ptr<SeqValue>>()->fields;
    CHECK(before.size() == 2);
    CHECK(before[0]->get<int>() == 1);
    CHECK(before[1]->get<int>() == 2);
    CHECK(after.size() == 3);
    CHECK(after[0]->get<int>() == 3);
}

TEST_CASE("List::foldr") {
    auto r = eval("import foldr from Std\\List in foldr (\\x acc -> x :: acc) [] [1,2,3]");
    CHECK(r->type == RuntimeObjectType::Seq);
    auto& seq = r->get<shared_ptr<SeqValue>>()->fields;
    CHECK(seq.size() == 3);
    CHECK(seq[0]->get<int>() == 1);
    CHECK(seq[1]->get<int>() == 2);
    CHECK(seq[2]->get<int>() == 3);
}

TEST_CASE("List::flatten") {
    auto r = eval("import flatten from Std\\List in flatten [[1,2], [3,4], [5]]");
    CHECK(r->type == RuntimeObjectType::Seq);
    auto& seq = r->get<shared_ptr<SeqValue>>()->fields;
    CHECK(seq.size() == 5);
    CHECK(seq[0]->get<int>() == 1);
    CHECK(seq[4]->get<int>() == 5);
}

TEST_CASE("List::zip") {
    auto r = eval("import zip from Std\\List in zip [1,2,3] [10,20,30]");
    CHECK(r->type == RuntimeObjectType::Seq);
    auto& seq = r->get<shared_ptr<SeqValue>>()->fields;
    CHECK(seq.size() == 3);
    auto& first = seq[0]->get<shared_ptr<TupleValue>>()->fields;
    CHECK(first[0]->get<int>() == 1);
    CHECK(first[1]->get<int>() == 10);
}

TEST_CASE("List::drop and take edge cases") {
    auto r = eval("import take, drop from Std\\List in (take 0 [1,2,3], take 10 [1,2], drop 0 [1,2,3], drop 10 [1,2])");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<shared_ptr<SeqValue>>()->fields.size() == 0); // take 0
    CHECK(tup[1]->get<shared_ptr<SeqValue>>()->fields.size() == 2); // take 10 of [1,2]
    CHECK(tup[2]->get<shared_ptr<SeqValue>>()->fields.size() == 3); // drop 0
    CHECK(tup[3]->get<shared_ptr<SeqValue>>()->fields.size() == 0); // drop 10
}

// ===== Tuple Module (Yona source) =====

TEST_CASE("Tuple::mapBoth") {
    auto r = eval("import mapBoth from Std\\Tuple in let t = (3, 7) in mapBoth (\\x -> x * 2) (\\x -> x + 10) t");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 6);
    CHECK(tup[1]->get<int>() == 17);
}

TEST_CASE("Tuple::zip and unzip") {
    auto r = eval("import zip, unzip from Std\\Tuple in unzip (zip [1,2,3] [10,20,30])");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    auto& firsts = tup[0]->get<shared_ptr<SeqValue>>()->fields;
    auto& seconds = tup[1]->get<shared_ptr<SeqValue>>()->fields;
    CHECK(firsts.size() == 3);
    CHECK(firsts[0]->get<int>() == 1);
    CHECK(seconds[2]->get<int>() == 30);
}

// ===== Range Module (Yona source) =====

TEST_CASE("Range::toList") {
    auto r = eval("import range, toList from Std\\Range in let rng = range 1 5 in toList rng");
    CHECK(r->type == RuntimeObjectType::Seq);
    auto& seq = r->get<shared_ptr<SeqValue>>()->fields;
    CHECK(seq.size() == 5);
    CHECK(seq[0]->get<int>() == 1);
    CHECK(seq[4]->get<int>() == 5);
}

TEST_CASE("Range::take and drop") {
    auto r = eval(R"(
        import range, toList, take, drop from Std\Range in
        let rng = range 1 10 in
        let taken = take 3 rng in
        let dropped = drop 7 rng in
        (toList taken, toList dropped)
    )");
    CHECK(r->type == Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    auto& taken = tup[0]->get<shared_ptr<SeqValue>>()->fields;
    auto& dropped = tup[1]->get<shared_ptr<SeqValue>>()->fields;
    CHECK(taken.size() == 3);
    CHECK(taken[0]->get<int>() == 1);
    CHECK(taken[2]->get<int>() == 3);
    CHECK(dropped.size() == 3);
    CHECK(dropped[0]->get<int>() == 8);
}

} // Yona Stdlib
