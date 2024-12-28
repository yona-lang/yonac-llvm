#pragma once

#include <antlr4-runtime.h>
#include <stack>

namespace yona {
using namespace std;

class YonaLexerBase : public antlr4::Lexer {
private:
  size_t interpolatedStringLevel = 0;
  stack<size_t> curlyLevels;
  unordered_set<string> symbols;

protected:
  void interpolationOpened();
  void interpolationClosed();
  void openCurly();
  void closeCurly();
  void interpolatedCurlyOpened();
  void interpolatedDoubleCurlyOpened();
  void interpolatedDoubleCurlyClosed();
  void addSymbol();
  unique_ptr<antlr4::CommonToken> curlyToken(string const &text);

public:
  YonaLexerBase(antlr4::CharStream *input);
};

} // namespace yona
