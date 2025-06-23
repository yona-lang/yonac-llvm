#include <algorithm>
#include <expected>

#include "Lexer.h"

namespace yona::lexer {

// Keyword lookup table
const std::unordered_map<std::string_view, TokenType> Lexer::keywords_ = {
    {"module", TokenType::MODULE},
    {"import", TokenType::IMPORT},
    {"from", TokenType::FROM},
    {"as", TokenType::AS},
    {"exports", TokenType::EXPORT},
    {"let", TokenType::LET},
    {"in", TokenType::IN},
    {"if", TokenType::IF},
    {"then", TokenType::THEN},
    {"else", TokenType::ELSE},
    {"case", TokenType::CASE},
    {"of", TokenType::OF},
    {"do", TokenType::DO},
    {"end", TokenType::END},
    {"try", TokenType::TRY},
    {"catch", TokenType::CATCH},
    {"raise", TokenType::RAISE},
    {"with", TokenType::WITH},
    {"fun", TokenType::FUN},
    {"lambda", TokenType::LAMBDA},
    {"record", TokenType::RECORD},
    {"type", TokenType::TYPE},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
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
    return type >= TokenType::MODULE && type <= TokenType::TYPE;
}

bool Token::is_operator() const noexcept {
    return type >= TokenType::PLUS && type <= TokenType::JOIN;
}

bool Token::is_literal() const noexcept {
    return type >= TokenType::INTEGER && type <= TokenType::UNIT;
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
        TokenType::ERROR,
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
    
    return make_token(TokenType::IDENTIFIER, lexeme);
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
        } else {
            break;
        }
    }
    
    std::string_view lexeme = current_lexeme();
    
    // Remove underscores for parsing
    std::string clean_lexeme;
    for (char c : lexeme) {
        if (c != '_') clean_lexeme += c;
    }
    
    if (has_dot || has_exp) {
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
        return make_token(TokenType::FLOAT, value);
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
        return make_token(TokenType::INTEGER, value);
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
            return make_token(TokenType::STRING, std::move(value));
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
    
    return make_token(TokenType::CHARACTER, ch);
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
    return make_token(TokenType::SYMBOL, symbol_name);
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
            if (match('+')) return make_token(TokenType::JOIN);
            return make_token(TokenType::PLUS);
            
        case '-':
            if (match('>')) return make_token(TokenType::ARROW);
            if (match('-')) return make_token(TokenType::REMOVE);
            if (match('|')) return make_token(TokenType::PREPEND);
            return make_token(TokenType::MINUS);
            
        case '*':
            if (match('*')) return make_token(TokenType::POWER);
            return make_token(TokenType::STAR);
            
        case '/':
            return make_token(TokenType::SLASH);
            
        case '%':
            return make_token(TokenType::PERCENT);
            
        case '=':
            if (match('=')) return make_token(TokenType::EQ);
            if (match('>')) return make_token(TokenType::FAT_ARROW);
            return make_token(TokenType::ASSIGN);
            
        case '!':
            if (match('=')) return make_token(TokenType::NEQ);
            return make_token(TokenType::NOT);
            
        case '<':
            if (match('=')) return make_token(TokenType::LTE);
            if (match('<')) return make_token(TokenType::LEFT_SHIFT);
            if (match('|')) return make_token(TokenType::PIPE_LEFT);
            return make_token(TokenType::LT);
            
        case '>':
            if (match('=')) return make_token(TokenType::GTE);
            if (match('>')) {
                if (match('>')) return make_token(TokenType::ZERO_FILL_RIGHT_SHIFT);
                return make_token(TokenType::RIGHT_SHIFT);
            }
            return make_token(TokenType::GT);
            
        case '&':
            if (match('&')) return make_token(TokenType::AND);
            return make_token(TokenType::BIT_AND);
            
        case '|':
            if (match('|')) return make_token(TokenType::OR);
            if (match('>')) return make_token(TokenType::PIPE_RIGHT);
            if (match('-')) return make_token(TokenType::APPEND);
            return make_token(TokenType::PIPE);
            
        case '^':
            return make_token(TokenType::BIT_XOR);
            
        case '~':
            return make_token(TokenType::BIT_NOT);
            
        case '@':
            return make_token(TokenType::AT);
            
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
        return make_token(TokenType::EOF_TOKEN);
    }
    
    mark_token_start();
    
    auto ch_result = advance_char();
    if (!ch_result) return std::unexpected(ch_result.error());
    
    char32_t ch = ch_result.value();
    
    // Special case for single underscore
    if (ch == '_') {
        // Check if it's just a single underscore or part of an identifier
        if (is_at_end() || !is_identifier_continue(peek_char().value_or(0))) {
            return make_token(TokenType::UNDERSCORE);
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
        case '(': return make_token(TokenType::LPAREN);
        case ')': return make_token(TokenType::RPAREN);
        case '[': return make_token(TokenType::LBRACKET);
        case ']': return make_token(TokenType::RBRACKET);
        case '{': return make_token(TokenType::LBRACE);
        case '}': return make_token(TokenType::RBRACE);
        case ',': return make_token(TokenType::COMMA);
        case ';': return make_token(TokenType::SEMICOLON);
        case '.':
            if (match('.')) return make_token(TokenType::DOTDOT);
            return make_token(TokenType::DOT);
        case '\\': return make_token(TokenType::BACKSLASH);
        case '"': 
            current_ = token_start_; // Reset for string scanning
            return scan_string();
        case '\'':
            current_ = token_start_; // Reset for character scanning
            return scan_character();
        case ':':
            // Could be COLON, CONS (::), or SYMBOL
            if (!is_at_end()) {
                auto next = peek_char();
                if (next) {
                    if (next.value() == ':') {
                        skip_char();
                        return make_token(TokenType::CONS);
                    } else if (is_identifier_start(next.value()) || is_operator_char(next.value())) {
                        current_ = token_start_; // Reset for symbol scanning
                        return scan_symbol();
                    }
                }
            }
            return make_token(TokenType::COLON);
            
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
            if (token_result.value().type == TokenType::EOF_TOKEN) {
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
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::CHARACTER: return "CHARACTER";
        case TokenType::SYMBOL: return "SYMBOL";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::UNIT: return "UNIT";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::MODULE: return "MODULE";
        case TokenType::IMPORT: return "IMPORT";
        case TokenType::FROM: return "FROM";
        case TokenType::AS: return "AS";
        case TokenType::EXPORT: return "EXPORT";
        case TokenType::LET: return "LET";
        case TokenType::IN: return "IN";
        case TokenType::IF: return "IF";
        case TokenType::THEN: return "THEN";
        case TokenType::ELSE: return "ELSE";
        case TokenType::CASE: return "CASE";
        case TokenType::OF: return "OF";
        case TokenType::DO: return "DO";
        case TokenType::END: return "END";
        case TokenType::TRY: return "TRY";
        case TokenType::CATCH: return "CATCH";
        case TokenType::RAISE: return "RAISE";
        case TokenType::WITH: return "WITH";
        case TokenType::FUN: return "FUN";
        case TokenType::LAMBDA: return "LAMBDA";
        case TokenType::RECORD: return "RECORD";
        case TokenType::TYPE: return "TYPE";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::POWER: return "POWER";
        case TokenType::EQ: return "EQ";
        case TokenType::NEQ: return "NEQ";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::LTE: return "LTE";
        case TokenType::GTE: return "GTE";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::BIT_AND: return "BIT_AND";
        case TokenType::BIT_OR: return "BIT_OR";
        case TokenType::BIT_XOR: return "BIT_XOR";
        case TokenType::BIT_NOT: return "BIT_NOT";
        case TokenType::LEFT_SHIFT: return "LEFT_SHIFT";
        case TokenType::RIGHT_SHIFT: return "RIGHT_SHIFT";
        case TokenType::ZERO_FILL_RIGHT_SHIFT: return "ZERO_FILL_RIGHT_SHIFT";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::ARROW: return "ARROW";
        case TokenType::FAT_ARROW: return "FAT_ARROW";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COLON: return "COLON";
        case TokenType::DOT: return "DOT";
        case TokenType::DOTDOT: return "DOTDOT";
        case TokenType::PIPE: return "PIPE";
        case TokenType::AT: return "AT";
        case TokenType::UNDERSCORE: return "UNDERSCORE";
        case TokenType::BACKSLASH: return "BACKSLASH";
        case TokenType::CONS: return "CONS";
        case TokenType::PIPE_LEFT: return "PIPE_LEFT";
        case TokenType::PIPE_RIGHT: return "PIPE_RIGHT";
        case TokenType::JOIN: return "JOIN";
        case TokenType::REMOVE: return "REMOVE";
        case TokenType::PREPEND: return "PREPEND";
        case TokenType::APPEND: return "APPEND";
        case TokenType::EOF_TOKEN: return "EOF";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace yona::lexer