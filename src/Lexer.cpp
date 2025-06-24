#include <algorithm>
#include <expected>

#include "Lexer.h"

namespace yona::lexer {

// Keyword lookup table
const std::unordered_map<std::string_view, TokenType> Lexer::keywords_ = {
    {"module", TokenType::YMODULE},
    {"import", TokenType::YIMPORT},
    {"from", TokenType::YFROM},
    {"as", TokenType::YAS},
    {"exports", TokenType::YEXPORT},
    {"let", TokenType::YLET},
    {"in", TokenType::YIN},
    {"if", TokenType::YIF},
    {"then", TokenType::YTHEN},
    {"else", TokenType::YELSE},
    {"case", TokenType::YCASE},
    {"of", TokenType::YOF},
    {"do", TokenType::YDO},
    {"end", TokenType::YEND},
    {"try", TokenType::YTRY},
    {"catch", TokenType::YCATCH},
    {"raise", TokenType::YRAISE},
    {"with", TokenType::YWITH},
    {"fun", TokenType::YFUN},
    {"lambda", TokenType::YLAMBDA},
    {"record", TokenType::YRECORD},
    {"type", TokenType::YTYPE},
    {"true", TokenType::YTRUE},
    {"false", TokenType::YFALSE},
};

// Token methods
std::string Token::to_string() const {
    return std::format("[{}:{}:{} {} '{}']",
        location.line, location.column,
        token_type_to_string(type),
        static_cast<int>(type),
        lexeme);
}

bool Token::is_keyword() const noexcept {
    return type >= TokenType::YMODULE && type <= TokenType::YTYPE;
}

bool Token::is_operator() const noexcept {
    return type >= TokenType::YPLUS && type <= TokenType::YJOIN;
}

bool Token::is_literal() const noexcept {
    return type >= TokenType::YINTEGER && type <= TokenType::YUNIT;
}

// Character classification
bool Lexer::is_alpha(char32_t ch) noexcept {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool Lexer::is_digit(char32_t ch) noexcept {
    return ch >= '0' && ch <= '9';
}

bool Lexer::is_alnum(char32_t ch) noexcept {
    return is_alpha(ch) || is_digit(ch);
}

bool Lexer::is_whitespace(char32_t ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

bool Lexer::is_identifier_start(char32_t ch) noexcept {
    return is_alpha(ch) || ch == '_' || ch > 127; // Allow Unicode identifiers
}

bool Lexer::is_identifier_continue(char32_t ch) noexcept {
    return is_alnum(ch) || ch == '_' || ch == '\'' || ch > 127;
}

bool Lexer::is_operator_char(char32_t ch) noexcept {
    return ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' ||
           ch == '=' || ch == '!' || ch == '<' || ch == '>' || ch == '&' ||
           ch == '|' || ch == '^' || ch == '~' || ch == '@';
}

// Constructor
Lexer::Lexer(std::string_view source, std::string_view filename)
    : source_(source), filename_(filename) {}

// UTF-8 decoding
std::expected<char32_t, LexError> Lexer::peek_char() const {
    if (current_ >= source_.length()) {
        return U'\0';
    }

    // Simple ASCII fast path
    unsigned char ch = source_[current_];
    if (ch < 0x80) {
        return static_cast<char32_t>(ch);
    }

    // UTF-8 decoding
    size_t len = 0;
    char32_t result = 0;

    if ((ch & 0xE0) == 0xC0) {
        len = 2;
        result = ch & 0x1F;
    } else if ((ch & 0xF0) == 0xE0) {
        len = 3;
        result = ch & 0x0F;
    } else if ((ch & 0xF8) == 0xF0) {
        len = 4;
        result = ch & 0x07;
    } else {
        return std::unexpected(LexError{
            LexError::Type::INVALID_CHARACTER,
            "Invalid UTF-8 sequence",
            current_location()
        });
    }

    if (current_ + len > source_.length()) {
        return std::unexpected(LexError{
            LexError::Type::INVALID_CHARACTER,
            "Truncated UTF-8 sequence",
            current_location()
        });
    }

    for (size_t i = 1; i < len; ++i) {
        unsigned char byte = source_[current_ + i];
        if ((byte & 0xC0) != 0x80) {
            return std::unexpected(LexError{
                LexError::Type::INVALID_CHARACTER,
                "Invalid UTF-8 continuation byte",
                current_location()
            });
        }
        result = (result << 6) | (byte & 0x3F);
    }

    return result;
}

std::expected<char32_t, LexError> Lexer::advance_char() {
    auto ch_result = peek_char();
    if (!ch_result) {
        return ch_result;
    }

    char32_t ch = ch_result.value();

    // Update position
    if (ch == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }

    // Advance current position by the number of bytes in the UTF-8 sequence
    if (ch < 0x80) {
        current_++;
    } else if (ch < 0x800) {
        current_ += 2;
    } else if (ch < 0x10000) {
        current_ += 3;
    } else {
        current_ += 4;
    }

    return ch;
}

void Lexer::skip_char() {
    if (auto result = advance_char(); !result) {
        // Ignore error when skipping
    }
}

// Token creation
Token Lexer::make_token(TokenType type) const {
    return Token{
        type,
        current_lexeme(),
        {token_start_line_, token_start_column_, token_start_, current_ - token_start_, filename_},
        {}
    };
}

Token Lexer::make_token(TokenType type, Token::LiteralValue value) const {
    return Token{
        type,
        current_lexeme(),
        {token_start_line_, token_start_column_, token_start_, current_ - token_start_, filename_},
        std::move(value)
    };
}

Token Lexer::make_error_token(const std::string& message) const {
    return Token{
        TokenType::YERROR,
        current_lexeme(),
        {token_start_line_, token_start_column_, token_start_, current_ - token_start_, filename_},
        message
    };
}

// Skip whitespace and comments
void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        auto ch_result = peek_char();
        if (!ch_result) return;

        char32_t ch = ch_result.value();

        if (is_whitespace(ch)) {
            skip_char();
        } else if (ch == '#') {
            // Single-line comment
            skip_char();
            while (!is_at_end()) {
                auto next = peek_char();
                if (!next || next.value() == '\n') break;
                skip_char();
            }
        } else if (ch == '/' && current_ + 1 < source_.length() && source_[current_ + 1] == '*') {
            // Multi-line comment
            skip_char(); // /
            skip_char(); // *
            int depth = 1;

            while (!is_at_end() && depth > 0) {
                auto next = peek_char();
                if (!next) break;

                if (next.value() == '/' && current_ + 1 < source_.length() && source_[current_ + 1] == '*') {
                    skip_char();
                    skip_char();
                    depth++;
                } else if (next.value() == '*' && current_ + 1 < source_.length() && source_[current_ + 1] == '/') {
                    skip_char();
                    skip_char();
                    depth--;
                } else {
                    skip_char();
                }
            }
        } else {
            break;
        }
    }
}

// Scan identifier
std::expected<Token, LexError> Lexer::scan_identifier() {
    while (!is_at_end()) {
        auto ch_result = peek_char();
        if (!ch_result) return std::unexpected(ch_result.error());

        if (!is_identifier_continue(ch_result.value())) {
            break;
        }
        skip_char();
    }

    std::string_view lexeme = current_lexeme();

    // Check if it's a keyword
    if (auto it = keywords_.find(lexeme); it != keywords_.end()) {
        return make_token(it->second);
    }

    return make_token(TokenType::YIDENTIFIER, lexeme);
}

// Scan number
std::expected<Token, LexError> Lexer::scan_number() {
    bool has_dot = false;
    bool has_exp = false;

    while (!is_at_end()) {
        auto ch_result = peek_char();
        if (!ch_result) return std::unexpected(ch_result.error());

        char32_t ch = ch_result.value();

        if (is_digit(ch)) {
            skip_char();
        } else if (ch == '.' && !has_dot && !has_exp) {
            // Look ahead to ensure it's not ".."
            if (current_ + 1 < source_.length() && source_[current_ + 1] == '.') {
                break;
            }
            has_dot = true;
            skip_char();
        } else if ((ch == 'e' || ch == 'E') && !has_exp) {
            has_exp = true;
            skip_char();

            // Handle optional sign
            auto next = peek_char();
            if (next && (next.value() == '+' || next.value() == '-')) {
                skip_char();
            }
        } else if (ch == '_') {
            // Allow underscores in numbers for readability
            skip_char();
        } else if ((ch == 'b' || ch == 'B') && !has_dot && !has_exp) {
            // Check if this is a byte suffix (must be at end of number)
            auto next = peek_char();
            if (!next || (!is_digit(next.value()) && next.value() != '_')) {
                skip_char(); // Consume the 'b' or 'B'
                break; // Exit the loop, we'll handle byte conversion after
            }
            break;
        } else {
            break;
        }
    }

    std::string_view lexeme = current_lexeme();

    // Check for byte suffix
    bool is_byte = false;
    if (lexeme.size() > 1 && (lexeme.back() == 'b' || lexeme.back() == 'B')) {
        is_byte = true;
        lexeme = lexeme.substr(0, lexeme.size() - 1);
    }

    // Remove underscores for parsing
    std::string clean_lexeme;
    for (char c : lexeme) {
        if (c != '_') clean_lexeme += c;
    }

    if (is_byte) {
        // Parse as byte
        int64_t value;
        try {
            size_t idx;
            value = std::stoll(clean_lexeme, &idx);
            if (idx != clean_lexeme.size() || value < 0 || value > 255) {
                return std::unexpected(LexError{
                    LexError::Type::INVALID_NUMBER_FORMAT,
                    "Byte literal must be between 0 and 255",
                    current_location()
                });
            }
        } catch (const std::exception&) {
            return std::unexpected(LexError{
                LexError::Type::INVALID_NUMBER_FORMAT,
                "Invalid byte literal",
                current_location()
            });
        }
        return make_token(TokenType::YBYTE, static_cast<uint8_t>(value));
    } else if (has_dot || has_exp) {
        // Parse as float
        double value;
        try {
            size_t idx;
            value = std::stod(clean_lexeme, &idx);
            if (idx != clean_lexeme.size()) {
                return std::unexpected(LexError{
                    LexError::Type::INVALID_NUMBER_FORMAT,
                    "Invalid floating-point number",
                    current_location()
                });
            }
        } catch (const std::exception&) {
            return std::unexpected(LexError{
                LexError::Type::INVALID_NUMBER_FORMAT,
                "Invalid floating-point number",
                current_location()
            });
        }
        return make_token(TokenType::YFLOAT, value);
    } else {
        // Parse as integer
        int64_t value;
        try {
            size_t idx;
            value = std::stoll(clean_lexeme, &idx);
            if (idx != clean_lexeme.size()) {
                return std::unexpected(LexError{
                    LexError::Type::INVALID_NUMBER_FORMAT,
                    "Invalid integer",
                    current_location()
                });
            }
        } catch (const std::exception&) {
            return std::unexpected(LexError{
                LexError::Type::INVALID_NUMBER_FORMAT,
                "Invalid integer",
                current_location()
            });
        }
        return make_token(TokenType::YINTEGER, value);
    }
}

// Parse escape sequence
std::expected<char32_t, LexError> Lexer::parse_escape_sequence() {
    auto ch_result = advance_char();
    if (!ch_result) return std::unexpected(ch_result.error());

    char32_t ch = ch_result.value();
    switch (ch) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        case '0': return '\0';
        case 'u': return parse_unicode_escape(4);
        case 'U': return parse_unicode_escape(8);
        default:
            return std::unexpected(LexError{
                LexError::Type::INVALID_ESCAPE_SEQUENCE,
                std::format("Invalid escape sequence '\\{}'", static_cast<char>(ch)),
                current_location()
            });
    }
}

