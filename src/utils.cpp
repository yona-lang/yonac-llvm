//
// Created by akovari on 15.12.24.
//
#include "utils.h"


#include <antlr4-runtime.h>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include "ErrorListener.h"
#include "YonaLexer.h"
#include "YonaParser.h"
#include "YonaVisitor.h"

namespace yona
{
    using namespace antlr4;
    using namespace std;

    ParseResult parse_input(istream& stream)
    {
        ANTLRInputStream input(stream);
        YonaLexer lexer(&input);
        CommonTokenStream tokens(&lexer);
        YonaParser parser(&tokens);
        auto ast_ctx = AstContext();
        parser::ErrorListener error_listener(ast_ctx);

        lexer.removeErrorListeners();
        parser.removeErrorListeners();
        lexer.addErrorListener(&error_listener);
        parser.addErrorListener(&error_listener);

        YonaParser::InputContext* tree = parser.input();
        BOOST_LOG_TRIVIAL(debug) << "Parse tree: " << tree->toStringTree();

        YonaVisitor yona_visitor;
        any ast = yona_visitor.visitInput(tree);
        auto node = any_cast<expr_wrapper>(ast).get_node<AstNode>();

        auto type = node->infer_type(ast_ctx);

        return ParseResult{ ast_ctx.getErrors().empty(), node, type, ast_ctx };
    }
}
