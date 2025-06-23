#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <optional>
#include <span>
#include <expected>
#include <unordered_map>
#include <format>
#include <source_location>

#include "SourceLocation.h"

namespace yona::lexer {

using ::yona::SourceLocation;

// Token types
enum class TokenType {
    // Literals
    INTEGER,
    FLOAT,
    STRING,
    CHARACTER,
    SYMBOL,
    TRUE,
    FALSE,
    UNIT,
    
    // Identifiers and keywords
    IDENTIFIER,
    MODULE,
    IMPORT,
    FROM,
    AS,
    EXPORT,
    LET,
    IN,
    IF,
    THEN,
    ELSE,
    CASE,
    OF,
    DO,
    END,
    TRY,
    CATCH,
    RAISE,
    WITH,
    FUN,
    LAMBDA,
    RECORD,
    TYPE,
    
    // Operators
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    POWER,          // **
    
    // Comparison
    EQ,             // ==
    NEQ,            // !=
    LT,             // <
    GT,             // >
    LTE,            // <=
    GTE,            // >=
    
    // Logical
    AND,            // &&
    OR,             // ||
    NOT,            // !
    
    // Bitwise
    BIT_AND,        // &
    BIT_OR,         // |
    BIT_XOR,        // ^
    BIT_NOT,        // ~
    LEFT_SHIFT,     // <<
    RIGHT_SHIFT,    // >>
    ZERO_FILL_RIGHT_SHIFT, // >>>
    
    // Assignment and binding
    ASSIGN,         // =
    ARROW,          // ->
    FAT_ARROW,      // =>
    
    // Delimiters
    LPAREN,         // (
    RPAREN,         // )
    LBRACKET,       // [
    RBRACKET,       // ]
    LBRACE,         // {
    RBRACE,         // }
    
    // Separators
    COMMA,          // ,
    SEMICOLON,      // ;
    COLON,          // :
    DOT,            // .
    DOTDOT,         // ..
    PIPE,           // |
    AT,             // @
    UNDERSCORE,     // _
    BACKSLASH,      // \ (for module paths)
    
    // List operations
    CONS,           // ::
    PIPE_LEFT,      // <|
    PIPE_RIGHT,     // |>
    JOIN,           // ++
    REMOVE,         // --
    PREPEND,        // -|
    APPEND,         // |-
    
    // Special
    EOF_TOKEN,
    NEWLINE,
    
    // Error
    ERROR
};

// Token structure
struct Token {
    TokenType type;
    std::string_view lexeme;
    SourceLocation location;
    
    // Value for literals
    using LiteralValue = std::variant<
        std::monostate,  // No value
        int64_t,         // INTEGER
        double,          // FLOAT
        std::string,     // STRING (owned, as it may contain escapes)
        char32_t,        // CHARACTER
        std::string_view // SYMBOL, IDENTIFIER (references to source)
    >;
    LiteralValue value;
    
    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] bool is_keyword() const noexcept;
    [[nodiscard]] bool is_operator() const noexcept;
    [[nodiscard]] bool is_literal() const noexcept;
};

// Lexer error
struct LexError {
    enum class Type {
        INVALID_CHARACTER,
        UNTERMINATED_STRING,
        UNTERMINATED_COMMENT,
        INVALID_ESCAPE_SEQUENCE,
        INVALID_NUMBER_FORMAT,
        INVALID_CHARACTER_LITERAL,
        UNICODE_ERROR
    };
    
    Type type;
    std::string message;
    SourceLocation location;
    
    [[nodiscard]] std::string to_string() const {
        return std::format("{}: Lexical error: {}", location.to_string(), message);
    }
};

// Modern C++23 lexer with zero-copy tokenization where possible
class Lexer {
public:
    explicit Lexer(std::string_view source, std::string_view filename = "<input>");
    
    // Main tokenization interface
    [[nodiscard]] std::expected<std::vector<Token>, std::vector<LexError>> tokenize();
    
    // Streaming interface for large files
    [[nodiscard]] std::expected<Token, LexError> next_token();
    
    // Peek at next token without consuming
    [[nodiscard]] std::expected<Token, LexError> peek_token();
    
    // Check if at end of input
    [[nodiscard]] bool is_at_end() const noexcept { return current_ >= source_.length(); }
    
    // Get current source location
    [[nodiscard]] SourceLocation current_location() const noexcept {
        return {line_, column_, current_, 0, filename_};
    }

private:
    std::string_view source_;
    std::string_view filename_;
    size_t current_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
    size_t token_start_ = 0;
    size_t token_start_line_ = 1;
    size_t token_start_column_ = 1;
    
    // Keyword lookup table
    static const std::unordered_map<std::string_view, TokenType> keywords_;
    
    // Character classification
    [[nodiscard]] static bool is_alpha(char32_t ch) noexcept;
    [[nodiscard]] static bool is_digit(char32_t ch) noexcept;
    [[nodiscard]] static bool is_alnum(char32_t ch) noexcept;
    [[nodiscard]] static bool is_whitespace(char32_t ch) noexcept;
    [[nodiscard]] static bool is_identifier_start(char32_t ch) noexcept;
    [[nodiscard]] static bool is_identifier_continue(char32_t ch) noexcept;
    [[nodiscard]] static bool is_operator_char(char32_t ch) noexcept;
    
    // UTF-8 handling
    [[nodiscard]] std::expected<char32_t, LexError> peek_char() const;
    [[nodiscard]] std::expected<char32_t, LexError> advance_char();
    void skip_char();
    
    // Token creation
    [[nodiscard]] Token make_token(TokenType type) const;
    [[nodiscard]] Token make_token(TokenType type, Token::LiteralValue value) const;
    [[nodiscard]] Token make_error_token(const std::string& message) const;
    
    // Lexing methods
    void skip_whitespace_and_comments();
    [[nodiscard]] std::expected<Token, LexError> scan_token();
    [[nodiscard]] std::expected<Token, LexError> scan_identifier();
    [[nodiscard]] std::expected<Token, LexError> scan_number();
    [[nodiscard]] std::expected<Token, LexError> scan_string();
    [[nodiscard]] std::expected<Token, LexError> scan_character();
    [[nodiscard]] std::expected<Token, LexError> scan_symbol();
    [[nodiscard]] std::expected<Token, LexError> scan_operator(char32_t first_char);
    
    // Helper methods
    [[nodiscard]] bool match(char32_t expected);
    [[nodiscard]] bool match_sequence(std::string_view sequence);
    [[nodiscard]] std::expected<char32_t, LexError> parse_escape_sequence();
    [[nodiscard]] std::expected<char32_t, LexError> parse_unicode_escape(int digits);
    
    // Mark token start position
    void mark_token_start() {
        token_start_ = current_;
        token_start_line_ = line_;
        token_start_column_ = column_;
    }
    
    // Get current lexeme
    [[nodiscard]] std::string_view current_lexeme() const {
        return source_.substr(token_start_, current_ - token_start_);
    }
};

// Token type to string conversion for debugging
[[nodiscard]] std::string_view token_type_to_string(TokenType type) noexcept;

} // namespace yona::lexer