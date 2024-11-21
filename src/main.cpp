﻿// yonac-llvm.cpp : Defines the entry point for the application.
//

#include <antlr4-runtime.h>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include "YonaLexer.h"
#include "YonaParser.h"
#include "YonaVisitor.h"
#include "interpreter.h"
#include "main.h"

#include "ErrorListener.h"
#include "optimizer.h"

namespace po = boost::program_options;
using namespace antlr4;
using namespace yona;
using namespace std;

void process_program_options(const int argc, const char* const argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()("help", "Show brief usage message");

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
    auto optimized_ast = optimizer.visit(any_cast<expr_wrapper>(ast).get_node<AstNode>());
    interpreter.visit(optimized_ast);

    stream.close();
    return 0;
}
