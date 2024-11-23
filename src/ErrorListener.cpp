//
// Created by akovari on 18.11.24.
//

#include <boost/log/trivial.hpp>

#include "ErrorListener.h"

#include <Token.h>

namespace yona::parser
{
    using namespace std;
    void ErrorListener::syntaxError(Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                                    size_t charPositionInLine, const std::string& msg, std::exception_ptr e)
    {
        BOOST_LOG_TRIVIAL(error) << "Syntax error at " << line << ":" << charPositionInLine << ": "
                                 << offendingSymbol->getText() << endl
                                 << "\t >>> " << msg;
    }
}
