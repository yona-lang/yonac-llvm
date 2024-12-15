//
// Created by akovari on 18.11.24.
//

#include <Token.h>

#include "ErrorListener.h"
#include "common.h"

namespace yona::parser
{
    using namespace std;
    void ErrorListener::syntaxError(Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                                    size_t charPositionInLine, const std::string& msg, std::exception_ptr e)
    {
        ast_ctx.addError(YonaError(TokenLocation(line, charPositionInLine, line,
                                                 charPositionInLine + offendingSymbol->getText().length(),
                                                 offendingSymbol->getText(), e),
                                   YonaError::SYNTAX, msg));
    }
}
