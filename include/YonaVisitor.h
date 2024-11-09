#pragma once

#include <iostream>
#include "YonaParserBaseVisitor.h"
#include "ast.h"

namespace yonac {
    using namespace std;
    using namespace ast;

    class YonaParserVisitor : public YonaParserBaseVisitor<unique_ptr<AstNode>> {
    };
}
