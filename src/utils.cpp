//
// Created by akovari on 15.12.24.
//
#include <filesystem>
#include <string>
#include <map>
#include <vector>

#include "utils.h"

namespace yona {
using namespace std;

std::string LookupTranslator::translate(const std::string &input) {
  std::string result = input;
  for (const auto &pair : lookupMap) {
    size_t pos = 0;
    while ((pos = result.find(pair.first, pos)) != std::string::npos) {
      result.replace(pos, pair.first.length(), pair.second);
      pos += pair.second.length();
    }
  }
  return result;
}

std::string AggregateTranslator::translate(const std::string &input) {
  std::string result = input;
  for (auto &translator : translators) {
    result = translator->translate(result);
  }
  return result;
}

string unescapeYonaString(const string &rawString) { return UNESCAPE_YONA.translate(rawString); }

string module_location(const vector<string> &module_name) {
  filesystem::path result;
  for (const auto component : module_name) {
    result /= component;
  }
  result.replace_extension(".yona");
  return result.generic_string();
}

template <typename T> optional<T> first_defined_optional(initializer_list<optional<T>> optionals) {
  for (const auto &optional : optionals) {
    if (optional.has_value()) {
      return optional;
    }
  }
  return nullopt;
}

template optional<any> first_defined_optional(initializer_list<optional<any>> optionals);
template optional<ast::InterpreterResult> first_defined_optional(initializer_list<optional<ast::InterpreterResult>> optionals);
} // namespace yona
