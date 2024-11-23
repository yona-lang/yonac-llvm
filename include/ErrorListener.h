//
// Created by akovari on 18.11.24.
//

#pragma once
#include <ConsoleErrorListener.h>

namespace yona::parser
{
    using namespace antlr4;
    class ErrorListener : public ConsoleErrorListener
    {
    public:
        ErrorListener() = default;
        ~ErrorListener() override = default;
        void syntaxError(Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line, size_t charPositionInLine,
                         const std::string& msg, std::exception_ptr e);
    };
}
