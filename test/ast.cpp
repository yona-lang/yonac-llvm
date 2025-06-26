//
// Created by akovari on 15.12.24.
//
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include "common.h"
#include <unordered_map>
#include <optional>
#include <sstream>

using namespace std;
using namespace yona;
using namespace yona::interp;
using namespace yona::interp::runtime;

using ErrorMap = unordered_map<yona_error::Type, unsigned int>;

// Data-driven test using Catch2's GENERATE macro
TEST_CASE("YonaTest", "[AstTest]") {
  // Test data structure
  struct TestCase {
    string name;
    string input;
    optional<pair<RuntimeObjectType, string>> expected;
    ErrorMap expected_errors;
  };

  // Convert the parameterized test data to Catch2 GENERATE
  auto test_case = GENERATE(
    TestCase{"SimpleAddition", "1 + 2", make_pair(Int, "3"), {}},
    TestCase{"SimpleSubtraction", "5 - 3", make_pair(Int, "2"), {}},
    TestCase{"ParseError", "1 +", nullopt, {{yona_error::Type::SYNTAX, 1}}}
    // Add more test cases here
  );

  SECTION(test_case.name) {
    stringstream ss(test_case.input);
    parser::Parser parser;
    parser::ParseResult parse_result = parser.parse_input(ss);

    auto node = parse_result.node;
    auto type = parse_result.type;
    auto type_ctx = parse_result.ast_ctx;

    if (parse_result.success && test_case.expected.has_value()) {
      Interpreter interpreter;
      auto result = any_cast<RuntimeObjectPtr>(node->accept(interpreter));
      CHECK(result->type == get<0>(test_case.expected.value()));

      stringstream res_ss;
      res_ss << *result;
      CHECK(res_ss.str() == get<1>(test_case.expected.value()));
    } else {
      for (auto [error_type, cnt] : test_case.expected_errors) {
        CHECK(type_ctx.getErrors().count(error_type) == cnt);
      }
    }
  }
}
