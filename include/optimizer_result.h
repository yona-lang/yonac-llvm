#pragma once

namespace yona::ast {
    class AstNode;
}

namespace yona::compiler {

// Result type for optimizer - returns optimized AST nodes
struct OptimizerResult {
    ast::AstNode* node;

    OptimizerResult() : node(nullptr) {}
    explicit OptimizerResult(ast::AstNode* n) : node(n) {}
};

} // namespace yona::compiler
