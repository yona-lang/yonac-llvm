#include <gtest/gtest.h>
#include "Lexer.h"
#include <sstream>

using namespace yona::lexer;
using namespace std;

class LexerTest : public ::testing::Test {
protected:
    void TestTokens(const string& input, const vector<TokenType>& expected_types) {
        Lexer lexer(input);
        auto result = lexer.tokenize();

        ASSERT_TRUE(result.has_value()) << "Lexer returned errors";

        auto tokens = result.value();
        ASSERT_EQ(tokens.size(), expected_types.size())
            << "Expected " << expected_types.size() << " tokens, got " << tokens.size();

        for (size_t i = 0; i < expected_types.size(); ++i) {
            EXPECT_EQ(tokens[i].type, expected_types[i])
                << "Token " << i << ": expected " << token_type_to_string(expected_types[i])
                << ", got " << token_type_to_string(tokens[i].type)
                << " ('" << tokens[i].lexeme << "')";
        }
    }

    void TestTokenValues(const string& input, const vector<pair<TokenType, variant<int64_t, double, string>>> expected) {
        Lexer lexer(input);
        auto result = lexer.tokenize();

        ASSERT_TRUE(result.has_value()) << "Lexer returned errors";

        auto tokens = result.value();
        ASSERT_EQ(tokens.size(), expected.size() + 1) // +1 for EOF
            << "Expected " << expected.size() << " tokens (+ EOF), got " << tokens.size();

        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(tokens[i].type, expected[i].first)
                << "Token " << i << ": type mismatch";

            if (holds_alternative<int64_t>(expected[i].second)) {
                EXPECT_EQ(get<int64_t>(tokens[i].value), get<int64_t>(expected[i].second))
                    << "Token " << i << ": integer value mismatch";
            } else if (holds_alternative<double>(expected[i].second)) {
                EXPECT_DOUBLE_EQ(get<double>(tokens[i].value), get<double>(expected[i].second))
                    << "Token " << i << ": float value mismatch";
            } else if (holds_alternative<string>(expected[i].second)) {
                EXPECT_EQ(get<string>(tokens[i].value), get<string>(expected[i].second))
                    << "Token " << i << ": string value mismatch";
            }
        }
    }
};

TEST_F(LexerTest, SimpleArithmetic) {
    TestTokens("10 + 20", {
        TokenType::YINTEGER,
        TokenType::YPLUS,
        TokenType::YINTEGER,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, IntegerLiterals) {
    TestTokenValues("42 1000 1_000_000", {
        {TokenType::YINTEGER, int64_t(42)},
        {TokenType::YINTEGER, int64_t(1000)},
        {TokenType::YINTEGER, int64_t(1000000)}
    });
}

TEST_F(LexerTest, FloatLiterals) {
    TestTokenValues("3.14 2.0 1e10 1.5e-3", {
        {TokenType::YFLOAT, 3.14},
        {TokenType::YFLOAT, 2.0},
        {TokenType::YFLOAT, 1e10},
        {TokenType::YFLOAT, 1.5e-3}
    });
}

TEST_F(LexerTest, StringLiterals) {
    TestTokenValues(R"("hello" "world\n" "quote:\"" "unicode:\u0041")", {
        {TokenType::YSTRING, string("hello")},
        {TokenType::YSTRING, string("world\n")},
        {TokenType::YSTRING, string("quote:\"")},
        {TokenType::YSTRING, string("unicode:A")}
    });
}

