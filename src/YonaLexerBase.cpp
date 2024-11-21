#include <antlr4-runtime.h>
#include <assert.h>

#include "YonaLexer.h"
#include "YonaLexerBase.h"

namespace yona
{
    using namespace std;

    YonaLexerBase::YonaLexerBase(antlr4::CharStream* input) : antlr4::Lexer(input) {}

    void YonaLexerBase::interpolationOpened() { interpolatedStringLevel++; }

    void YonaLexerBase::interpolationClosed()
    {
        interpolatedStringLevel--;
        assert(interpolatedStringLevel >= 0);
    }

    void YonaLexerBase::openCurly()
    {
        if (interpolatedStringLevel > 0)
        {
            const size_t curLevel = curlyLevels.top();
            curlyLevels.pop();
            curlyLevels.push(curLevel + 1);
        }
    }

    void YonaLexerBase::closeCurly()
    {
        if (interpolatedStringLevel > 0)
        {
            const size_t curLevel = curlyLevels.top();
            curlyLevels.pop();
            curlyLevels.push(curLevel - 1);

            if (curlyLevels.top() == 0)
            {
                curlyLevels.pop();
                popMode();
                setType(YonaLexer::CLOSE_INTERP);
            }
        }
    }

    void YonaLexerBase::interpolatedCurlyOpened() { curlyLevels.push(1); }

    void YonaLexerBase::interpolatedDoubleCurlyOpened() { emit(curlyToken("{")); }

    void YonaLexerBase::interpolatedDoubleCurlyClosed() { emit(curlyToken("}")); }

    unique_ptr<antlr4::CommonToken> YonaLexerBase::curlyToken(string const& text)
    {
        const size_t stop = getCharIndex() - 1;
        const size_t start = text.empty() ? stop : stop - text.length();
        return make_unique<antlr4::CommonToken>(new antlr4::CommonToken(
            make_pair(this, getInputStream()), YonaLexer::REGULAR_STRING_INSIDE, DEFAULT_TOKEN_CHANNEL, start, stop));
    }

} // yonac
