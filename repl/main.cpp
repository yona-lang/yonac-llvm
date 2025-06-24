// main.cpp : Defines the entry point for the application.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <ranges>

#include <CLI/CLI.hpp>

#include "Interpreter.h"
#include "Optimizer.h"
#include "Parser.h"
#include "common.h"
#include "terminal.h"
#include "types.h"
#include "version.h"

using namespace std;

#ifdef _WIN32
#define PATH_SEPARATOR ";"
#else
#define PATH_SEPARATOR ":"
#endif

struct ProgramOptions {
  bool compile = false;
  bool print_ast = false;
  bool has_expr = false;
  string expr;
  vector<string> search_paths;
  vector<string> module_paths;
  string main_fun_name = "run";
};

// Helper function to split paths by the system-specific separator
vector<string> split_paths(const string& paths) {
  vector<string> result;
  stringstream ss(paths);
  string item;

  while (getline(ss, item, PATH_SEPARATOR[0])) {
    if (!item.empty()) {
      result.push_back(item);
    }
  }

  return result;
}

ProgramOptions process_program_options(const int argc, const char *const argv[]) {
  ProgramOptions opts;
  string expr_input;
  string module_path;
  string yona_path_env;

  CLI::App app{"Yona Language REPL", "yona"};
  app.set_version_flag("-v,--version", string("Yona Language ") + YONA_VERSION_STRING);

  // Options
  app.add_option("-e,--expr", expr_input, "Evaluate expression");
  app.add_option("-m,--module", module_path, "Input module file (lookup-able in YONA_PATH, without .yona extension)")
     ->check(CLI::NonexistentPath); // This allows non-existent paths (will be looked up in YONA_PATH)
  app.add_option("-f,--function", opts.main_fun_name, "Main function FQN")
     ->default_val("run");
  app.add_flag("--ast", opts.print_ast, "Print AST");
  app.add_flag("-c,--compile", opts.compile, "Compile mode");

  // Environment variable for search paths
  char* env_val = getenv("YONA_PATH");
  if (env_val) {
    yona_path_env = env_val;
    opts.search_paths = split_paths(yona_path_env);
  }

  // Allow module as positional argument
  app.add_option("module", module_path, "Input module file")
     ->check(CLI::NonexistentPath);

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    exit(app.exit(e));
  }

  // Process module path
  if (!module_path.empty()) {
    opts.module_paths.push_back(filesystem::path(module_path).string());
  }

  // Set environment
  yona::YONA_ENVIRONMENT.search_paths = opts.search_paths;
  yona::YONA_ENVIRONMENT.main_fun_name = opts.main_fun_name;

  if (opts.compile) {
    yona::YONA_ENVIRONMENT.compile_mode = true;
  }

  // Process expression if provided
  if (!expr_input.empty()) {
    opts.has_expr = true;
    opts.expr = expr_input;
  }

  return opts;
}


int main(const int argc, const char *argv[]) {
  using namespace yona;
  using namespace yona::ast;
  using namespace yona::compiler::types;

  const auto [term_width, term_height] = terminal::get_terminal_size();

  auto opts = process_program_options(argc, argv);

  parser::Parser parser;
  parser::ParseResult parse_result;

  // Parse either expression or module
  if (opts.has_expr) {
    // Parse expression from command line
    stringstream expr_stream(opts.expr);
    parse_result = parser.parse_input(expr_stream);
  } else if (!opts.module_paths.empty()) {
    // Parse module file
    parse_result = parser.parse_input(opts.module_paths);
  } else {
    // Start REPL mode
    cout << ANSI_COLOR_BOLD_BLUE << "Yona Language [" << YONA_VERSION_STRING << "]" << ANSI_COLOR_RESET << endl;
    cout << "Type ':help' for available commands." << endl;

    string line;
    int line_number = 1;

    while (true) {
      cout << ANSI_COLOR_BOLD_BLUE << "[" << line_number << "] " << ANSI_COLOR_RESET;
      cout.flush();

      if (!getline(cin, line)) {
        cout << endl << "Goodbye!" << endl;
        break;
      }

      // Handle REPL commands
      if (line == ":quit" || line == ":q") {
        cout << "Goodbye!" << endl;
        break;
      } else if (line == ":help" || line == ":h") {
        cout << "Available commands:" << endl;
        cout << "  :quit, :q     - Exit the REPL" << endl;
        cout << "  :help, :h     - Show this help" << endl;
        cout << "  :clear, :c    - Clear the screen" << endl;
        cout << "  :ast <expr>   - Show AST for expression" << endl;
        cout << endl;
        cout << "Enter any Yona expression to evaluate it." << endl;
        continue;
      } else if (line == ":clear" || line == ":c") {
        terminal::clear_screen();
        continue;
      } else if (line.starts_with(":ast ")) {
        string expr = line.substr(5);
        stringstream expr_stream(expr);
        parse_result = parser.parse_input(expr_stream);

        if (!parse_result.success) {
          for (auto [_type, error] : parse_result.ast_ctx.getErrors()) {
            std::cerr << *error << std::endl;
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
          std::cerr << *error << std::endl;
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
            std::cerr << *error << std::endl;
          }
          continue;
        }

        // Interpret the expression
        auto result = any_cast<shared_ptr<interp::RuntimeObject>>(optimized_ast->accept(interpreter));

        // Show the result
        cout << *result << endl;

      } catch (yona_error const &error) {
        std::cerr << error << std::endl;
      }

      line_number++;
    }

    return EXIT_SUCCESS;
  }

  if (!parse_result.success) {
    std::cerr << parse_result.ast_ctx.errors_.size() << " errors found. Please fix them and re-run." << std::endl;
    for (const auto &[_type, error] : parse_result.ast_ctx.getErrors()) {
      std::cerr << *error << std::endl;
    }
    return EXIT_FAILURE;
  }

  try {
    compiler::Optimizer optimizer;
    interp::Interpreter interpreter;

    auto optimized_ast = any_cast<expr_wrapper>(parse_result.node->accept(optimizer)).get_node<AstNode>();

    // Only show AST for module files, not for expressions
    if (!opts.has_expr && opts.print_ast) {
      std::cout << *optimized_ast << std::endl;
    }

    // Enable type checking (can be made optional via command line flag later)
    interpreter.enable_type_checking(true);

    // Type check the AST before interpretation
    if (!interpreter.type_check(optimized_ast)) {
      std::cerr << "Type checking failed:" << std::endl;
      for (const auto& error : interpreter.get_type_errors()) {
        std::cerr << *error << std::endl;
      }
      return EXIT_FAILURE;
    }

    // Interpret the type-checked AST
    auto result = any_cast<shared_ptr<interp::RuntimeObject>>(optimized_ast->accept(interpreter));

    // For expressions, just show the result
    if (opts.has_expr) {
      cout << *result << endl;
    } else {
      std::cout << ANSI_COLOR_BOLD_GREEN << string(term_width, FULL_BLOCK) << ANSI_COLOR_RESET << std::endl;
      std::cout << *result << std::endl;
    }
  } catch (yona_error const &error) {
    std::cerr << ANSI_COLOR_BOLD_RED << string(term_width, FULL_BLOCK) << ANSI_COLOR_RESET << std::endl;
    std::cerr << error << std::endl;
  }

  return EXIT_SUCCESS;
}
