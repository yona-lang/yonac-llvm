// main.cpp : Defines the entry point for the application.

#include <iostream>
#include <fstream>

#include <antlr4-runtime.h>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <boost/log/core.hpp>
#include <boost/log/utility/setup/console.hpp>

#include "utils.h"
#include "types.h"
#include "common.h"
#include "Interpreter.h"
#include "Optimizer.h"

void process_program_options(const int argc, const char* const argv[])
{
    namespace po = boost::program_options;

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
        std::cerr << e.what() << '\n';
        exit(EXIT_FAILURE);
    }
    po::notify(args);
}

void init_logging()
{
    boost::log::add_console_log(std::cout);
}

int main(const int argc, const char* argv[])
{
    using namespace antlr4;
    using namespace yona;
    using namespace yona::compiler::types;

    init_logging();
    process_program_options(argc, argv);

    string fname;
#ifdef NDEBUG
    fname = argc > 1 ? argv[1] : "test.yona";
#else
    fname = argv[1];
#endif
    ifstream stream(fname);
    BOOST_LOG_TRIVIAL(info) << "Yona Parser started for input file: " << fname;

    auto [success, node, type, ast_ctx] = parse_input(stream);

    if (!success)
    {
        BOOST_LOG_TRIVIAL(error) << ast_ctx.getErrors().size() << " errors found. Please fix them and re-run.";
        for (auto err : ast_ctx.getErrors())
        {
            BOOST_LOG_TRIVIAL(error) << err;
        }
        return 1;
    }

    BOOST_LOG_TRIVIAL(trace) << "Result expression type: " << typeid(type).name() << endl;

    compiler::Optimizer optimizer;
    interp::Interpreter interpreter;

    auto optimized_ast = any_cast<expr_wrapper>(node->accept(optimizer)).get_node<AstNode>();
    auto result = optimized_ast->accept(interpreter);
    cout << result.type().name() << endl;
    delete optimized_ast;

    stream.close();
    return 0;
}
