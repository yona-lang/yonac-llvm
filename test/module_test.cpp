#include <gtest/gtest.h>
#include "Interpreter.h"
#include "Parser.h"
#include "runtime.h"
#include <cstdlib>
#include <sstream>
#include <filesystem>

using namespace yona;
using namespace yona::ast;
using namespace yona::interp;
using namespace yona::interp::runtime;
using namespace std;

class ModuleTest : public ::testing::Test {
protected:
    parser::Parser parser;
    Interpreter interp;
    
    void SetUp() override {
        // Set YONA_PATH to include test/code directory
        string yona_path = filesystem::current_path().string() + "/test/code";
        setenv("YONA_PATH", yona_path.c_str(), 1);
    }
};

TEST_F(ModuleTest, SimpleModuleImport) {
    // Test: import add from Test\Test in add(1, 2)
    stringstream code("import add from Test\\Test in add(1, 2)");
    
    auto parse_result = parser.parse_input(code);
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, Int);
    EXPECT_EQ(result->get<int>(), 3);
}

TEST_F(ModuleTest, ImportWithAlias) {
    // Test: import multiply as mult from Test\Test in mult(3, 4)
    stringstream code("import multiply as mult from Test\\Test in mult(3, 4)");
    
    auto parse_result = parser.parse_input(code);
    ASSERT_TRUE(parse_result.node != nullptr);
    
    auto result = any_cast<RuntimeObjectPtr>(interp.visit(parse_result.node.get()));
    
    EXPECT_EQ(result->type, Int);
    EXPECT_EQ(result->get<int>(), 12);
}

TEST_F(ModuleTest, ImportNonExportedFunction) {
    // Test: import internal_func from Test\Test - should fail
    stringstream code("import internal_func from Test\\Test in internal_func(5)");
    
    auto parse_result = parser.parse_input(code);
    ASSERT_TRUE(parse_result.node != nullptr);
    
    EXPECT_THROW({
        interp.visit(parse_result.node.get());
    }, yona_error);
}

TEST_F(ModuleTest, ModuleCaching) {
    // Import same module twice, should use cache
    
    // First import
    stringstream code1("import add from Test\\Test in add(10, 20)");
    auto parse_result1 = parser.parse_input(code1);
    ASSERT_TRUE(parse_result1.node != nullptr);
    
    auto result1 = any_cast<RuntimeObjectPtr>(interp.visit(parse_result1.node.get()));
    EXPECT_EQ(result1->type, Int);
    EXPECT_EQ(result1->get<int>(), 30);
    
    // Second import - should use cached module  
    stringstream code2("import multiply from Test\\Test in multiply(5, 6)");
    auto parse_result2 = parser.parse_input(code2);
    ASSERT_TRUE(parse_result2.node != nullptr);
    
    auto result2 = any_cast<RuntimeObjectPtr>(interp.visit(parse_result2.node.get()));
    EXPECT_EQ(result2->type, Int);
    EXPECT_EQ(result2->get<int>(), 30);
}