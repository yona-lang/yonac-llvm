#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_approx.hpp>
#include "Lexer.h"
#include <sstream>

using namespace yona::lexer;
using namespace std;

struct LexerTest {
    void TestTokens(const string& input, const vector<TokenType>& expected_types) {
        Lexer lexer(input);
        auto result = lexer.tokenize();

        REQUIRE(result.has_value());

        auto tokens = result.value();
        REQUIRE(tokens.size() == expected_types.size());

        for (size_t i = 0; i < expected_types.size(); ++i) {
            CHECK(tokens[i].type == expected_types[i]);
        }
    }

    void TestTokenValues(const string& input, const vector<pair<TokenType, variant<int64_t, double, string>>> expected) {
        Lexer lexer(input);
        auto result = lexer.tokenize();

        REQUIRE(result.has_value());

        auto tokens = result.value();
        REQUIRE(tokens.size() == expected.size() + 1); // +1 for EOF

        for (size_t i = 0; i < expected.size(); ++i) {
            CHECK(tokens[i].type == expected[i].first);

            if (holds_alternative<int64_t>(expected[i].second)) {
                CHECK(get<int64_t>(tokens[i].value) == get<int64_t>(expected[i].second));
            } else if (holds_alternative<double>(expected[i].second)) {
                CHECK(get<double>(tokens[i].value) == Catch::Approx(get<double>(expected[i].second)));
            } else if (holds_alternative<string>(expected[i].second)) {
                CHECK(get<string>(tokens[i].value) == get<string>(expected[i].second));
            }
        }
    }
};

TEST_CASE("SimpleArithmetic", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens("10 + 20", {
        TokenType::YINTEGER,
        TokenType::YPLUS,
        TokenType::YINTEGER,
        TokenType::YEOF_TOKEN
    });
}

TEST_CASE("IntegerLiterals", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokenValues("42 1000 1_000_000", {
        {TokenType::YINTEGER, int64_t(42)},
        {TokenType::YINTEGER, int64_t(1000)},
        {TokenType::YINTEGER, int64_t(1000000)}
    });
}

TEST_CASE("FloatLiterals", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokenValues("3.14 2.0 1e10 1.5e-3", {
        {TokenType::YFLOAT, 3.14},
        {TokenType::YFLOAT, 2.0},
        {TokenType::YFLOAT, 1e10},
        {TokenType::YFLOAT, 1.5e-3}
    });
}

TEST_CASE("StringLiterals", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokenValues(R"("hello" "world\n" "quote:\"" "unicode:\u0041")", {
        {TokenType::YSTRING, string("hello")},
        {TokenType::YSTRING, string("world\n")},
        {TokenType::YSTRING, string("quote:\"")},
        {TokenType::YSTRING, string("unicode:A")}
    });
}

TEST_CASE("Identifiers", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens("foo bar_baz x' _test", {
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YEOF_TOKEN
    });
}

TEST_CASE("Keywords", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens("let if then else true false", {
        TokenType::YLET,
        TokenType::YIF,
        TokenType::YTHEN,
        TokenType::YELSE,
        TokenType::YTRUE,
        TokenType::YFALSE,
        TokenType::YEOF_TOKEN
    });
}

TEST_CASE("Operators", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens("+ - * / % ** == != < > <= >= && || ! & | ^ ~ << >> >>>", {
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

TEST_CASE("Delimiters", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens("( ) [ ] { } , ; : . .. = -> =>", {
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

TEST_CASE("ListOperators", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens(":: <| |> ++ @ _", {
        TokenType::YCONS,
        TokenType::YPIPE_LEFT,
        TokenType::YPIPE_RIGHT,
        TokenType::YJOIN,
        TokenType::YAT,
        TokenType::YUNDERSCORE,
        TokenType::YEOF_TOKEN
    });
}

TEST_CASE("YonaSequenceOperators", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens("-- -| |-", {
        TokenType::YREMOVE,
        TokenType::YPREPEND,
        TokenType::YAPPEND,
        TokenType::YEOF_TOKEN
    });
}

TEST_CASE("Comments", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens(R"(
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

TEST_CASE("Symbols", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    Lexer lexer(":foo :+ :==");
    auto result = lexer.tokenize();

    REQUIRE(result.has_value());
    auto tokens = result.value();
    REQUIRE(tokens.size() == 4); // 3 symbols + EOF

    CHECK(tokens[0].type == TokenType::YSYMBOL);
    CHECK(get<string_view>(tokens[0].value) == "foo");

    CHECK(tokens[1].type == TokenType::YSYMBOL);
    CHECK(get<string_view>(tokens[1].value) == "+");

    CHECK(tokens[2].type == TokenType::YSYMBOL);
    CHECK(get<string_view>(tokens[2].value) == "==");
}

TEST_CASE("ComplexExpression", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens("let x = if y > 0 then y * 2 else -y", {
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

TEST_CASE("ErrorHandling", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    Lexer lexer(R"("unterminated string)");
    auto result = lexer.tokenize();

    REQUIRE_FALSE(result.has_value());
    auto errors = result.error();
    REQUIRE(errors.size() >= 1);
    CHECK(errors[0].type == LexError::Type::UNTERMINATED_STRING);
}

TEST_CASE("CharacterLiterals", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    Lexer lexer("'a' '\\n' '\\u0041'");
    auto result = lexer.tokenize();

    REQUIRE(result.has_value());
    auto tokens = result.value();
    REQUIRE(tokens.size() == 4); // 3 chars + EOF

    CHECK(tokens[0].type == TokenType::YCHARACTER);
    CHECK(get<char32_t>(tokens[0].value) == U'a');

    CHECK(tokens[1].type == TokenType::YCHARACTER);
    CHECK(get<char32_t>(tokens[1].value) == U'\n');

    CHECK(tokens[2].type == TokenType::YCHARACTER);
    CHECK(get<char32_t>(tokens[2].value) == U'A');
}

TEST_CASE("UnicodeIdentifiers", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    fixture.TestTokens("λ пользователь 用户", {
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YIDENTIFIER,
        TokenType::YEOF_TOKEN
    });
}

TEST_CASE("LocationTracking", "[LexerTest]") /* FIXTURE */ {
    LexerTest fixture;
    Lexer lexer("foo\nbar");
    auto result = lexer.tokenize();

    REQUIRE(result.has_value());
    auto tokens = result.value();
    REQUIRE(tokens.size() >= 2);

    CHECK(tokens[0].location.line == 1);
    CHECK(tokens[0].location.column == 1);

    CHECK(tokens[1].location.line == 2);
    CHECK(tokens[1].location.column == 1);
}
