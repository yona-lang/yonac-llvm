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

TEST_SUITE("Stdlib Comprehensive") {

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

// =============================================================================
// Math Module
// =============================================================================

TEST_CASE("Math::abs on negative integer") {
    auto r = eval("import abs from Std\\Math in abs (-5)");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 5);
}

TEST_CASE("Math::abs on positive integer") {
    auto r = eval("import abs from Std\\Math in abs 7");
    CHECK(r->get<int>() == 7);
}

TEST_CASE("Math::abs on zero") {
    auto r = eval("import abs from Std\\Math in abs 0");
    CHECK(r->get<int>() == 0);
}

TEST_CASE("Math::abs on negative float") {
    auto r = eval("import abs from Std\\Math in abs (-3.14)");
    CHECK(r->type == RuntimeObjectType::Float);
    CHECK(r->get<double>() == doctest::Approx(3.14));
}

TEST_CASE("Math::max and min with integers") {
    auto r = eval("import max, min from Std\\Math in (max 3 7, min 3 7)");
    CHECK(r->type == RuntimeObjectType::Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 7);
    CHECK(tup[1]->get<int>() == 3);
}

TEST_CASE("Math::max and min with equal values") {
    auto r = eval("import max, min from Std\\Math in (max 5 5, min 5 5)");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 5);
    CHECK(tup[1]->get<int>() == 5);
}

TEST_CASE("Math::max with mixed int and float") {
    auto r = eval("import max from Std\\Math in max 3 7.5");
    CHECK(r->type == RuntimeObjectType::Float);
    CHECK(r->get<double>() == doctest::Approx(7.5));
}

TEST_CASE("Math::min with negative values") {
    auto r = eval("import min from Std\\Math in min (-10) (-3)");
    CHECK(r->get<int>() == -10);
}

TEST_CASE("Math::factorial") {
    auto r = eval("import factorial from Std\\Math in factorial 5");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 120);
}

TEST_CASE("Math::factorial of zero") {
    auto r = eval("import factorial from Std\\Math in factorial 0");
    CHECK(r->get<int>() == 1);
}

TEST_CASE("Math::factorial of one") {
    auto r = eval("import factorial from Std\\Math in factorial 1");
    CHECK(r->get<int>() == 1);
}

TEST_CASE("Math::sqrt") {
    auto r = eval("import sqrt from Std\\Math in sqrt 4.0");
    CHECK(r->type == RuntimeObjectType::Float);
    CHECK(r->get<double>() == doctest::Approx(2.0));
}

TEST_CASE("Math::sqrt of non-perfect square") {
    auto r = eval("import sqrt from Std\\Math in sqrt 2.0");
    CHECK(r->get<double>() == doctest::Approx(1.41421356));
}

TEST_CASE("Math::sin and cos") {
    auto r = eval("import sin, cos from Std\\Math in (sin 0.0, cos 0.0)");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<double>() == doctest::Approx(0.0));
    CHECK(tup[1]->get<double>() == doctest::Approx(1.0));
}

TEST_CASE("Math::tan") {
    auto r = eval("import tan from Std\\Math in tan 0.0");
    CHECK(r->get<double>() == doctest::Approx(0.0));
}

TEST_CASE("Math::pow") {
    auto r = eval("import pow from Std\\Math in pow 2.0 10.0");
    CHECK(r->type == RuntimeObjectType::Float);
    CHECK(r->get<double>() == doctest::Approx(1024.0));
}

TEST_CASE("Math::pow with zero exponent") {
    auto r = eval("import pow from Std\\Math in pow 5.0 0.0");
    CHECK(r->get<double>() == doctest::Approx(1.0));
}

TEST_CASE("Math::pi constant") {
    auto r = eval("import pi from Std\\Math in pi");
    CHECK(r->type == RuntimeObjectType::Float);
    CHECK(r->get<double>() == doctest::Approx(3.14159265358979));
}

TEST_CASE("Math::e constant") {
    auto r = eval("import e from Std\\Math in e");
    CHECK(r->type == RuntimeObjectType::Float);
    CHECK(r->get<double>() == doctest::Approx(2.71828182845905));
}

TEST_CASE("Math::ceil") {
    auto r = eval("import ceil from Std\\Math in ceil 2.3");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 3);
}

TEST_CASE("Math::ceil on negative") {
    auto r = eval("import ceil from Std\\Math in ceil (-2.7)");
    CHECK(r->get<int>() == -2);
}

TEST_CASE("Math::floor") {
    auto r = eval("import floor from Std\\Math in floor 2.7");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Math::floor on negative") {
    auto r = eval("import floor from Std\\Math in floor (-2.3)");
    CHECK(r->get<int>() == -3);
}

TEST_CASE("Math::round") {
    auto r = eval("import round from Std\\Math in round 2.5");
    CHECK(r->type == RuntimeObjectType::Int);
    // C++ std::round rounds half away from zero
    CHECK(r->get<int>() == 3);
}

TEST_CASE("Math::round down") {
    auto r = eval("import round from Std\\Math in round 2.4");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Math::exp") {
    auto r = eval("import exp from Std\\Math in exp 0.0");
    CHECK(r->get<double>() == doctest::Approx(1.0));
}

TEST_CASE("Math::log") {
    auto r = eval("import log from Std\\Math in log 1.0");
    CHECK(r->get<double>() == doctest::Approx(0.0));
}

TEST_CASE("Math::log10") {
    auto r = eval("import log10 from Std\\Math in log10 100.0");
    CHECK(r->get<double>() == doctest::Approx(2.0));
}

