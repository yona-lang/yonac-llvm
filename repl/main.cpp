// main.cpp : Defines the entry point for the application.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <type_traits>

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
#include "version.h"

using namespace std;

struct search_paths {
  vector<string> values;
};

struct path {
  vector<string> values;
};

#ifdef _WIN32
#define PATH_SEPARATOR ";"
#else
#define PATH_SEPARATOR ":"
#endif

static void validate(boost::any &v, vector<string> const &tokens, search_paths *target_type, int) {
  if (v.empty())
    v = boost::any(search_paths());

  search_paths *p = boost::any_cast<search_paths>(&v);
  BOOST_ASSERT(p);

  const boost::char_separator<char> sep(PATH_SEPARATOR);
  for (string const &t : tokens) {
    // tokenize values and push them back onto p->values
    boost::tokenizer<boost::char_separator<char>> tok(t, sep);
    ranges::copy(tok, back_inserter(p->values));
  }
}

static void validate(boost::any &v, vector<string> const &tokens, path *target_type, int) {
  if (v.empty())
    v = boost::any(path());

  path *p = boost::any_cast<path>(&v);
  BOOST_ASSERT(p);

  for (string const &t : tokens) {
    p->values.push_back(filesystem::path(t));
  }
}

struct ProgramOptions {
  bool compile = false;
  bool has_expr = false;
  string expr;
  search_paths sp;
  path mp;
  string main_fun_name = "run";
};

ProgramOptions process_program_options(const int argc, const char *const argv[]) {
  namespace po = boost::program_options;

  ProgramOptions opts;
  string expr_input;

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "Show brief usage message");
  desc.add_options()("expr,e", po::value<string>(&expr_input), "Evaluate expression");
  desc.add_options()("module,m", po::value<path>(&opts.mp),
                     "Input module file (lookup-able in YONA_PATH, separated "
                     "by system specific path separator, "
                     "without .yona extension)");
  desc.add_options()("function,f", po::value<string>(&opts.main_fun_name)->default_value("run"), "Main function FQN");

  po::options_description desc_env;
  desc_env.add_options()("yona-path", po::value<search_paths>(&opts.sp)->multitoken(), "Yona search paths (colon-separated list)");

  po::positional_options_description p;
  p.add("module", 1);

  po::variables_map args, env_args;

  try {
    po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), args);
    po::store(po::parse_environment(desc_env, [](const std::string &i_env_var) { return i_env_var == "YONA_PATH" ? "yona-path" : ""; }), env_args);
  } catch (po::error const &e) {
    BOOST_LOG_TRIVIAL(error) << e.what();
    exit(EXIT_FAILURE);
  }
  po::notify(args);
  po::notify(env_args);

  yona::YONA_ENVIRONMENT.search_paths = opts.sp.values;
  yona::YONA_ENVIRONMENT.main_fun_name = opts.main_fun_name;

  if (args.contains("help")) {
    BOOST_LOG_TRIVIAL(info) << desc;
    exit(EXIT_SUCCESS);
  }

  if (opts.compile) {
    BOOST_LOG_TRIVIAL(trace) << "compile mode";
    yona::YONA_ENVIRONMENT.compile_mode = true;
  }

  // Process expression if provided
  if (args.contains("expr")) {
    opts.has_expr = true;
    opts.expr = expr_input;
  }

  return opts;
}

void init_logging() { boost::log::add_console_log(std::cout); }

