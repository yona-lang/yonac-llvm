//
// Created by Adam Kovari on 17.12.2024.
//

#include "Parser.h"

#include <boost/log/trivial.hpp>

#include "ErrorListener.h"
#include "ErrorStrategy.h"
#include "YonaLexer.h"
#include "YonaParser.h"
#include "YonaVisitor.h"
#include "utils.h"

namespace yona::parser {
ParseResult Parser::parse_input(const vector<string> &module_name) {
  module_import_queue_.push(module_name);
  unordered_map<string, ParseResult> processed_modules;
  bool all_success = true;
  shared_ptr<AstNode> result_node;
  Type result_type;
  AstContext all_ast_ctx;

  while (!module_import_queue_.empty()) {
    bool success;
    shared_ptr<AstNode> node = nullptr;
    Type type;
    AstContext ast_ctx;

    if (string file_path = module_location(module_import_queue_.front()); !processed_modules.contains(file_path)) {
      ifstream stream(file_path);
      BOOST_LOG_TRIVIAL(info) << "Yona Parser started for input file: " << file_path;
      const auto result = parse_input(stream);
      BOOST_LOG_TRIVIAL(trace) << "Yona Parser finished for input file: " << file_path;
      stream.close();
      success = result.success;
      node = result.node;
      type = result.type;
      ast_ctx = result.ast_ctx;
      processed_modules[file_path] = {success, node, type, ast_ctx};
    } else {
      BOOST_LOG_TRIVIAL(info) << "Loading previously processed module from cache: " << file_path;
      auto &result = processed_modules[file_path];
      success = result.success;
      node = result.node;
      type = result.type;
      ast_ctx = result.ast_ctx;
    }

    all_success &= success;
    all_ast_ctx = all_ast_ctx + ast_ctx;

    if (!result_node) {
      result_node = node;
      result_type = type;
    }
    module_import_queue_.pop();
  }

  return {all_success, result_node, result_type, all_ast_ctx};
}

ParseResult Parser::parse_input(istream &stream) const {
  ANTLRInputStream input(stream);
  YonaLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  YonaParser parser(&tokens);
  auto ast_ctx = AstContext();
  ErrorListener error_listener(ast_ctx);

  lexer.removeErrorListeners();
  parser.removeErrorListeners();
  lexer.addErrorListener(&error_listener);
  parser.addErrorListener(&error_listener);
  parser.setErrorHandler(make_shared<ErrorStrategy>(ast_ctx));

  YonaParser::InputContext *tree = parser.input();
  // BOOST_LOG_TRIVIAL(trace) << "Parse tree: " << tree->toStringTree();

  if (ast_ctx.hasErrors()) {
    return {!ast_ctx.hasErrors(), nullptr, nullptr, ast_ctx};
  }

  YonaVisitor yona_visitor(module_import_queue_);
  any ast = yona_visitor.visitInput(tree);
  auto node = any_cast<expr_wrapper>(ast).get_node<AstNode>();

  auto type = node->infer_type(ast_ctx);

  return {!ast_ctx.hasErrors(), shared_ptr<AstNode>(node), type, ast_ctx};
}
} // namespace yona::parser