TEST_CASE("Math::asin and acos") {
    auto r = eval("import asin, acos from Std\\Math in (asin 0.0, acos 1.0)");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<double>() == doctest::Approx(0.0));
    CHECK(tup[1]->get<double>() == doctest::Approx(0.0));
}

TEST_CASE("Math::atan") {
    auto r = eval("import atan from Std\\Math in atan 0.0");
    CHECK(r->get<double>() == doctest::Approx(0.0));
}

TEST_CASE("Math::atan2") {
    auto r = eval("import atan2 from Std\\Math in atan2 0.0 1.0");
    CHECK(r->get<double>() == doctest::Approx(0.0));
}

// =============================================================================
// String Module
// =============================================================================

TEST_CASE("String::length on empty string") {
    auto r = eval(R"(import length from Std\String in length "")");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 0);
}

TEST_CASE("String::toUpperCase and toLowerCase roundtrip") {
    auto r = eval(R"(import toUpperCase, toLowerCase from Std\String in toLowerCase (toUpperCase "Hello World"))");
    CHECK(r->get<string>() == "hello world");
}

TEST_CASE("String::trim with tabs and newlines") {
    auto r = eval(R"(import trim from Std\String in trim "\t hello \n")");
    CHECK(r->get<string>() == "hello");
}

TEST_CASE("String::trim on empty string") {
    auto r = eval(R"(import trim from Std\String in trim "   ")");
    CHECK(r->get<string>() == "");
}

TEST_CASE("String::split and join roundtrip") {
    auto r = eval(R"(
        import split, join from Std\String in
        join "-" (split "," "a,b,c")
    )");
    CHECK(r->type == RuntimeObjectType::String);
    CHECK(r->get<string>() == "a-b-c");
}

TEST_CASE("String::split with no delimiter found") {
    auto r = eval(R"(
        import split from Std\String in
        import length from Std\List in
        length (split "," "no commas here")
    )");
    CHECK(r->get<int>() == 1);
}

TEST_CASE("String::contains negative case") {
    auto r = eval(R"(import contains from Std\String in contains "xyz" "hello")");
    CHECK(r->type == RuntimeObjectType::Bool);
    CHECK(r->get<bool>() == false);
}

TEST_CASE("String::indexOf not found returns -1") {
    auto r = eval(R"(import indexOf from Std\String in indexOf "xyz" "hello")");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == -1);
}

TEST_CASE("String::replace with no match") {
    auto r = eval(R"(import replace from Std\String in replace "xyz" "abc" "hello world")");
    CHECK(r->get<string>() == "hello world");
}

TEST_CASE("String::replace all occurrences") {
    auto r = eval(R"(import replace from Std\String in replace "o" "0" "hello world of code")");
    CHECK(r->get<string>() == "hell0 w0rld 0f c0de");
}

TEST_CASE("String::startsWith negative") {
    auto r = eval(R"(import startsWith from Std\String in startsWith "xyz" "hello")");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("String::endsWith negative") {
    auto r = eval(R"(import endsWith from Std\String in endsWith "xyz" "hello")");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("String::endsWith with suffix longer than string") {
    auto r = eval(R"(import endsWith from Std\String in endsWith "toolongprefix" "hi")");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("String::chars produces individual characters") {
    auto r = eval(R"(
        import chars from Std\String in
        import lookup from Std\List in
        lookup 1 (chars "abc")
    )");
    CHECK(r->get<string>() == "b");
}

TEST_CASE("String::chars on empty string") {
    auto r = eval(R"(
        import chars from Std\String in
        import length from Std\List in
        length (chars "")
    )");
    CHECK(r->get<int>() == 0);
}

TEST_CASE("String::substring extracts substring") {
    auto r = eval(R"(import substring from Std\String in substring "hello" 1 3)");
    CHECK(r->type == RuntimeObjectType::String);
    CHECK(r->get<string>() == "ell");
}

TEST_CASE("String::substring from start") {
    auto r = eval(R"(import substring from Std\String in substring "hello" 0 5)");
    CHECK(r->get<string>() == "hello");
}

TEST_CASE("String::toString on float") {
    auto r = eval(R"(import toString from Std\String in toString 3.14)");
    CHECK(r->type == RuntimeObjectType::String);
    // just verify it produces a string representation
    CHECK(r->get<string>().find("3.14") != string::npos);
}

TEST_CASE("String::toString on bool") {
    auto r = eval(R"(import toString from Std\String in toString true)");
    CHECK(r->type == RuntimeObjectType::String);
    CHECK(r->get<string>() == "true");
}

TEST_CASE("String::join with empty list") {
    auto r = eval(R"(import join from Std\String in join "," [])");
    CHECK(r->get<string>() == "");
}

TEST_CASE("String::join with single element") {
    auto r = eval(R"(import join from Std\String in join "," ["only"])");
    CHECK(r->get<string>() == "only");
}

// =============================================================================
// Types Module
// =============================================================================

TEST_CASE("Types::typeOf on float") {
    auto r = eval(R"(import typeOf from Std\Types in typeOf 3.14)");
    CHECK(r->type == RuntimeObjectType::Symbol);
    CHECK(r->get<shared_ptr<SymbolValue>>()->name == "float");
}

TEST_CASE("Types::typeOf on seq") {
    auto r = eval(R"(import typeOf from Std\Types in typeOf [1, 2, 3])");
    CHECK(r->get<shared_ptr<SymbolValue>>()->name == "seq");
}

TEST_CASE("Types::typeOf on tuple") {
    auto r = eval(R"(import typeOf from Std\Types in let t = (1, 2) in typeOf t)");
    CHECK(r->get<shared_ptr<SymbolValue>>()->name == "tuple");
}

TEST_CASE("Types::typeOf on dict") {
    auto r = eval(R"(import typeOf from Std\Types in typeOf {"a": 1})");
    CHECK(r->get<shared_ptr<SymbolValue>>()->name == "dict");
}

TEST_CASE("Types::typeOf on set") {
    auto r = eval(R"(import typeOf from Std\Types in typeOf {1, 2, 3})");
    CHECK(r->get<shared_ptr<SymbolValue>>()->name == "set");
}

TEST_CASE("Types::typeOf on function") {
    auto r = eval(R"(import typeOf from Std\Types in typeOf (\x -> x))");
    CHECK(r->get<shared_ptr<SymbolValue>>()->name == "function");
}

TEST_CASE("Types::isFloat positive and negative") {
    auto r = eval(R"(import isFloat from Std\Types in (isFloat 3.14, isFloat 42))");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == false);
}

TEST_CASE("Types::isBool positive and negative") {
    auto r = eval(R"(import isBool from Std\Types in (isBool true, isBool 42))");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == false);
}

TEST_CASE("Types::isSeq positive and negative") {
    auto r = eval(R"(import isSeq from Std\Types in (isSeq [1, 2], isSeq 42))");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == false);
}

