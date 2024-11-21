#pragma once

#include <antlr4-runtime.h>
#include <iostream>
#include <stack>

namespace yona
{
    using namespace std;

    class YonaLexerBase : public antlr4::Lexer
    {
    private:
        size_t interpolatedStringLevel = 0;
        stack<size_t> curlyLevels;

    protected:
        void interpolationOpened();
        void interpolationClosed();
        void openCurly();
        void closeCurly();
        void interpolatedCurlyOpened();
        void interpolatedDoubleCurlyOpened();
        void interpolatedDoubleCurlyClosed();
        unique_ptr<antlr4::CommonToken> curlyToken(string const& text);

    public:
        YonaLexerBase(antlr4::CharStream* input);
    };

} // yonac
