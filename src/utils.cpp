//
// Created by akovari on 15.12.24.
//
#include "utils.h"

#include <antlr4-runtime.h>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace yona {
using namespace antlr4;
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
  for (const auto &component : module_name) {
    result /= component;
  }
  result.replace_extension(".yona");
  return result;
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
} // namespace yona
