// main.cpp : Defines the entry point for the application.

#include <antlr4-runtime.h>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <iostream>

#include "utils.h"
#include "types.h"
#include "common.h"
#include "Interpreter.h"
#include "Optimizer.h"

namespace po = boost::program_options;
using namespace antlr4;
using namespace yona;
using namespace std;
using namespace yona::compiler::types;

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
    BOOST_LOG_TRIVIAL(info) << "Yona Parser started for input file: " << fname;

    ParseResult parse_result = parse_input(stream);

    if (!parse_result.success)
    {
        BOOST_LOG_TRIVIAL(error) << parse_result.type_ctx.getErrors().size() << " errors found. Please fix them and re-run.";
        for (auto err : parse_result.type_ctx.getErrors())
        {
            BOOST_LOG_TRIVIAL(error) << err;
        }
        return 1;
    }

    BOOST_LOG_TRIVIAL(trace) << "Result expression type: " << typeid(parse_result.type).name() << endl;

    compiler::Optimizer optimizer;
    interp::Interpreter interpreter;

    auto optimized_ast = any_cast<expr_wrapper>(parse_result.node->accept(optimizer)).get_node<AstNode>();
    auto result = optimized_ast->accept(interpreter);
    cout << result.type().name() << endl;
    delete optimized_ast;

    stream.close();
    return 0;
}
