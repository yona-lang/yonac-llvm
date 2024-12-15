//
// Created by akovari on 15.12.24.
//

#pragma once

#include "ast.h"

namespace yona
{
    using namespace std;
    using namespace compiler::types;

    struct ParseResult
    {
        bool success;
        ast::AstNode* node;
        Type type;
        TypeInferenceContext type_ctx;
    };

    ParseResult parse_input(istream& stream);
}
