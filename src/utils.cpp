//
// Created by akovari on 15.12.24.
//
#include "utils.h"


#include <antlr4-runtime.h>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include "YonaLexer.h"
#include "YonaParser.h"
#include "YonaVisitor.h"
#include "ErrorListener.h"

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
        parser::ErrorListener error_listener;

        lexer.removeErrorListeners();
        parser.removeErrorListeners();
        lexer.addErrorListener(&error_listener);
        parser.addErrorListener(&error_listener);

        YonaParser::InputContext* tree = parser.input();
        BOOST_LOG_TRIVIAL(debug) << "Parse tree: " << tree->toStringTree();

        YonaVisitor yona_visitor;
        any ast = yona_visitor.visitInput(tree);
        auto node = any_cast<expr_wrapper>(ast).get_node<AstNode>();

        auto typeCtx = TypeInferenceContext();
        auto type = node->infer_type(typeCtx);

        return ParseResult{typeCtx.getErrors().empty(), node, type, typeCtx};
    }
}