TEST_CASE("Types::isTuple positive and negative") {
    auto r = eval(R"(import isTuple from Std\Types in let t = (1, 2) in (isTuple t, isTuple [1, 2]))");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == false);
}

TEST_CASE("Types::isFunction positive") {
    auto r = eval(R"(import isFunction from Std\Types in isFunction (\x -> x))");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Types::isFunction negative") {
    auto r = eval(R"(import isFunction from Std\Types in isFunction 42)");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("Types::isSet and isDict") {
    auto r = eval(R"(import isSet, isDict from Std\Types in (isSet {1, 2}, isDict {"a": 1}))");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == true);
}

TEST_CASE("Types::toInt from float truncates") {
    auto r = eval(R"(import toInt from Std\Types in toInt 3.99)");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 3);
}

TEST_CASE("Types::toInt from bool") {
    auto r = eval(R"(import toInt from Std\Types in (toInt true, toInt false))");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 1);
    CHECK(tup[1]->get<int>() == 0);
}

TEST_CASE("Types::toFloat from string") {
    auto r = eval(R"(import toFloat from Std\Types in toFloat "3.14")");
    CHECK(r->type == RuntimeObjectType::Float);
    CHECK(r->get<double>() == doctest::Approx(3.14));
}

TEST_CASE("Types::toFloat from bool") {
    auto r = eval(R"(import toFloat from Std\Types in (toFloat true, toFloat false))");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<double>() == doctest::Approx(1.0));
    CHECK(tup[1]->get<double>() == doctest::Approx(0.0));
}

TEST_CASE("Types::toStr on bool") {
    auto r = eval(R"(import toStr from Std\Types in toStr false)");
    CHECK(r->type == RuntimeObjectType::String);
    CHECK(r->get<string>() == "false");
}

TEST_CASE("Types::toStr on float") {
    auto r = eval(R"(import toStr from Std\Types in toStr 3.14)");
    CHECK(r->type == RuntimeObjectType::String);
    CHECK(r->get<string>().find("3.14") != string::npos);
}

// =============================================================================
// Set Module
// =============================================================================

