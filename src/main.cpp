// main.cpp : Defines the entry point for the application.

#include <antlr4-runtime.h>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include "Interpreter.h"
#include "YonaLexer.h"
#include "YonaParser.h"
#include "YonaVisitor.h"
#include "main.h"

#include "ErrorListener.h"
#include "Optimizer.h"

namespace po = boost::program_options;
using namespace antlr4;
using namespace yona;
using namespace std;

void process_program_options(const int argc, const char* const argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()("help", "Show brief usage message");
    desc.add_options()("compile", "Compile the input file");

    po::variables_map args;

    try
    {
        po::store(po::parse_command_line(argc, argv, desc), args);
    }
    catch (po::error const& e)
    {
        cerr << e.what() << '\n';
        exit(EXIT_FAILURE);
    }
    po::notify(args);
}

int main(const int argc, const char* argv[])
{
    process_program_options(argc, argv);

    string fname;
#ifdef NDEBUG
    fname = argc > 1 ? argv[1] : "test.yona";
#else
    fname = argv[1];
#endif
    ifstream stream(fname);

    ANTLRInputStream input(stream);
    YonaLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    YonaParser parser(&tokens);
    parser::ErrorListener error_listener;

    lexer.removeErrorListeners();
    parser.removeErrorListeners();
    lexer.addErrorListener(&error_listener);
    parser.addErrorListener(&error_listener);

    BOOST_LOG_TRIVIAL(info) << "Yona Parser started for input file: " << fname;
    YonaParser::InputContext* tree = parser.input();
    BOOST_LOG_TRIVIAL(debug) << "Parse tree: " << tree->toStringTree();

    YonaVisitor yona_visitor;
    compiler::Optimizer optimizer;
    interp::Interpreter interpreter;
    auto ast = yona_visitor.visitInput(tree);
    auto typeCtx = TypeInferenceContext();
    auto type = any_cast<expr_wrapper>(ast).get_node<AstNode>()->infer_type(typeCtx);

    if (!typeCtx.getErrors().empty())
    {
        BOOST_LOG_TRIVIAL(error) << typeCtx.getErrors().size() << " errors found. Please fix them and re-run.";
        for (auto err : typeCtx.getErrors())
        {
            BOOST_LOG_TRIVIAL(error) << "Type error at " << err.token.getStart()->getLine() << ":"
                                     << err.token.getStart()->getCharPositionInLine() << err.message;
        }
        return 1;
    }

    BOOST_LOG_TRIVIAL(trace) << "Result expression type: " << typeid(type).name() << endl;

    auto optimized_ast = any_cast<expr_wrapper>(ast).get_node<AstNode>()->accept(optimizer);
    auto result = any_cast<expr_wrapper>(optimized_ast).get_node<AstNode>()->accept(interpreter);
    cout << result.type().name() << endl;
    delete any_cast<expr_wrapper>(optimized_ast).get_node<AstNode>();

    stream.close();
    return 0;
}
