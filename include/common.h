//
// Created by akovari on 28.11.24.
//

#pragma once

#include <antlr4-runtime.h>
#include <string>

#include "colors.h"

namespace yona
{
    using namespace std;
    using namespace antlr4;

    using SourceContext = const ParserRuleContext&;
    using ModuleImportQueue = shared_ptr<queue<vector<string>>>;

    inline struct YonaEnvironment
    {
        vector<string> search_paths;
        bool compile_mode;
    } YONA_ENVIRONMENT;

    struct TokenLocation final
    {
        size_t start_line;
        size_t start_col;
        size_t stop_line;
        size_t stop_col;
        string text;

        TokenLocation(SourceContext token) :
            start_line(token.getStart()->getLine()), start_col(token.getStart()->getCharPositionInLine()),
            stop_line(token.getStop()->getLine()), stop_col(token.getStop()->getCharPositionInLine()),
            text(token.getStart()->getText())
        {
        }

        TokenLocation(antlr4::Token& token, Recognizer& recognizer) :
            start_line(token.getLine()), start_col(token.getCharPositionInLine()), stop_line(token.getLine()),
            stop_col(token.getText().length() + start_col), text(recognizer.getTokenErrorDisplay(&token))
        {
        }

        TokenLocation(size_t start_line, size_t start_col, size_t stop_line, size_t stop_col, string text) :
            start_line(start_line), start_col(start_col), stop_line(stop_line), stop_col(stop_col),
            text(std::move(text))
        {
        }
    };

    inline std::ostream& operator<<(std::ostream& os, const TokenLocation& rhs)
    {
        os << "[" << rhs.start_line << ":" << rhs.start_col << "-" << rhs.stop_line << ":" << rhs.stop_col << "] "
           << rhs.text;
        return os;
    }

    struct YonaError final
    {
        enum Type
        {
            SYNTAX,
            TYPE,
            RUNTIME
        } type;
        TokenLocation source_token;
        string message;

        explicit YonaError(TokenLocation source_token, Type type, string message) :
            type(type), source_token(std::move(source_token)), message(std::move(message))
        {
        }
    };

    inline const char* ErrorTypes[] = { "syntax", "type", "check" };

    inline std::ostream& operator<<(std::ostream& os, const YonaError& rhs)
    {
        os << ANSI_COLOR_RED << "Invalid " << ErrorTypes[rhs.type] << " at " << rhs.source_token << ANSI_COLOR_RESET
           << ":\n"
           << rhs.message;
        return os;
    }

    class AstContext final
    {
    private:
        unordered_multimap<YonaError::Type, YonaError> errors_;

    public:
        void addError(const YonaError& error) { errors_.insert({ error.type, error }); }
        [[nodiscard]] bool hasErrors() const { return !errors_.empty(); }
        [[nodiscard]] const unordered_multimap<YonaError::Type, YonaError>& getErrors() const { return errors_; }
        [[nodiscard]] size_t errorCount() const { return errors_.size(); }
    };
};
