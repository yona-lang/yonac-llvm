//
// Created by Adam Kovari on 17.12.2024.
//

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <expected>
#include <unordered_map>

#include "yona_export.h"
#include "ast.h"
#include "common.h"
#include "types.h"
#include "Lexer.h"

namespace yona::parser {
using namespace std;
using namespace ast;
using namespace lexer;

// Parser configuration for performance tuning
struct ParserConfig {
    size_t max_lookahead = 3;           // Maximum tokens to look ahead
    size_t initial_ast_pool_size = 1024; // Pre-allocate AST nodes
    bool enable_error_recovery = true;   // Try to recover from parse errors
    bool enable_optimizations = true;    // Enable parsing optimizations
};

// Parse error with detailed information
struct ParseError {
    enum class Type {
        UNEXPECTED_TOKEN,
        UNEXPECTED_EOF,
        INVALID_SYNTAX,
        INVALID_NUMBER,
        INVALID_STRING,
        INVALID_PATTERN,
        MISSING_TOKEN,
        AMBIGUOUS_PARSE
    };

    Type type;
    string message;
    SourceLocation location;
    optional<TokenType> expected_token;
    optional<TokenType> actual_token;

    [[nodiscard]] string format() const;
};

// Forward declaration for implementation
class ParserImpl;

// Result type for old interface compatibility
struct ParseResult {
    bool success;
    shared_ptr<AstNode> node;
    compiler::types::Type type;
    AstContext ast_ctx;

    ParseResult() : success(false), node(nullptr), type(nullptr) {}
    ParseResult(bool s, shared_ptr<AstNode> n, compiler::types::Type t, AstContext ctx)
        : success(s), node(std::move(n)), type(t), ast_ctx(std::move(ctx)) {}
};

// High-performance recursive descent parser
class YONA_API Parser {
private:
    unique_ptr<ParserImpl> impl_;
    ModuleImportQueue module_import_queue_;

public:
    explicit Parser(ParserConfig config = {});
    ~Parser();

    // Disable copy but allow move
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    Parser(Parser&&) noexcept;
    Parser& operator=(Parser&&) noexcept;

    // Modern parsing interface
    [[nodiscard]] expected<unique_ptr<ModuleExpr>, vector<ParseError>>
        parse_module(string_view source, string_view filename = "<input>");

    [[nodiscard]] expected<unique_ptr<ExprNode>, vector<ParseError>>
        parse_expression(string_view source, string_view filename = "<input>");

    // Legacy interface for compatibility
    ParseResult parse_input(const vector<string>& module_name);
    ParseResult parse_input(istream& stream);
};

} // namespace yona::parser
