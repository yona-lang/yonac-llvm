//
// Created by akovari on 28.11.24.
//

#pragma once

#include <antlr4-runtime.h>
#include <queue>
#include <string>

#include "terminal.h"

namespace yona {
using namespace std;
using namespace antlr4;

using SourceContext = const ParserRuleContext &;
using ModuleImportQueue = queue<vector<string>>;

inline struct YonaEnvironment {
  vector<string> search_paths;
  bool compile_mode = false;
} YONA_ENVIRONMENT;

struct SourceInfo final {
  size_t start_line;
  size_t start_col;
  size_t stop_line;
  size_t stop_col;
  string text;

  SourceInfo(SourceContext token)
      : start_line(token.getStart() ? token.getStart()->getLine() : 0), start_col(token.getStart() ? token.getStart()->getCharPositionInLine() : 0),
        stop_line(token.getStop() ? token.getStop()->getLine() : 0), stop_col(token.getStop() ? token.getStop()->getCharPositionInLine() : 0),
        text(token.getStart() ? token.getStart()->getText() : "?") {}

  SourceInfo(antlr4::Token &token, Recognizer &recognizer)
      : start_line(token.getLine()), start_col(token.getCharPositionInLine()), stop_line(token.getLine()),
        stop_col(token.getText().length() + start_col), text(recognizer.getTokenErrorDisplay(&token)) {}

  SourceInfo(const size_t start_line, const size_t start_col, const size_t stop_line, const size_t stop_col, string text)
      : start_line(start_line), start_col(start_col), stop_line(stop_line), stop_col(stop_col), text(std::move(text)) {}
};

inline std::ostream &operator<<(std::ostream &os, const SourceInfo &rhs) {
  os << "[" << rhs.start_line << ":" << rhs.start_col << "-" << rhs.stop_line << ":" << rhs.stop_col << "] " << rhs.text;
  return os;
}

struct yona_error final : std::runtime_error {
  enum Type { SYNTAX, TYPE, RUNTIME } type;
  SourceInfo source_token;
  string message;

  explicit yona_error(SourceInfo source_token, Type type, string message)
      : runtime_error(message), type(type), source_token(std::move(source_token)), message(std::move(message)) {}
};

inline const char *ErrorTypes[] = {"syntax", "type", "runtime check"};

inline std::ostream &operator<<(std::ostream &os, const yona_error &rhs) {
  os << ANSI_COLOR_RED << "Invalid " << ErrorTypes[rhs.type] << " at " << rhs.source_token << ANSI_COLOR_RESET << ": " << rhs.message;
  return os;
}

class AstContext final {
private:
  unordered_multimap<yona_error::Type, yona_error> errors_;

public:
  explicit AstContext(unordered_multimap<yona_error::Type, yona_error> errors) : errors_(std::move(errors)) {};
  explicit AstContext() = default;

  void addError(const yona_error &error) { errors_.insert({error.type, error}); }
  [[nodiscard]] bool hasErrors() const { return !errors_.empty(); }
  [[nodiscard]] const unordered_multimap<yona_error::Type, yona_error> &getErrors() const { return errors_; }
  [[nodiscard]] size_t errorCount() const { return errors_.size(); }

  AstContext operator+(const AstContext &other) const {
    unordered_multimap new_errors(errors_);
    new_errors.insert(other.errors_.begin(), other.errors_.end());
    return AstContext(new_errors);
  }
};

template <typename T> struct Frame {
private:
  vector<T> args_;
  unordered_map<string, T> locals_;

public:
  shared_ptr<Frame> parent;

  explicit Frame(shared_ptr<Frame> parent) : parent(std::move(parent)) {}

  void write(const string &name, T value);
  void write(const string &name, any value);
  T lookup(SourceInfo source_token, const string &name);
};
}; // namespace yona
