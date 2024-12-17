// main.cpp : Defines the entry point for the application.

#include <fstream>
#include <iostream>
#include <type_traits>

#include <antlr4-runtime.h>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/tokenizer.hpp>

#include "Interpreter.h"
#include "Optimizer.h"
#include "Parser.h"
#include "common.h"
#include "types.h"

using namespace std;

struct search_paths
{
    vector<string> values;
};

// Function which validates additional tokens from command line.
static void validate(boost::any& v, vector<string> const& tokens, search_paths* target_type, int)
{
    if (v.empty())
        v = boost::any(search_paths());

    search_paths* p = boost::any_cast<search_paths>(&v);
    BOOST_ASSERT(p);

    const boost::char_separator sep(":");
    for (string const& t : tokens)
    {
        if (t.find(","))
        {
            // tokenize values and push them back onto p->values
            boost::tokenizer tok(t, sep);
            ranges::copy(tok, back_inserter(p->values));
        }
        else
        {
            // store value as is
            p->values.push_back(t);
        }
    }
}

string process_program_options(const int argc, const char* const argv[])
{
    namespace po = boost::program_options;

    bool compile;
    string input_file;
    search_paths sp;

    po::options_description desc("Allowed options");
    desc.add_options()("help", "Show brief usage message");
    desc.add_options()("compile", po::bool_switch(&compile)->default_value(false), "Compile the input file");
    desc.add_options()("input-file", po::value<string>(&input_file), "Input file");

    po::options_description desc_env;
    desc_env.add_options()("yona-path", po::value<search_paths>(&sp)->multitoken(),
                           "Yona search paths (colon-separated list)");

    po::positional_options_description p;
    p.add("input-file", 1);

    po::variables_map args, env_args;

    try
    {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), args);
        po::store(po::parse_environment(desc_env, [](const std::string& i_env_var)
                                        { return i_env_var == "YONAPATH" ? "yona-path" : ""; }),
                  env_args);
    }
    catch (po::error const& e)
    {
        std::cerr << e.what() << '\n';
        exit(EXIT_FAILURE);
    }
    po::notify(args);
    po::notify(env_args);

    yona::YONA_ENVIRONMENT.search_paths = sp.values;

    if (args.contains("help"))
    {
        BOOST_LOG_TRIVIAL(info) << desc;
        exit(EXIT_SUCCESS);
    }

    if (compile)
    {
        BOOST_LOG_TRIVIAL(info) << "compile mode";
        // TODO compile mode
    }

    return input_file;
}

void init_logging() { boost::log::add_console_log(std::cout); }

int main(const int argc, const char* argv[])
{
    using namespace antlr4;
    using namespace yona;
    using namespace yona::ast;
    using namespace yona::compiler::types;

    init_logging();
    auto input_file = process_program_options(argc, argv);

    ifstream stream(input_file);
    BOOST_LOG_TRIVIAL(info) << "Yona Parser started for input file: " << input_file;

    parser::Parser parser;
    auto [success, node, type, ast_ctx] = parser.parse_input(stream);

    if (!success)
    {
        BOOST_LOG_TRIVIAL(error) << ast_ctx.errorCount() << " errors found. Please fix them and re-run.";
        for (auto [_type, error] : ast_ctx.getErrors())
        {
            BOOST_LOG_TRIVIAL(error) << error;
        }
        return EXIT_FAILURE;
    }

    BOOST_LOG_TRIVIAL(trace) << "Result expression type: " << typeid(type).name() << endl;

    compiler::Optimizer optimizer;
    interp::Interpreter interpreter;

    auto optimized_ast = any_cast<expr_wrapper>(node->accept(optimizer)).get_node<AstNode>();
    auto result = optimized_ast->accept(interpreter);
    cout << result.type().name() << endl;
    delete optimized_ast;

    stream.close();
    return EXIT_SUCCESS;
}
