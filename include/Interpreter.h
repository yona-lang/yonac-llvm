//
// Created by akovari on 17.11.24.
//

#pragma once

#include "ast.h"
#include "interpretation.h"

namespace yona::interp
{
    using namespace std;
    using namespace ast;

    class Interpreter : public AstVisitor<result_t>
    {
    public:
        result_t visit(const AstNode& node) override;
        result_t visit(const BinaryOpExpr& node);
        result_t visit(const AddExpr& node);
    };

} // yonac::interp
