//
// Created by Adam Kovari on 17.12.2024.
//

#include "Parser.h"

#include <boost/log/trivial.hpp>

#include "ErrorListener.h"
#include "ErrorStrategy.h"
#include "YonaLexer.h"
#include "YonaParser.h"
#include "YonaVisitor.h"

namespace yona::parser
{
    ParseResult Parser::parse_input(istream& stream)
    {
        ANTLRInputStream input(stream);
        YonaLexer lexer(&input);
        CommonTokenStream tokens(&lexer);
        YonaParser parser(&tokens);
        auto ast_ctx = AstContext();
        ErrorListener error_listener(ast_ctx);

        lexer.removeErrorListeners();
        parser.removeErrorListeners();
        lexer.addErrorListener(&error_listener);
        parser.addErrorListener(&error_listener);
        parser.setErrorHandler(make_shared<ErrorStrategy>(ast_ctx));

        YonaParser::InputContext* tree = parser.input();
        BOOST_LOG_TRIVIAL(debug) << "Parse tree: " << tree->toStringTree();

        if (ast_ctx.hasErrors())
        {
            return ParseResult{ !ast_ctx.hasErrors(), nullptr, nullptr, ast_ctx };
        }

        YonaVisitor yona_visitor(std::move(module_import_queue));
        any ast = yona_visitor.visitInput(tree);
        auto node = any_cast<expr_wrapper>(ast).get_node<AstNode>();

        auto type = node->infer_type(ast_ctx);

        return ParseResult{ !ast_ctx.hasErrors(), node, type, ast_ctx };
    }

}
