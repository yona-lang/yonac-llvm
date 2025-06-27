//
// Created by akovari on 28.11.24.
//

#pragma once

#include <map>
#include <queue>
#include <string>
#include <any>
#include <optional>
#include <memory>
#include <format>

#include "terminal.h"
#include "SourceLocation.h"

namespace yona {
using namespace std;

using ModuleImportQueue = queue<vector<string>>;

inline struct YonaEnvironment {
  vector<string> search_paths;
  string main_fun_name;
  bool compile_mode = false;
} YONA_ENVIRONMENT;

class yona_error final : public runtime_error {
public:
  enum Type { SYNTAX, TYPE, REFERENCE, IO, COMPILER, NOT_IMPLEMENTED, RUNTIME };

  yona_error(const SourceLocation& ctx, Type type, const string& message)
      : runtime_error(message), ctx_(ctx), type_(type) {}

  [[nodiscard]] string format() const {
    static const map<Type, pair<string, string>> TYPE_DESCRIPTION = {
        {SYNTAX, {"Syntax", ANSI_COLOR_RED}},
        {TYPE, {"Type", ANSI_COLOR_CYAN}},
        {REFERENCE, {"Reference", ANSI_COLOR_GREEN}},
        {IO, {"IO", ANSI_COLOR_MAGENTA}},
        {COMPILER, {"Compiler", ANSI_COLOR_YELLOW}},
        {NOT_IMPLEMENTED, {"Not Implemented", ANSI_COLOR_WHITE}},
        {RUNTIME, {"Runtime", ANSI_COLOR_BRIGHT_RED}},
    };

    const string location = ctx_.is_valid() ? ctx_.to_string() + " " : "";

    auto [type_name, color] = TYPE_DESCRIPTION.at(type_);
    return std::format("{}{}{} error{}: {}", color, type_name, ANSI_COLOR_RESET,
                       location.empty() ? "" : " at " + location, what());
  }

  SourceLocation ctx_;
  Type type_;
};

inline ostream& operator<<(ostream& stream, const yona_error& error) {
  stream << error.format();
  return stream;
}

// Base frame template for symbol tables
template <typename T> struct Frame {
  shared_ptr<Frame> parent = nullptr;
  map<string, T> locals_;

  explicit Frame(shared_ptr<Frame> p) : parent(std::move(p)){};

  void write(const string& name, T value);
  T lookup(SourceInfo source_token, const string& name);
  optional<T> try_lookup(SourceInfo source_token, const string& name) noexcept;
  void merge(const Frame& other);
};

// AST context for error collection
struct AstContext {
  multimap<yona_error::Type, shared_ptr<yona_error>> errors_;

  void addError(const shared_ptr<yona_error>& error) { errors_.emplace(error->type_, error); }

  [[nodiscard]] multimap<yona_error::Type, shared_ptr<yona_error>> getErrors() const { return errors_; }

  [[nodiscard]] bool hasErrors() const { return !errors_.empty(); }
};

// Forward declarations for AST nodes
namespace ast {
class AstNode;
}

// Utilities
string module_location(const vector<string>& module_name);

} // namespace yona