// Parse unicode escape
std::expected<char32_t, LexError> Lexer::parse_unicode_escape(int digits) {
    char32_t value = 0;

    for (int i = 0; i < digits; ++i) {
        auto ch_result = advance_char();
        if (!ch_result) return std::unexpected(ch_result.error());

        char32_t ch = ch_result.value();
        if (ch >= '0' && ch <= '9') {
            value = value * 16 + (ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            value = value * 16 + (ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            value = value * 16 + (ch - 'A' + 10);
        } else {
            return std::unexpected(LexError{
                LexError::Type::INVALID_ESCAPE_SEQUENCE,
                "Invalid hex digit in unicode escape",
                current_location()
            });
        }
    }

    return value;
}

// Scan string
std::expected<Token, LexError> Lexer::scan_string() {
    skip_char(); // Skip opening quote
    std::string value;

    while (!is_at_end()) {
        auto ch_result = peek_char();
        if (!ch_result) return std::unexpected(ch_result.error());

        char32_t ch = ch_result.value();

        if (ch == '"') {
            skip_char();
            return make_token(TokenType::YSTRING, std::move(value));
        } else if (ch == '\\') {
            skip_char();
            auto escaped = parse_escape_sequence();
            if (!escaped) return std::unexpected(escaped.error());

            // Convert char32_t to UTF-8
            if (escaped.value() < 0x80) {
                value += static_cast<char>(escaped.value());
            } else if (escaped.value() < 0x800) {
                value += static_cast<char>(0xC0 | (escaped.value() >> 6));
                value += static_cast<char>(0x80 | (escaped.value() & 0x3F));
            } else if (escaped.value() < 0x10000) {
                value += static_cast<char>(0xE0 | (escaped.value() >> 12));
                value += static_cast<char>(0x80 | ((escaped.value() >> 6) & 0x3F));
                value += static_cast<char>(0x80 | (escaped.value() & 0x3F));
            } else {
                value += static_cast<char>(0xF0 | (escaped.value() >> 18));
                value += static_cast<char>(0x80 | ((escaped.value() >> 12) & 0x3F));
                value += static_cast<char>(0x80 | ((escaped.value() >> 6) & 0x3F));
                value += static_cast<char>(0x80 | (escaped.value() & 0x3F));
            }
        } else if (ch == '\n') {
            return std::unexpected(LexError{
                LexError::Type::UNTERMINATED_STRING,
                "Unterminated string literal",
                current_location()
            });
        } else {
            skip_char();
            // Add the character to the string
            size_t start = current_;
            if (ch < 0x80) {
                value += static_cast<char>(ch);
            } else {
                // Multi-byte UTF-8 character
                size_t len = ch < 0x800 ? 2 : (ch < 0x10000 ? 3 : 4);
                value.append(source_.substr(start - len, len));
            }
        }
    }

    return std::unexpected(LexError{
        LexError::Type::UNTERMINATED_STRING,
        "Unterminated string literal",
        current_location()
    });
}

// Scan character literal
std::expected<Token, LexError> Lexer::scan_character() {
    skip_char(); // Skip opening quote

    auto ch_result = peek_char();
    if (!ch_result) return std::unexpected(ch_result.error());

    char32_t ch = ch_result.value();

    if (ch == '\\') {
        skip_char();
        auto escaped = parse_escape_sequence();
        if (!escaped) return std::unexpected(escaped.error());
        ch = escaped.value();
    } else if (ch == '\'') {
        return std::unexpected(LexError{
            LexError::Type::INVALID_CHARACTER_LITERAL,
            "Empty character literal",
            current_location()
        });
    } else {
        skip_char();
    }

    // Expect closing quote
    auto close_result = advance_char();
    if (!close_result || close_result.value() != '\'') {
        return std::unexpected(LexError{
            LexError::Type::INVALID_CHARACTER_LITERAL,
            "Unterminated character literal",
            current_location()
        });
    }

    return make_token(TokenType::YCHARACTER, ch);
}

// Scan symbol
std::expected<Token, LexError> Lexer::scan_symbol() {
    skip_char(); // Skip colon

    // Symbol can be an identifier or operator
    auto ch_result = peek_char();
    if (!ch_result) return std::unexpected(ch_result.error());

    if (is_identifier_start(ch_result.value())) {
        while (!is_at_end()) {
            auto next = peek_char();
            if (!next || !is_identifier_continue(next.value())) break;
            skip_char();
        }
    } else {
        // Allow operator symbols like :+, :==, etc.
        while (!is_at_end()) {
            auto next = peek_char();
            if (!next || !is_operator_char(next.value())) break;
            skip_char();
        }
    }

    std::string_view symbol_name = source_.substr(token_start_ + 1, current_ - token_start_ - 1);
    return make_token(TokenType::YSYMBOL, symbol_name);
}

// Match helper
bool Lexer::match(char32_t expected) {
    if (is_at_end()) return false;

    auto ch_result = peek_char();
    if (!ch_result || ch_result.value() != expected) return false;

    skip_char();
    return true;
}

// Scan operator
std::expected<Token, LexError> Lexer::scan_operator(char32_t first_char) {
    switch (first_char) {
        case '+':
            if (match('+')) return make_token(TokenType::YJOIN);
            return make_token(TokenType::YPLUS);

        case '-':
            if (match('>')) return make_token(TokenType::YARROW);
            if (match('-')) return make_token(TokenType::YREMOVE);
            if (match('|')) return make_token(TokenType::YPREPEND);
            return make_token(TokenType::YMINUS);

        case '*':
            if (match('*')) return make_token(TokenType::YPOWER);
            return make_token(TokenType::YSTAR);

        case '/':
            return make_token(TokenType::YSLASH);

        case '%':
            return make_token(TokenType::YPERCENT);

        case '=':
            if (match('=')) return make_token(TokenType::YEQ);
            if (match('>')) return make_token(TokenType::YFAT_ARROW);
            return make_token(TokenType::YASSIGN);

        case '!':
            if (match('=')) return make_token(TokenType::YNEQ);
            return make_token(TokenType::YNOT);

        case '<':
            if (match('=')) return make_token(TokenType::YLTE);
            if (match('<')) return make_token(TokenType::YLEFT_SHIFT);
            if (match('|')) return make_token(TokenType::YPIPE_LEFT);
            return make_token(TokenType::YLT);

        case '>':
            if (match('=')) return make_token(TokenType::YGTE);
            if (match('>')) {
                if (match('>')) return make_token(TokenType::YZERO_FILL_RIGHT_SHIFT);
                return make_token(TokenType::YRIGHT_SHIFT);
            }
            return make_token(TokenType::YGT);

        case '&':
            if (match('&')) return make_token(TokenType::YAND);
            return make_token(TokenType::YBIT_AND);

        case '|':
            if (match('|')) return make_token(TokenType::YOR);
            if (match('>')) return make_token(TokenType::YPIPE_RIGHT);
            if (match('-')) return make_token(TokenType::YAPPEND);
            return make_token(TokenType::YPIPE);

        case '^':
            return make_token(TokenType::YBIT_XOR);

        case '~':
            return make_token(TokenType::YBIT_NOT);

        case '@':
            return make_token(TokenType::YAT);

        default:
            return std::unexpected(LexError{
                LexError::Type::INVALID_CHARACTER,
                std::format("Unexpected operator character '{}'", static_cast<char>(first_char)),
                current_location()
            });
    }
}

// Main token scanning
std::expected<Token, LexError> Lexer::scan_token() {
    skip_whitespace_and_comments();

    if (is_at_end()) {
        mark_token_start();  // Mark position for EOF
        return make_token(TokenType::YEOF_TOKEN);
    }

    mark_token_start();

    auto ch_result = advance_char();
    if (!ch_result) return std::unexpected(ch_result.error());

    char32_t ch = ch_result.value();

    // Special case for single underscore
    if (ch == '_') {
        // Check if it's just a single underscore or part of an identifier
        if (is_at_end() || !is_identifier_continue(peek_char().value_or(0))) {
            return make_token(TokenType::YUNDERSCORE);
        }
        // Otherwise, it's the start of an identifier
        return scan_identifier();
    }

    // Identifiers and keywords
    if (is_identifier_start(ch)) {
        return scan_identifier();
    }

    // Numbers
    if (is_digit(ch)) {
        current_ = token_start_ + 1; // Reset to after first char (safe for digits which are always 1 byte)
        return scan_number();
    }

    // Single character tokens
    switch (ch) {
        case '(': return make_token(TokenType::YLPAREN);
        case ')': return make_token(TokenType::YRPAREN);
        case '[': return make_token(TokenType::YLBRACKET);
        case ']': return make_token(TokenType::YRBRACKET);
        case '{': return make_token(TokenType::YLBRACE);
        case '}': return make_token(TokenType::YRBRACE);
        case ',': return make_token(TokenType::YCOMMA);
        case ';': return make_token(TokenType::YSEMICOLON);
        case '.':
            if (match('.')) return make_token(TokenType::YDOTDOT);
            return make_token(TokenType::YDOT);
        case '\\': return make_token(TokenType::YBACKSLASH);
        case '"':
            current_ = token_start_; // Reset for string scanning
            return scan_string();
        case '\'':
            current_ = token_start_; // Reset for character scanning
            return scan_character();
        case ':':
            // Could be COLON, CONS (::), CONS_RIGHT (:>), or SYMBOL
            if (!is_at_end()) {
                auto next = peek_char();
                if (next) {
                    if (next.value() == ':') {
                        skip_char();
                        return make_token(TokenType::YCONS);
                    } else if (next.value() == '>') {
                        skip_char();
                        return make_token(TokenType::YCONS_RIGHT);
                    } else if (is_identifier_start(next.value()) || is_operator_char(next.value())) {
                        current_ = token_start_; // Reset for symbol scanning
                        return scan_symbol();
                    }
                }
            }
            return make_token(TokenType::YCOLON);

        // Operators
        case '+': case '-': case '*': case '/': case '%':
        case '=': case '!': case '<': case '>':
        case '&': case '|': case '^': case '~': case '@':
            current_ = token_start_ + 1; // Reset to after first char
            return scan_operator(ch);

        default:
            return std::unexpected(LexError{
                LexError::Type::INVALID_CHARACTER,
                std::format("Unexpected character '{}'", static_cast<char>(ch)),
                current_location()
            });
    }
}

// Public interface
std::expected<Token, LexError> Lexer::next_token() {
    return scan_token();
}

std::expected<Token, LexError> Lexer::peek_token() {
    // Save state
    size_t saved_current = current_;
    size_t saved_line = line_;
    size_t saved_column = column_;

    auto token = scan_token();

    // Restore state
    current_ = saved_current;
    line_ = saved_line;
    column_ = saved_column;

    return token;
}

std::expected<std::vector<Token>, std::vector<LexError>> Lexer::tokenize() {
    std::vector<Token> tokens;
    std::vector<LexError> errors;

    while (true) {
        auto token_result = next_token();
        if (token_result) {
            tokens.push_back(token_result.value());
            if (token_result.value().type == TokenType::YEOF_TOKEN) {
                break;
            }
        } else {
            errors.push_back(token_result.error());
            // Try to recover by skipping the problematic character
            if (!is_at_end()) {
                skip_char();
            }
        }
    }

    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }

    return tokens;
}

// Token type to string conversion
std::string_view token_type_to_string(TokenType type) noexcept {
    switch (type) {
        case TokenType::YINTEGER: return "INTEGER";
        case TokenType::YFLOAT: return "FLOAT";
        case TokenType::YSTRING: return "STRING";
        case TokenType::YCHARACTER: return "CHARACTER";
        case TokenType::YSYMBOL: return "SYMBOL";
        case TokenType::YTRUE: return "TRUE";
        case TokenType::YFALSE: return "FALSE";
        case TokenType::YUNIT: return "UNIT";
        case TokenType::YIDENTIFIER: return "IDENTIFIER";
        case TokenType::YMODULE: return "MODULE";
        case TokenType::YIMPORT: return "IMPORT";
        case TokenType::YFROM: return "FROM";
        case TokenType::YAS: return "AS";
        case TokenType::YEXPORT: return "EXPORT";
        case TokenType::YLET: return "LET";
        case TokenType::YIN: return "IN";
        case TokenType::YIF: return "IF";
        case TokenType::YTHEN: return "THEN";
        case TokenType::YELSE: return "ELSE";
        case TokenType::YCASE: return "CASE";
        case TokenType::YOF: return "OF";
        case TokenType::YDO: return "DO";
        case TokenType::YEND: return "END";
        case TokenType::YTRY: return "TRY";
        case TokenType::YCATCH: return "CATCH";
        case TokenType::YRAISE: return "RAISE";
        case TokenType::YWITH: return "WITH";
        case TokenType::YFUN: return "FUN";
        case TokenType::YLAMBDA: return "LAMBDA";
        case TokenType::YRECORD: return "RECORD";
        case TokenType::YTYPE: return "TYPE";
        case TokenType::YPLUS: return "PLUS";
        case TokenType::YMINUS: return "MINUS";
        case TokenType::YSTAR: return "STAR";
        case TokenType::YSLASH: return "SLASH";
        case TokenType::YPERCENT: return "PERCENT";
        case TokenType::YPOWER: return "POWER";
        case TokenType::YEQ: return "EQ";
        case TokenType::YNEQ: return "NEQ";
        case TokenType::YLT: return "LT";
        case TokenType::YGT: return "GT";
        case TokenType::YLTE: return "LTE";
        case TokenType::YGTE: return "GTE";
        case TokenType::YAND: return "AND";
        case TokenType::YOR: return "OR";
        case TokenType::YNOT: return "NOT";
        case TokenType::YBIT_AND: return "BIT_AND";
        case TokenType::YBIT_OR: return "BIT_OR";
        case TokenType::YBIT_XOR: return "BIT_XOR";
        case TokenType::YBIT_NOT: return "BIT_NOT";
        case TokenType::YLEFT_SHIFT: return "LEFT_SHIFT";
        case TokenType::YRIGHT_SHIFT: return "RIGHT_SHIFT";
        case TokenType::YZERO_FILL_RIGHT_SHIFT: return "ZERO_FILL_RIGHT_SHIFT";
        case TokenType::YASSIGN: return "ASSIGN";
        case TokenType::YARROW: return "ARROW";
        case TokenType::YFAT_ARROW: return "FAT_ARROW";
        case TokenType::YLPAREN: return "LPAREN";
        case TokenType::YRPAREN: return "RPAREN";
        case TokenType::YLBRACKET: return "LBRACKET";
        case TokenType::YRBRACKET: return "RBRACKET";
        case TokenType::YLBRACE: return "LBRACE";
        case TokenType::YRBRACE: return "RBRACE";
        case TokenType::YCOMMA: return "COMMA";
        case TokenType::YSEMICOLON: return "SEMICOLON";
        case TokenType::YCOLON: return "COLON";
        case TokenType::YDOT: return "DOT";
        case TokenType::YDOTDOT: return "DOTDOT";
        case TokenType::YPIPE: return "PIPE";
        case TokenType::YAT: return "AT";
        case TokenType::YUNDERSCORE: return "UNDERSCORE";
        case TokenType::YBACKSLASH: return "BACKSLASH";
        case TokenType::YCONS: return "CONS";
        case TokenType::YPIPE_LEFT: return "PIPE_LEFT";
        case TokenType::YPIPE_RIGHT: return "PIPE_RIGHT";
        case TokenType::YJOIN: return "JOIN";
        case TokenType::YREMOVE: return "REMOVE";
        case TokenType::YPREPEND: return "PREPEND";
        case TokenType::YAPPEND: return "APPEND";
        case TokenType::YEOF_TOKEN: return "EOF";
        case TokenType::YNEWLINE: return "NEWLINE";
        case TokenType::YERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace yona::lexer