TEST_F(LexerTest, Identifiers) {
    TestTokens("foo bar_baz x' _test", {
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, Keywords) {
    TestTokens("let if then else true false", {
        TokenType::YLET,
        TokenType::YIF,
        TokenType::YTHEN,
        TokenType::YELSE,
        TokenType::YTRUE,
        TokenType::YFALSE,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, Operators) {
    TestTokens("+ - * / % ** == != < > <= >= && || ! & | ^ ~ << >> >>>", {
        TokenType::YPLUS,
        TokenType::YMINUS,
        TokenType::YSTAR,
        TokenType::YSLASH,
        TokenType::YPERCENT,
        TokenType::YPOWER,
        TokenType::YEQ,
        TokenType::YNEQ,
        TokenType::YLT,
        TokenType::YGT,
        TokenType::YLTE,
        TokenType::YGTE,
        TokenType::YAND,
        TokenType::YOR,
        TokenType::YNOT,
        TokenType::YBIT_AND,
        TokenType::YPIPE,
        TokenType::YBIT_XOR,
        TokenType::YBIT_NOT,
        TokenType::YLEFT_SHIFT,
        TokenType::YRIGHT_SHIFT,
        TokenType::YZERO_FILL_RIGHT_SHIFT,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, Delimiters) {
    TestTokens("( ) [ ] { } , ; : . .. = -> =>", {
        TokenType::YLPAREN,
        TokenType::YRPAREN,
        TokenType::YLBRACKET,
        TokenType::YRBRACKET,
        TokenType::YLBRACE,
        TokenType::YRBRACE,
        TokenType::YCOMMA,
        TokenType::YSEMICOLON,
        TokenType::YCOLON,
        TokenType::YDOT,
        TokenType::YDOTDOT,
        TokenType::YASSIGN,
        TokenType::YARROW,
        TokenType::YFAT_ARROW,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, ListOperators) {
    TestTokens(":: <| |> ++ @ _", {
        TokenType::YCONS,
        TokenType::YPIPE_LEFT,
        TokenType::YPIPE_RIGHT,
        TokenType::YJOIN,
        TokenType::YAT,
        TokenType::YUNDERSCORE,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, YonaSequenceOperators) {
    TestTokens("-- -| |-", {
        TokenType::YREMOVE,
        TokenType::YPREPEND,
        TokenType::YAPPEND,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, Comments) {
    TestTokens(R"(
        # Single line comment
        42 # Another comment
        /* Multi-line
           comment */
        43
        /* Nested /* comments */ work */
        44
    )", {
        TokenType::YINTEGER,
        TokenType::YINTEGER,
        TokenType::YINTEGER,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, Symbols) {
    Lexer lexer(":foo :+ :==");
    auto result = lexer.tokenize();

    ASSERT_TRUE(result.has_value());
    auto tokens = result.value();
    ASSERT_EQ(tokens.size(), 4); // 3 symbols + EOF

    EXPECT_EQ(tokens[0].type, TokenType::YSYMBOL);
    EXPECT_EQ(get<string_view>(tokens[0].value), "foo");

    EXPECT_EQ(tokens[1].type, TokenType::YSYMBOL);
    EXPECT_EQ(get<string_view>(tokens[1].value), "+");

    EXPECT_EQ(tokens[2].type, TokenType::YSYMBOL);
    EXPECT_EQ(get<string_view>(tokens[2].value), "==");
}

TEST_F(LexerTest, ComplexExpression) {
    TestTokens("let x = if y > 0 then y * 2 else -y", {
        TokenType::YLET,
        TokenType::YIDENTIFIER,
        TokenType::YASSIGN,
        TokenType::YIF,
        TokenType::YIDENTIFIER,
        TokenType::YGT,
        TokenType::YINTEGER,
        TokenType::YTHEN,
        TokenType::YIDENTIFIER,
        TokenType::YSTAR,
        TokenType::YINTEGER,
        TokenType::YELSE,
        TokenType::YMINUS,
        TokenType::YIDENTIFIER,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, ErrorHandling) {
    Lexer lexer(R"("unterminated string)");
    auto result = lexer.tokenize();

    ASSERT_FALSE(result.has_value());
    auto errors = result.error();
    ASSERT_GE(errors.size(), 1);
    EXPECT_EQ(errors[0].type, LexError::Type::UNTERMINATED_STRING);
}

TEST_F(LexerTest, CharacterLiterals) {
    Lexer lexer("'a' '\\n' '\\u0041'");
    auto result = lexer.tokenize();

    ASSERT_TRUE(result.has_value());
    auto tokens = result.value();
    ASSERT_EQ(tokens.size(), 4); // 3 chars + EOF

    EXPECT_EQ(tokens[0].type, TokenType::YCHARACTER);
    EXPECT_EQ(get<char32_t>(tokens[0].value), U'a');

    EXPECT_EQ(tokens[1].type, TokenType::YCHARACTER);
    EXPECT_EQ(get<char32_t>(tokens[1].value), U'\n');

    EXPECT_EQ(tokens[2].type, TokenType::YCHARACTER);
    EXPECT_EQ(get<char32_t>(tokens[2].value), U'A');
}

TEST_F(LexerTest, UnicodeIdentifiers) {
    TestTokens("λ пользователь 用户", {
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YEOF_TOKEN
    });
}

TEST_F(LexerTest, LocationTracking) {
    Lexer lexer("foo\nbar");
    auto result = lexer.tokenize();

    ASSERT_TRUE(result.has_value());
    auto tokens = result.value();
    ASSERT_GE(tokens.size(), 2);

    EXPECT_EQ(tokens[0].location.line, 1);
    EXPECT_EQ(tokens[0].location.column, 1);

    EXPECT_EQ(tokens[1].location.line, 2);
    EXPECT_EQ(tokens[1].location.column, 1);
}
