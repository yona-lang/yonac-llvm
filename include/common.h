//
// Created by akovari on 28.11.24.
//

#pragma once

#include "ast.h"

namespace yona
{
    using namespace std;
    using namespace ast;

    struct expr_wrapper
    {
    private:
        AstNode* node;

    public:
        explicit expr_wrapper(AstNode* node) : node(node) {}

        template <typename T>
            requires derived_from<T, AstNode>
        T* get_node()
        {
            return static_cast<T*>(node);
        }
    };
};
