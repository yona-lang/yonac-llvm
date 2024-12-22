//
// Created by Adam Kovari on 17.12.2024.
//

#pragma once

#include "ast.h"
#include "common.h"
#include "types.h"

namespace yona::parser
{
  using namespace std;
  using namespace ast;

  struct ParseResult
  {
    bool success;
    AstNode* node;
    Type type;
    AstContext ast_ctx;

    ~ParseResult() { delete node; }
  };

  class Parser
  {
  private:
    ModuleImportQueue module_import_queue;

  public:
    ParseResult parse_input(istream& stream);
  };
}
