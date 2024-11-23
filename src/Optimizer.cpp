//
// Created by akovari on 17.11.24.
//

#include "Optimizer.h"
#include "ast.h"

namespace yona::compiler
{
    using namespace std;
    AstNode Optimizer::visit(const AstNode& node) { return node; }

}
