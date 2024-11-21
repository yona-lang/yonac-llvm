//
// Created by akovari on 17.11.24.
//

#pragma once

#include "ast.h"


namespace yona::compiler
{
    using namespace std;
    using namespace yona::ast;

    class Optimizer : public AstVisitor<AstNode>
    {
    public:
        AstNode visit(const AstNode& node) override;
    };
}
