//
// Created by akovari on 15.12.24.
//
#include "utils.h"

#include <antlr4-runtime.h>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace yona
{
  using namespace antlr4;
  using namespace std;

  std::string LookupTranslator::translate(const std::string& input)
  {
    std::string result = input;
    for (const auto& pair : lookupMap)
    {
      size_t pos = 0;
      while ((pos = result.find(pair.first, pos)) != std::string::npos)
      {
        result.replace(pos, pair.first.length(), pair.second);
        pos += pair.second.length();
      }
    }
    return result;
  }

  std::string AggregateTranslator::translate(const std::string& input)
  {
    std::string result = input;
    for (auto& translator : translators)
    {
      result = translator->translate(result);
    }
    return result;
  }

  string unescapeYonaString(const string& rawString) { return UNESCAPE_YONA.translate(rawString); }
}
