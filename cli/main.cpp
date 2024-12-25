// main.cpp : Defines the entry point for the application.

#include <filesystem>
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
#include "terminal.h"
#include "types.h"

using namespace std;

struct search_paths
{
  vector<string> values;
};

struct path
{
  vector<string> values;
};

#ifdef _WIN32
#define PATH_SEPARATOR ";"
#else
#define PATH_SEPARATOR ":"
#endif

static void validate(boost::any& v, vector<string> const& tokens, search_paths* target_type, int)
{
  if (v.empty())
    v = boost::any(search_paths());

  search_paths* p = boost::any_cast<search_paths>(&v);
  BOOST_ASSERT(p);

  const boost::char_separator sep(PATH_SEPARATOR);
  for (string const& t : tokens)
  {
    // tokenize values and push them back onto p->values
    boost::tokenizer tok(t, sep);
    ranges::copy(tok, back_inserter(p->values));
  }
}

static void validate(boost::any& v, vector<string> const& tokens, path* target_type, int)
{
  if (v.empty())
    v = boost::any(path());

  path* p = boost::any_cast<path>(&v);
  BOOST_ASSERT(p);

  for (string const& t : tokens)
  {
    p->values.push_back(filesystem::path(t));
  }
}

vector<string> process_program_options(const int argc, const char* const argv[])
{
  namespace po = boost::program_options;

  bool compile;
  search_paths sp;
  path mp;

  po::options_description desc("Allowed options");
  desc.add_options()("help", "Show brief usage message");
  desc.add_options()("compile", po::bool_switch(&compile)->default_value(false), "Compile the input file");
  desc.add_options()("module", po::value<path>(&mp),
                     "Input module file (lookup-able in YONA_PATH, separated by system specific path separator, "
                     "without .yona extension)");

  po::options_description desc_env;
  desc_env.add_options()("yona-path", po::value<search_paths>(&sp)->multitoken(),
                         "Yona search paths (colon-separated list)");

  po::positional_options_description p;
  p.add("module", 1);

  po::variables_map args, env_args;

  try
  {
    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), args);
    po::store(po::parse_environment(desc_env, [](const std::string& i_env_var)
                                    { return i_env_var == "YONA_PATH" ? "yona-path" : ""; }),
              env_args);
  }
  catch (po::error const& e)
  {
    BOOST_LOG_TRIVIAL(error) << e.what();
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
    BOOST_LOG_TRIVIAL(trace) << "compile mode";
    yona::YONA_ENVIRONMENT.compile_mode = true;
  }

  return mp.values;
}

void init_logging() { boost::log::add_console_log(std::cout); }

int main(const int argc, const char* argv[])
{
  using namespace antlr4;
  using namespace yona;
  using namespace yona::ast;
  using namespace yona::compiler::types;

  auto [term_width, term_height] = get_terminal_size();

  init_logging();
  auto module = process_program_options(argc, argv);

  parser::Parser parser;
  auto [success, node, type, ast_ctx] = parser.parse_input(module);

  if (!success)
  {
    BOOST_LOG_TRIVIAL(error) << ast_ctx.errorCount() << " errors found. Please fix them and re-run.";
    for (auto [_type, error] : ast_ctx.getErrors())
    {
      BOOST_LOG_TRIVIAL(error) << error;
    }
    return EXIT_FAILURE;
  }

  compiler::Optimizer optimizer;
  interp::Interpreter interpreter;

  try
  {
    auto optimized_ast = any_cast<expr_wrapper>(node->accept(optimizer)).get_node<AstNode>();
    auto result        = any_cast<shared_ptr<interp::RuntimeObject>>(optimized_ast->accept(interpreter));


    BOOST_LOG_TRIVIAL(info) << ANSI_COLOR_BOLD_GREEN << string(term_width, FULL_BLOCK) << ANSI_COLOR_RESET << endl
                            << *result;
  }
  catch (yona_error const& error)
  {
    BOOST_LOG_TRIVIAL(error) << ANSI_COLOR_BOLD_RED << string(term_width, FULL_BLOCK) << ANSI_COLOR_RESET << endl
                             << error;
  }

  return EXIT_SUCCESS;
}