TEST_CASE("Set::add and contains on literal set") {
    auto r = eval(R"(
        import add, contains from Std\Set in
        contains 4 (add 4 {1, 2, 3})
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Set::remove") {
    auto r = eval(R"(
        import remove, contains, size from Std\Set in
        let s = remove 2 {1, 2, 3} in
        (contains 2 s, size s)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == false);
    CHECK(tup[1]->get<int>() == 2);
}

TEST_CASE("Set::remove nonexistent element") {
    auto r = eval(R"(
        import remove, size from Std\Set in
        size (remove 99 {1, 2, 3})
    )");
    CHECK(r->get<int>() == 3);
}

TEST_CASE("Set::toList and fromList roundtrip preserves elements") {
    auto r = eval(R"(
        import fromList, toList, size from Std\Set in
        import length from Std\List in
        let s = fromList [3, 1, 2, 1, 2] in
        (size s, length (toList s))
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 3);
    CHECK(tup[1]->get<int>() == 3);
}

TEST_CASE("Set::union combines two sets") {
    auto r = eval(R"(
        import union, size from Std\Set in
        size (union {1, 2, 3} {3, 4, 5})
    )");
    CHECK(r->get<int>() == 5);
}

TEST_CASE("Set::union with identical sets") {
    auto r = eval(R"(
        import union, size from Std\Set in
        size (union {1, 2} {1, 2})
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Set::intersection") {
    auto r = eval(R"(
        import intersection, size from Std\Set in
        size (intersection {1, 2, 3, 4} {2, 4, 6})
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Set::intersection with no overlap") {
    auto r = eval(R"(
        import intersection, isEmpty from Std\Set in
        isEmpty (intersection {1, 2} {3, 4})
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Set::difference") {
    auto r = eval(R"(
        import difference, size from Std\Set in
        size (difference {1, 2, 3, 4} {2, 4})
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Set::difference with self yields empty") {
    auto r = eval(R"(
        import difference, isEmpty from Std\Set in
        isEmpty (difference {1, 2, 3} {1, 2, 3})
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Set::fold sum") {
    auto r = eval(R"(
        import fold from Std\Set in
        fold (\acc x -> acc + x) 0 {10, 20, 30}
    )");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 60);
}

TEST_CASE("Set::map with collapsing duplicates") {
    // Mapping all elements to same value should collapse to size 1
    auto r = eval(R"(
        import map, size from Std\Set in
        size (map (\x -> 1) {1, 2, 3})
    )");
    CHECK(r->get<int>() == 1);
}

TEST_CASE("Set::filter with no matches") {
    auto r = eval(R"(
        import filter, isEmpty from Std\Set in
        isEmpty (filter (\x -> x > 100) {1, 2, 3})
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Set::isEmpty on non-empty set") {
    auto r = eval(R"(import isEmpty from Std\Set in isEmpty {1})");
    CHECK(r->get<bool>() == false);
}

// =============================================================================
// Dict Module
// =============================================================================

TEST_CASE("Dict::put overwrites existing key") {
    auto r = eval(R"(
        import put, lookup from Std\Dict in
        lookup "a" (put "a" 99 {"a": 1, "b": 2})
    )");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 99);
}

TEST_CASE("Dict::get returns some on hit") {
    auto r = eval(R"(
        import get from Std\Dict in
        import unwrapOr from Std\Option in
        unwrapOr 0 (get "b" {"a": 1, "b": 42})
    )");
    CHECK(r->get<int>() == 42);
}

TEST_CASE("Dict::get returns none on miss") {
    auto r = eval(R"(
        import get from Std\Dict in
        import isNone from Std\Option in
        isNone (get "z" {"a": 1})
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Dict::remove reduces size") {
    auto r = eval(R"(
        import remove, size from Std\Dict in
        size (remove "a" {"a": 1, "b": 2, "c": 3})
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Dict::remove nonexistent key") {
    auto r = eval(R"(
        import remove, size from Std\Dict in
        size (remove "z" {"a": 1, "b": 2})
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Dict::containsKey negative case") {
    auto r = eval(R"(
        import containsKey from Std\Dict in
        containsKey "z" {"a": 1, "b": 2}
    )");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("Dict::values returns all values") {
    auto r = eval(R"(
        import values from Std\Dict in
        import length from Std\List in
        length (values {"a": 1, "b": 2, "c": 3})
    )");
    CHECK(r->get<int>() == 3);
}

TEST_CASE("Dict::size") {
    auto r = eval(R"(
        import size from Std\Dict in
        size {"x": 10, "y": 20}
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Dict::toList produces tuples") {
    auto r = eval(R"(
        import toList from Std\Dict in
        import length, lookup from Std\List in
        let pairs = toList {"a": 1} in
        let pair = lookup 0 pairs in
        pair
    )");
    CHECK(r->type == RuntimeObjectType::Tuple);
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<string>() == "a");
    CHECK(tup[1]->get<int>() == 1);
}

TEST_CASE("Dict::fromList builds dict") {
    auto r = eval(R"(
        import fromList, size, lookup from Std\Dict in
        let d = fromList [("a", 1), ("b", 2), ("c", 3)] in
        (size d, lookup "b" d)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 3);
    CHECK(tup[1]->get<int>() == 2);
}

TEST_CASE("Dict::merge overwrites with second dict values") {
    auto r = eval(R"(
        import merge, lookup from Std\Dict in
        lookup "a" (merge {"a": 1, "b": 2} {"a": 99, "c": 3})
    )");
    CHECK(r->get<int>() == 99);
}

TEST_CASE("Dict::map transforms key-value pairs") {
    auto r = eval(R"(
        import map, lookup from Std\Dict in
        lookup "a" (map (\k v -> (k, v * 10)) {"a": 1, "b": 2})
    )");
    CHECK(r->get<int>() == 10);
}

TEST_CASE("Dict::filter keeps matching entries") {
    auto r = eval(R"(
        import filter, size from Std\Dict in
        size (filter (\k v -> v > 1) {"a": 1, "b": 2, "c": 3})
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("Dict::fold accumulates over entries") {
    auto r = eval(R"(
        import fold from Std\Dict in
        import snd from Std\Tuple in
        fold (\acc kv -> acc + (snd kv)) 0 {"a": 10, "b": 20, "c": 30}
    )");
    CHECK(r->get<int>() == 60);
}

TEST_CASE("Dict::lookup returns unit on miss") {
    auto r = eval(R"(
        import lookup from Std\Dict in
        import typeOf from Std\Types in
        typeOf (lookup "missing" {"a": 1})
    )");
    CHECK(r->get<shared_ptr<SymbolValue>>()->name == "unit");
}

TEST_CASE("Dict::isEmpty on non-empty dict") {
    auto r = eval(R"(import isEmpty from Std\Dict in isEmpty {"a": 1})");
    CHECK(r->get<bool>() == false);
}

// =============================================================================
// Option Module - edge cases
// =============================================================================

TEST_CASE("Option::map on None returns None") {
    auto r = eval(R"(
        import none, map, isNone from Std\Option in
        isNone (map (\x -> x * 2) none)
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Option::unwrapOr on Some ignores default") {
    auto r = eval(R"(
        import some, unwrapOr from Std\Option in
        unwrapOr "default" (some "value")
    )");
    CHECK(r->get<string>() == "value");
}

TEST_CASE("Option::unwrapOr on None uses default") {
    auto r = eval(R"(
        import none, unwrapOr from Std\Option in
        unwrapOr "default" none
    )");
    CHECK(r->get<string>() == "default");
}

TEST_CASE("Option::isSome on None") {
    auto r = eval(R"(import none, isSome from Std\Option in isSome none)");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("Option::isNone on Some") {
    auto r = eval(R"(import some, isNone from Std\Option in isNone (some 42))");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("Option::some with string value") {
    auto r = eval(R"(
        import some, unwrapOr from Std\Option in
        unwrapOr "" (some "hello")
    )");
    CHECK(r->get<string>() == "hello");
}

TEST_CASE("Option::map chaining") {
    auto r = eval(R"(
        import some, map, unwrapOr from Std\Option in
        unwrapOr 0 (map (\x -> x + 1) (map (\x -> x * 2) (some 5)))
    )");
    CHECK(r->get<int>() == 11);
}

// =============================================================================
// Result Module - edge cases
// =============================================================================

TEST_CASE("Result::map on err passes through error") {
    auto r = eval(R"(
        import err, map, isErr from Std\Result in
        isErr (map (\x -> x * 2) (err "oops"))
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Result::mapErr on ok passes through value") {
    auto r = eval(R"(
        import ok, mapErr, unwrapOr from Std\Result in
        unwrapOr 0 (mapErr (\e -> "modified") (ok 42))
    )");
    CHECK(r->get<int>() == 42);
}

TEST_CASE("Result::mapErr transforms error") {
    auto r = eval(R"(
        import err, mapErr, isErr from Std\Result in
        isErr (mapErr (\e -> "Error: " ++ e) (err "oops"))
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Result::isOk on err") {
    auto r = eval(R"(import err, isOk from Std\Result in isOk (err "fail"))");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("Result::isErr on ok") {
    auto r = eval(R"(import ok, isErr from Std\Result in isErr (ok 42))");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("Result::unwrapOr on ok ignores default") {
    auto r = eval(R"(
        import ok, unwrapOr from Std\Result in
        unwrapOr "default" (ok "value")
    )");
    CHECK(r->get<string>() == "value");
}

TEST_CASE("Result::map chaining") {
    auto r = eval(R"(
        import ok, map, unwrapOr from Std\Result in
        unwrapOr 0 (map (\x -> x + 10) (map (\x -> x * 3) (ok 5)))
    )");
    CHECK(r->get<int>() == 25);
}

// =============================================================================
// System Module (safe tests only)
// =============================================================================

TEST_CASE("System::getCwd returns a non-empty string") {
    auto r = eval(R"(import getCwd from Std\System in getCwd)");
    CHECK(r->type == RuntimeObjectType::String);
    CHECK(r->get<string>().size() > 0);
}

TEST_CASE("System::getEnv on HOME returns some") {
    auto r = eval(R"(
        import getEnv from Std\System in
        import isSome from Std\Option in
        isSome (getEnv "HOME")
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("System::getEnv on nonexistent var returns none") {
    auto r = eval(R"(
        import getEnv from Std\System in
        import isNone from Std\Option in
        isNone (getEnv "YONA_NONEXISTENT_ENV_VAR_12345")
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("System::getArgs returns a sequence") {
    auto r = eval(R"(
        import getArgs from Std\System in
        import isSeq from Std\Types in
        isSeq getArgs
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("System::currentTimeMillis returns positive float") {
    auto r = eval(R"(import currentTimeMillis from Std\System in currentTimeMillis > 0.0)");
    CHECK(r->get<bool>() == true);
}

// =============================================================================
// Time Module
// =============================================================================

TEST_CASE("Time::now is positive") {
    auto r = eval(R"(import now from Std\Time in now > 0)");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Time::nowMillis is positive") {
    auto r = eval(R"(import nowMillis from Std\Time in nowMillis > 0)");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Time::now and nowMillis are both positive") {
    auto r = eval(R"(
        import now, nowMillis from Std\Time in
        let n = now in
        let ms = nowMillis in
        (n > 0, ms > 0)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == true);
}

TEST_CASE("Time::format produces a date string") {
    auto r = eval(R"(
        import format from Std\Time in
        import length from Std\String in
        length (format "%Y-%m-%d" 0)
    )");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 10); // "1970-01-01" is 10 chars
}

TEST_CASE("Time::format epoch is 1970") {
    auto r = eval(R"(
        import format from Std\Time in
        import startsWith from Std\String in
        startsWith "1970" (format "%Y-%m-%d" 0)
    )");
    CHECK(r->get<bool>() == true);
}

// =============================================================================
// Random Module
// =============================================================================

TEST_CASE("Random::int returns value in range") {
    auto r = eval(R"(
        import int from Std\Random in
        let v = int 1 100 in
        v >= 1 && v <= 100
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Random::int with same min and max") {
    auto r = eval(R"(import int from Std\Random in int 42 42)");
    CHECK(r->type == RuntimeObjectType::Int);
    CHECK(r->get<int>() == 42);
}

TEST_CASE("Random::float returns value in [0,1)") {
    auto r = eval(R"(
        import float from Std\Random in
        let v = float in
        v >= 0.0 && v < 1.0
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Random::choice picks from list") {
    auto r = eval(R"(
        import choice from Std\Random in
        import contains from Std\List in
        let v = choice [10, 20, 30] in
        contains v [10, 20, 30]
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Random::choice on single element list") {
    auto r = eval(R"(import choice from Std\Random in choice [42])");
    CHECK(r->get<int>() == 42);
}

TEST_CASE("Random::shuffle preserves elements") {
    auto r = eval(R"(
        import shuffle from Std\Random in
        import fold from Std\List in
        fold (\acc x -> acc + x) 0 (shuffle [1, 2, 3, 4, 5])
    )");
    CHECK(r->get<int>() == 15);
}

TEST_CASE("Random::shuffle preserves length") {
    auto r = eval(R"(
        import shuffle from Std\Random in
        import length from Std\List in
        length (shuffle [1, 2, 3, 4, 5])
    )");
    CHECK(r->get<int>() == 5);
}

// =============================================================================
// File Module (path utilities only - no side effects)
// =============================================================================

TEST_CASE("File::basename extracts filename") {
    auto r = eval(R"(import basename from Std\File in basename "/path/to/file.txt")");
    CHECK(r->get<string>() == "file.txt");
}

TEST_CASE("File::basename on bare filename") {
    auto r = eval(R"(import basename from Std\File in basename "file.txt")");
    CHECK(r->get<string>() == "file.txt");
}

TEST_CASE("File::dirname extracts directory") {
    auto r = eval(R"(import dirname from Std\File in dirname "/path/to/file.txt")");
    CHECK(r->get<string>() == "/path/to");
}

TEST_CASE("File::extension extracts extension") {
    auto r = eval(R"(import extension from Std\File in extension "file.txt")");
    CHECK(r->get<string>() == ".txt");
}

TEST_CASE("File::extension on file with no extension") {
    auto r = eval(R"(import extension from Std\File in extension "Makefile")");
    CHECK(r->get<string>() == "");
}

TEST_CASE("File::extension with multiple dots") {
    auto r = eval(R"(import extension from Std\File in extension "archive.tar.gz")");
    CHECK(r->get<string>() == ".gz");
}

TEST_CASE("File::join combines path components") {
    auto r = eval(R"(import join from Std\File in join "/path" "file.txt")");
    CHECK(r->type == RuntimeObjectType::String);
    auto s = r->get<string>();
    CHECK(s.find("path") != string::npos);
    CHECK(s.find("file.txt") != string::npos);
}

TEST_CASE("File::exists on current directory") {
    auto r = eval(R"(import exists from Std\File in exists ".")");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("File::exists on nonexistent path") {
    auto r = eval(R"(import exists from Std\File in exists "/nonexistent/path/12345")");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("File::isDir on current directory") {
    auto r = eval(R"(import isDir from Std\File in isDir ".")");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("File::isFile on nonexistent file") {
    auto r = eval(R"(import isFile from Std\File in isFile "/nonexistent_file_12345.txt")");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("File::absolute returns an absolute path") {
    auto r = eval(R"(
        import absolute from Std\File in
        import startsWith from Std\String in
        startsWith "/" (absolute ".")
    )");
    CHECK(r->get<bool>() == true);
}

// =============================================================================
// Json Module
// =============================================================================

// Json object parsing requires escaped quotes inside strings,
// which is a known limitation (brace escaping TODO). Tested via CLI instead.

TEST_CASE("Json::parse null") {
    auto r = eval(R"(
        import parse from Std\Json in
        import typeOf from Std\Types in
        typeOf (parse "null")
    )");
    CHECK(r->get<shared_ptr<SymbolValue>>()->name == "unit");
}

TEST_CASE("Json::parse boolean") {
    auto r = eval(R"(
        import parse from Std\Json in
        (parse "true", parse "false")
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<bool>() == false);
}

TEST_CASE("Json::parse negative number") {
    auto r = eval(R"(import parse from Std\Json in parse "-42")");
    CHECK(r->get<int>() == -42);
}

TEST_CASE("Json::parse float") {
    auto r = eval(R"(import parse from Std\Json in parse "3.14")");
    CHECK(r->type == RuntimeObjectType::Float);
    CHECK(r->get<double>() == doctest::Approx(3.14));
}

TEST_CASE("Json::parse empty array") {
    auto r = eval(R"(
        import parse from Std\Json in
        import length from Std\List in
        length (parse "[]")
    )");
    CHECK(r->get<int>() == 0);
}

// Json empty object test: {} conflicts with string interpolation syntax

TEST_CASE("Json::stringify array") {
    auto r = eval(R"(import stringify from Std\Json in stringify [1, 2, 3])");
    CHECK(r->type == RuntimeObjectType::String);
    CHECK(r->get<string>() == "[1,2,3]");
}

TEST_CASE("Json::stringify bool") {
    auto r = eval(R"(import stringify from Std\Json in (stringify true, stringify false))");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<string>() == "true");
    CHECK(tup[1]->get<string>() == "false");
}

TEST_CASE("Json::stringify tuple as array") {
    auto r = eval(R"(import stringify from Std\Json in let t = (1, 2, 3) in stringify t)");
    CHECK(r->get<string>() == "[1,2,3]");
}

TEST_CASE("Json::parse and stringify roundtrip for int") {
    auto r = eval(R"(
        import parse, stringify from Std\Json in
        parse (stringify 42)
    )");
    CHECK(r->get<int>() == 42);
}

// =============================================================================
// Regexp Module
// =============================================================================

TEST_CASE("Regexp::test negative match") {
    auto r = eval(R"(import test from Std\Regexp in test "[0-9]+" "abcdef")");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("Regexp::test with anchored pattern") {
    auto r = eval(R"(import test from Std\Regexp in test "^abc" "abcdef")");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Regexp::test with end anchor") {
    auto r = eval(R"(import test from Std\Regexp in test "def$" "abcdef")");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Regexp::match returns none when no match") {
    auto r = eval(R"(
        import match from Std\Regexp in
        import isNone from Std\Option in
        isNone (match "[0-9]+" "abcdef")
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Regexp::match returns some with matched text") {
    auto r = eval(R"(
        import match from Std\Regexp in
        import unwrapOr from Std\Option in
        unwrapOr "" (match "[0-9]+" "abc123def")
    )");
    CHECK(r->get<string>() == "123");
}

TEST_CASE("Regexp::matchAll finds all matches") {
    auto r = eval(R"(
        import matchAll from Std\Regexp in
        import lookup from Std\List in
        let matches = matchAll "[a-z]+" "a1b2c3" in
        (lookup 0 matches, lookup 1 matches, lookup 2 matches)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<string>() == "a");
    CHECK(tup[1]->get<string>() == "b");
    CHECK(tup[2]->get<string>() == "c");
}

TEST_CASE("Regexp::matchAll with no matches returns empty list") {
    auto r = eval(R"(
        import matchAll from Std\Regexp in
        import isEmpty from Std\List in
        isEmpty (matchAll "[0-9]+" "abcdef")
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Regexp::replace with no match leaves string unchanged") {
    auto r = eval(R"(import replace from Std\Regexp in replace "[0-9]+" "X" "abcdef")");
    CHECK(r->get<string>() == "abcdef");
}

TEST_CASE("Regexp::split with complex pattern") {
    auto r = eval(R"(
        import split from Std\Regexp in
        import lookup from Std\List in
        let parts = split "\\s+" "hello   world   test" in
        (lookup 0 parts, lookup 1 parts, lookup 2 parts)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<string>() == "hello");
    CHECK(tup[1]->get<string>() == "world");
    CHECK(tup[2]->get<string>() == "test");
}

// =============================================================================
// List Module - additional edge cases
// =============================================================================

TEST_CASE("List::head") {
    auto r = eval(R"(import head from Std\List in head [10, 20, 30])");
    CHECK(r->get<int>() == 10);
}

TEST_CASE("List::tail") {
    auto r = eval(R"(
        import tail, length from Std\List in
        length (tail [10, 20, 30])
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("List::tail values") {
    auto r = eval(R"(
        import tail, lookup from Std\List in
        let t = tail [10, 20, 30] in
        (lookup 0 t, lookup 1 t)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 20);
    CHECK(tup[1]->get<int>() == 30);
}

TEST_CASE("List::take more than available") {
    auto r = eval(R"(
        import take, length from Std\List in
        length (take 100 [1, 2, 3])
    )");
    CHECK(r->get<int>() == 3);
}

TEST_CASE("List::take zero") {
    auto r = eval(R"(
        import take, isEmpty from Std\List in
        isEmpty (take 0 [1, 2, 3])
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("List::drop more than available") {
    auto r = eval(R"(
        import drop, isEmpty from Std\List in
        isEmpty (drop 100 [1, 2, 3])
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("List::drop zero") {
    auto r = eval(R"(
        import drop, length from Std\List in
        length (drop 0 [1, 2, 3])
    )");
    CHECK(r->get<int>() == 3);
}

TEST_CASE("List::flatten with empty inner lists") {
    auto r = eval(R"(
        import flatten, length from Std\List in
        length (flatten [[], [1], [], [2, 3], []])
    )");
    CHECK(r->get<int>() == 3);
}

TEST_CASE("List::flatten with nested lists") {
    auto r = eval(R"(
        import flatten from Std\List in
        flatten [[1, 2], [3], [4, 5]]
    )");
    auto& seq = r->get<shared_ptr<SeqValue>>()->fields;
    CHECK(seq.size() == 5);
    CHECK(seq[0]->get<int>() == 1);
    CHECK(seq[4]->get<int>() == 5);
}

TEST_CASE("List::zip with different lengths truncates") {
    auto r = eval(R"(
        import zip from Std\List in
        import length from Std\List in
        length (zip [1, 2, 3] ["a", "b"])
    )");
    CHECK(r->get<int>() == 2);
}

TEST_CASE("List::zip produces tuples") {
    auto r = eval(R"(
        import zip, lookup from Std\List in
        import fst, snd from Std\Tuple in
        let pair = lookup 0 (zip [1, 2] ["a", "b"]) in
        (fst pair, snd pair)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 1);
    CHECK(tup[1]->get<string>() == "a");
}

TEST_CASE("List::splitAt at zero") {
    auto r = eval(R"(
        import splitAt from Std\List in
        import length from Std\List in
        let (left, right) = splitAt 0 [1, 2, 3] in
        (length left, length right)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 0);
    CHECK(tup[1]->get<int>() == 3);
}

TEST_CASE("List::splitAt at end") {
    auto r = eval(R"(
        import splitAt from Std\List in
        import length from Std\List in
        let (left, right) = splitAt 3 [1, 2, 3] in
        (length left, length right)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 3);
    CHECK(tup[1]->get<int>() == 0);
}

TEST_CASE("List::reverse of empty list") {
    auto r = eval(R"(
        import reverse, isEmpty from Std\List in
        isEmpty (reverse [])
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("List::reverse of single element") {
    auto r = eval(R"(
        import reverse, lookup from Std\List in
        lookup 0 (reverse [42])
    )");
    CHECK(r->get<int>() == 42);
}

TEST_CASE("List::map on empty list") {
    auto r = eval(R"(
        import map, isEmpty from Std\List in
        isEmpty (map (\x -> x * 2) [])
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("List::filter all match") {
    auto r = eval(R"(
        import filter, length from Std\List in
        length (filter (\x -> x > 0) [1, 2, 3])
    )");
    CHECK(r->get<int>() == 3);
}

TEST_CASE("List::filter none match") {
    auto r = eval(R"(
        import filter, isEmpty from Std\List in
        isEmpty (filter (\x -> x > 100) [1, 2, 3])
    )");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("List::any on empty list") {
    auto r = eval(R"(import any from Std\List in any (\x -> x > 0) [])");
    CHECK(r->get<bool>() == false);
}

TEST_CASE("List::all on empty list") {
    auto r = eval(R"(import all from Std\List in all (\x -> x > 0) [])");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("List::foldl with subtraction shows left-associativity") {
    // foldl (-) 10 [1, 2, 3] = ((10-1)-2)-3 = 4
    auto r = eval(R"(import foldl from Std\List in foldl (\acc x -> acc - x) 10 [1, 2, 3])");
    CHECK(r->get<int>() == 4);
}

TEST_CASE("List::foldr with subtraction shows right-associativity") {
    // foldr (-) 0 [1, 2, 3] = 1-(2-(3-0)) = 1-2+3 = 2
    auto r = eval(R"(import foldr from Std\List in foldr (\x acc -> x - acc) 0 [1, 2, 3])");
    CHECK(r->get<int>() == 2);
}

// =============================================================================
// Timer Module
// =============================================================================

TEST_CASE("Timer::millis returns positive int") {
    auto r = eval(R"(import millis from Std\Timer in millis > 0)");
    CHECK(r->get<bool>() == true);
}

TEST_CASE("Timer::measure returns tuple of (nanos, value)") {
    auto r = eval(R"(
        import measure from Std\Timer in
        let (nanos, value) = measure (\-> 1 + 2) in
        (nanos >= 0, value)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<bool>() == true);
    CHECK(tup[1]->get<int>() == 3);
}

// =============================================================================
// Tuple Module
// =============================================================================

TEST_CASE("Tuple::fst and snd with different types") {
    auto r = eval(R"(
        import fst, snd from Std\Tuple in
        let t = ("hello", 42) in
        (fst t, snd t)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<string>() == "hello");
    CHECK(tup[1]->get<int>() == 42);
}

TEST_CASE("Tuple::swap preserves values") {
    auto r = eval(R"(
        import swap, fst, snd from Std\Tuple in
        let p = ("a", "b") in
        let s = swap p in
        (fst s, snd s)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<string>() == "b");
    CHECK(tup[1]->get<string>() == "a");
}

TEST_CASE("Tuple::mapBoth applies functions to each element") {
    auto r = eval(R"(
        import mapBoth, fst, snd from Std\Tuple in
        let p = (5, 100) in
        let t = mapBoth (\x -> x * 2) (\x -> x + 10) p in
        (fst t, snd t)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 10);
    CHECK(tup[1]->get<int>() == 110);
}

TEST_CASE("Tuple::zip and unzip roundtrip") {
    auto r = eval(R"(
        import zip, unzip from Std\Tuple in
        import length from Std\List in
        let zipped = zip [1, 2, 3] ["a", "b", "c"] in
        let (nums, strs) = unzip zipped in
        (length nums, length strs)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 3);
    CHECK(tup[1]->get<int>() == 3);
}

// =============================================================================
// Range Module
// =============================================================================

TEST_CASE("Range::range toList produces correct elements") {
    auto r = eval(R"(
        import range, toList from Std\Range in
        import lookup from Std\List in
        let list = toList (range 1 5) in
        (lookup 0 list, lookup 4 list)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 1);
    CHECK(tup[1]->get<int>() == 5);
}

TEST_CASE("Range::range single element") {
    auto r = eval(R"(
        import range, length from Std\Range in
        length (range 5 5)
    )");
    CHECK(r->get<int>() == 1);
}

TEST_CASE("Range::take and drop on range") {
    auto r = eval(R"(
        import range, take, drop, toList from Std\Range in
        import length from Std\List in
        let rng = range 1 10 in
        let taken = toList (take 3 rng) in
        let dropped = toList (drop 7 rng) in
        (length taken, length dropped)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 3);
    CHECK(tup[1]->get<int>() == 3);
}

// =============================================================================
// Cross-module integration tests
// =============================================================================

TEST_CASE("Integration: String split then List map") {
    auto r = eval(R"(
        import split from Std\String in
        import map from Std\List in
        import toUpperCase from Std\String in
        import lookup from Std\List in
        let parts = split "," "hello,world" in
        let upper = map toUpperCase parts in
        (lookup 0 upper, lookup 1 upper)
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<string>() == "HELLO");
    CHECK(tup[1]->get<string>() == "WORLD");
}

TEST_CASE("Integration: Dict keys then Set fromList for unique check") {
    auto r = eval(R"(
        import keys from Std\Dict in
        import fromList, size from Std\Set in
        import length from Std\List in
        let ks = keys {"a": 1, "b": 2, "c": 3} in
        (length ks, size (fromList ks))
    )");
    auto& tup = r->get<shared_ptr<TupleValue>>()->fields;
    CHECK(tup[0]->get<int>() == 3);
    CHECK(tup[1]->get<int>() == 3);
}

// Json parse with objects requires embedded quotes (known limitation)

TEST_CASE("Integration: List fold to build string via String join") {
    auto r = eval(R"(
        import map from Std\List in
        import join from Std\String in
        import toStr from Std\Types in
        join ", " (map toStr [1, 2, 3])
    )");
    CHECK(r->get<string>() == "1, 2, 3");
}

TEST_CASE("Integration: Regexp matchAll then List length") {
    auto r = eval(R"(
        import matchAll from Std\Regexp in
        import length from Std\List in
        length (matchAll "[a-zA-Z]+" "hello world 123 foo")
    )");
    CHECK(r->get<int>() == 3);
}

TEST_CASE("Integration: Option map with Math function") {
    auto r = eval(R"(
        import some, map, unwrapOr from Std\Option in
        import abs from Std\Math in
        unwrapOr 0 (map abs (some (-42)))
    )");
    CHECK(r->get<int>() == 42);
}

} // TEST_SUITE("Stdlib Comprehensive")
