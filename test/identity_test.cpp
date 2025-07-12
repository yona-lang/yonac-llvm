#include <catch2/catch_test_macros.hpp>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <iostream>
#include "ast_visitor_impl.h"

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;


TEST_CASE("IdentityLambda", "[IdentityTest]") {
    std::cerr << "TEST: Starting IdentityLambda test" << std::endl;
    parser::Parser parser;
    Interpreter interp;

    std::cerr << "TEST: Creating stringstream" << std::endl;
    // Test the full lambda application
    stringstream ss("(\\(x) -> x)(42)");
    std::cerr << "TEST: Parsing lambda application: (\\(x) -> x)(42)" << std::endl;
    auto parse_result = parser.parse_input(ss);
    std::cerr << "TEST: parse_input returned" << std::endl;

    if (!parse_result.success || parse_result.node == nullptr) {
        if (parse_result.ast_ctx.hasErrors()) {
            for (const auto& [type, error] : parse_result.ast_ctx.getErrors()) {
                std::cerr << "Parse error: " << error->what() << std::endl;
            }
        }
        FAIL("Parse failed");
    }

    std::cerr << "TEST: Checking for MainNode" << std::endl;
    auto main = dynamic_cast<MainNode*>(parse_result.node.get());
    REQUIRE(main != nullptr);

    std::cerr << "TEST: Calling interpreter" << std::endl;
    std::cerr << "TEST: About to call interp.visit(main)" << std::endl;

    try {
        auto visit_result = interp.visit(main);
        std::cerr << "TEST: interp.visit(main) returned" << std::endl;
        auto interpreter_result = visit_result;
      auto result = interpreter_result.value;
        std::cerr << "TEST: result extraction complete" << std::endl;
        std::cerr << "TEST: Interpreter returned" << std::endl;

        // The result should be an integer 42
        CHECK(result->type == yona::interp::runtime::Int);
        CHECK(result->get<int>() == 42);
    } catch (const std::exception& e) {
        std::cerr << "TEST: Exception caught: " << e.what() << std::endl;
        FAIL("Exception: " << e.what());
    }
}
