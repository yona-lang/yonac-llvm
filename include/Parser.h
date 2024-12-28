//
// Created by Adam Kovari on 17.12.2024.
//

#pragma once

#include "ast.h"
#include "common.h"
#include "types.h"

namespace yona::parser {
using namespace std;
using namespace ast;

struct ParseResult {
  bool success;
  shared_ptr<AstNode> node;
  Type type;
  AstContext ast_ctx;
};

class Parser {
private:
  ModuleImportQueue module_import_queue_;

public:
  ParseResult parse_input(const vector<string> &module_name);
  ParseResult parse_input(istream &stream) const;
};
} // namespace yona::parser
