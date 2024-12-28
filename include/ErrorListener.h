//
// Created by akovari on 18.11.24.
//

#pragma once
#include <ConsoleErrorListener.h>

#include "common.h"

namespace yona::parser {
using namespace antlr4;
class ErrorListener : public ConsoleErrorListener {
  AstContext &ast_ctx;

public:
  explicit ErrorListener(AstContext &ast_ctx) : ast_ctx(ast_ctx) {}
  ~ErrorListener() override = default;
  void syntaxError(Recognizer *recognizer, antlr4::Token *offendingSymbol, size_t line, size_t charPositionInLine, const std::string &msg,
                   std::exception_ptr e) override;
};
} // namespace yona::parser
