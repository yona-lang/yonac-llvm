//
// Created by akovari on 18.11.24.
//

#include <Token.h>

#include "ErrorListener.h"
#include "common.h"

namespace yona::parser {
using namespace std;
void ErrorListener::syntaxError(Recognizer *recognizer, Token *offendingSymbol, size_t line, size_t charPositionInLine, const std::string &msg,
                                std::exception_ptr e) {
  ast_ctx.addError(yona_error(SourceInfo(*offendingSymbol, *recognizer), yona_error::SYNTAX, msg));
}
} // namespace yona::parser
