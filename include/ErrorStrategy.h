//
// Created by Adam Kovari on 16.12.2024.
//

#pragma once

#include <antlr4-runtime.h>

#include "common.h"

namespace yona::parser
{
    using namespace antlr4;

    class ErrorStrategy final : public DefaultErrorStrategy
    {
    private:
        AstContext& ast_ctx;

    public:
        explicit ErrorStrategy(AstContext& ast_ctx) : ast_ctx(ast_ctx) {}
        ~ErrorStrategy() override = default;

    protected:
        void handleRecognitionException(IntervalSet et, const std::string& message, const std::exception& cause,
                                        int line);

        void reportNoViableAlternative(Parser* parser, const NoViableAltException& e);
    };
}
