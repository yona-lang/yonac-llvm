//
// Created by akovari on 15.12.24.
//
#include "utils.h"


#include <antlr4-runtime.h>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "ErrorListener.h"
#include "ErrorStrategy.h"
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
        parser.setErrorHandler(make_shared<parser::ErrorStrategy>(ast_ctx));

        YonaParser::InputContext* tree = parser.input();
        BOOST_LOG_TRIVIAL(debug) << "Parse tree: " << tree->toStringTree();

        if (ast_ctx.hasErrors())
        {
            return ParseResult{ !ast_ctx.hasErrors(), nullptr, nullptr, ast_ctx };
        }

        YonaVisitor yona_visitor;
        any ast = yona_visitor.visitInput(tree);
        auto node = any_cast<expr_wrapper>(ast).get_node<AstNode>();

        auto type = node->infer_type(ast_ctx);

        return ParseResult{ !ast_ctx.hasErrors(), node, type, ast_ctx };
    }

    std::string LookupTranslator::translate(const std::string& input)
    {
        std::string result = input;
        for (const auto& pair : lookupMap)
        {
            size_t pos = 0;
            while ((pos = result.find(pair.first, pos)) != std::string::npos)
            {
                result.replace(pos, pair.first.length(), pair.second);
                pos += pair.second.length();
            }
        }
        return result;
    }

    std::string AggregateTranslator::translate(const std::string& input)
    {
        std::string result = input;
        for (auto& translator : translators)
        {
            result = translator->translate(result);
        }
        return result;
    }

    string unescapeYonaString(const string& rawString) { return UNESCAPE_YONA.translate(rawString); }
}
