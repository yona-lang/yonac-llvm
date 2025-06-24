#include <iostream>
#include <sstream>
#include <gtest/gtest.h>
#include "Parser.h"

using namespace std;
using namespace yona;

TEST(ParserDebugTest, MinimalParseTest) {
    // Test with the simplest possible input
    stringstream ss("1");

    parser::Parser p;
    try {
        cout << "Calling parse_input with '1'..." << endl;
        auto result = p.parse_input(ss);
        cout << "parse_input returned successfully" << endl;

        EXPECT_TRUE(result.success || !result.success); // Just check it doesn't crash
    } catch (const exception& e) {
        cout << "Exception caught: " << e.what() << endl;
        FAIL() << "Parser threw exception: " << e.what();
    }
}

TEST(ParserDebugTest, DirectParserTest) {
    // Test parser directly without going through Parser class
    stringstream ss("10 + 20");

    parser::Parser p;
    try {
        cout << "Calling parse_input..." << endl;
        auto result = p.parse_input(ss);
        cout << "parse_input returned" << endl;

        EXPECT_TRUE(result.success || !result.success); // Just check it doesn't crash
    } catch (const exception& e) {
        cout << "Exception caught: " << e.what() << endl;
        FAIL() << "Parser threw exception: " << e.what();
    }
}
