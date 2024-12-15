//
// Created by akovari on 28.11.24.
//

#pragma once

#include <string>
#include <antlr4-runtime.h>

#include "colors.h"

namespace yona
{
    using namespace std;

    using Token = const antlr4::ParserRuleContext&;

    struct TokenLocation final
    {
        size_t start_line;
        size_t start_col;
        size_t stop_line;
        size_t stop_col;
        string text;
        std::exception_ptr e;

        TokenLocation(Token token) :
            start_line(token.getStart()->getLine()), start_col(token.getStart()->getCharPositionInLine()),
            stop_line(token.getStop()->getLine()), stop_col(token.getStop()->getCharPositionInLine()),
            text(token.getStart()->getText())
        {
        }

        TokenLocation(size_t start_line, size_t start_col, size_t stop_line, size_t stop_col, const string& text,
                      std::exception_ptr e) :
            start_line(start_line), start_col(start_col), stop_line(stop_line), stop_col(stop_col), text(text), e(e)
        {
        }
    };

    inline std::ostream& operator<<(std::ostream& os, const TokenLocation& rhs)
    {
        os << "[" << rhs.start_line << ":" << rhs.start_col << "-" << rhs.stop_line << ":" << rhs.stop_col << "] "
           << "'" << rhs.text << "'";
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
            type(type), source_token(source_token), message(std::move(message))
        {
        }
    };

    inline const char* ErrorTypes[] = {"Syntax", "Type", "Runtime"};

    inline std::ostream& operator<<(std::ostream& os, const YonaError& rhs)
    {
        os << ANSI_COLOR_RED << ErrorTypes[rhs.type] << " error at " << rhs.source_token << ANSI_COLOR_RESET << ": " << rhs.message;
        return os;
    }

    class AstContext final
    {
    private:
        vector<YonaError> errors;

    public:
        void addError(const YonaError& error) { errors.push_back(error); }
        [[nodiscard]] const vector<YonaError>& getErrors() const { return errors; }
    };
};
