//
// Created by akovari on 15.12.24.
//

#pragma once

#include "ast.h"
#include "common.h"

namespace yona
{
    using namespace std;
    using namespace ast;
    using namespace compiler::types;

    struct ParseResult
    {
        bool success;
        AstNode* node;
        Type type;
        AstContext ast_ctx;
    };

    ParseResult parse_input(istream& stream);
}