int main(const int argc, const char *argv[]) {
  using namespace yona;
  using namespace yona::ast;
  using namespace yona::compiler::types;

  auto [term_width, term_height] = get_terminal_size();

  init_logging();
  auto opts = process_program_options(argc, argv);

  parser::Parser parser;
  parser::ParseResult parse_result;

  // Parse either expression or module
  if (opts.has_expr) {
    // Parse expression from command line
    stringstream expr_stream(opts.expr);
    parse_result = parser.parse_input(expr_stream);
  } else if (!opts.mp.values.empty()) {
    // Parse module file
    parse_result = parser.parse_input(opts.mp.values);
  } else {
    // Start REPL mode
    cout << "Yona REPL v" << YONA_VERSION_STRING << endl;
    cout << "Type :help for help, :quit to exit" << endl;
    
    string line;
    int line_number = 1;
    
    while (true) {
      cout << "yona> ";
      cout.flush();
      
      if (!getline(cin, line)) {
        // EOF reached (Ctrl+D)
        cout << endl;
        break;
      }
      
      // Handle REPL commands
      if (line == ":quit" || line == ":q") {
        break;
      } else if (line == ":help" || line == ":h") {
        cout << "REPL Commands:" << endl;
        cout << "  :help, :h    Show this help message" << endl;
        cout << "  :quit, :q    Exit the REPL" << endl;
        cout << "  :ast <expr>  Show the AST of an expression" << endl;
        cout << endl;
        cout << "Enter any Yona expression to evaluate it." << endl;
        continue;
      } else if (line.starts_with(":ast ")) {
        string expr = line.substr(5);
        stringstream expr_stream(expr);
        parse_result = parser.parse_input(expr_stream);
        
        if (!parse_result.success) {
          for (auto [_type, error] : parse_result.ast_ctx.getErrors()) {
            BOOST_LOG_TRIVIAL(error) << *error;
          }
          continue;
        }
        
        cout << *parse_result.node << endl;
        continue;
      } else if (line.empty() || line.starts_with(":")) {
        if (!line.empty()) {
          cout << "Unknown command. Type :help for help." << endl;
        }
        continue;
      }
      
      // Parse and evaluate the expression
      stringstream expr_stream(line);
      parse_result = parser.parse_input(expr_stream);
      
      if (!parse_result.success) {
        for (auto [_type, error] : parse_result.ast_ctx.getErrors()) {
          BOOST_LOG_TRIVIAL(error) << *error;
        }
        continue;
      }
      
      try {
        compiler::Optimizer optimizer;
        interp::Interpreter interpreter;
        
        auto optimized_ast = any_cast<expr_wrapper>(parse_result.node->accept(optimizer)).get_node<AstNode>();
        
        // Type check before interpretation
        interpreter.enable_type_checking(true);
        if (!interpreter.type_check(optimized_ast)) {
          for (const auto& error : interpreter.get_type_errors()) {
            BOOST_LOG_TRIVIAL(error) << *error;
          }
          continue;
        }
        
        // Interpret the expression
        auto result = any_cast<shared_ptr<interp::RuntimeObject>>(optimized_ast->accept(interpreter));
        
        // Show the result
        cout << *result << endl;
        
      } catch (yona_error const &error) {
        BOOST_LOG_TRIVIAL(error) << error;
      }
      
      line_number++;
    }
    
    return EXIT_SUCCESS;
  }

  if (!parse_result.success) {
    BOOST_LOG_TRIVIAL(error) << parse_result.ast_ctx.errors_.size() << " errors found. Please fix them and re-run.";
    for (auto [_type, error] : parse_result.ast_ctx.getErrors()) {
      BOOST_LOG_TRIVIAL(error) << *error;
    }
    return EXIT_FAILURE;
  }

  try {
    compiler::Optimizer optimizer;
    interp::Interpreter interpreter;

    auto optimized_ast = any_cast<expr_wrapper>(parse_result.node->accept(optimizer)).get_node<AstNode>();
    
    // Only show AST for module files, not for expressions
    if (!opts.has_expr) {
      BOOST_LOG_TRIVIAL(info) << *optimized_ast;
    }
    
    // Enable type checking (can be made optional via command line flag later)
    interpreter.enable_type_checking(true);
    
    // Type check the AST before interpretation
    if (!interpreter.type_check(optimized_ast)) {
      BOOST_LOG_TRIVIAL(error) << "Type checking failed:";
      for (const auto& error : interpreter.get_type_errors()) {
        BOOST_LOG_TRIVIAL(error) << *error;
      }
      return EXIT_FAILURE;
    }
    
    // Interpret the type-checked AST
    auto result = any_cast<shared_ptr<interp::RuntimeObject>>(optimized_ast->accept(interpreter));

    // For expressions, just show the result
    if (opts.has_expr) {
      cout << *result << endl;
    } else {
      BOOST_LOG_TRIVIAL(info) << ANSI_COLOR_BOLD_GREEN << string(term_width, FULL_BLOCK) << ANSI_COLOR_RESET << endl << *result;
    }
  } catch (yona_error const &error) {
    BOOST_LOG_TRIVIAL(error) << ANSI_COLOR_BOLD_RED << string(term_width, FULL_BLOCK) << ANSI_COLOR_RESET << endl << error;
  }

  return EXIT_SUCCESS;
}
