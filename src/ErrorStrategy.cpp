//
// Created by Adam Kovari on 16.12.2024.
//

#include "ErrorStrategy.h"

#include <YonaParser.h>

namespace yona::parser
{
  using namespace std;

  void ErrorStrategy::handleRecognitionException(IntervalSet et, const string& message, const exception& cause,
                                                 int line)
  {
    // TODO
    // if (et.contains(YonaParser::KW_END) || et.contains(YonaParser::NEWLINE))
    // {
    //     throw new IncompleteSource(source, message, cause, line);
    // }
  }

  void ErrorStrategy::reportNoViableAlternative(Parser* parser, const NoViableAltException& e)
  {
    Recognizer* recognizer = e.getRecognizer();
    string msg = "Reason: * can't choose next alternative. ";
    vector<string> stack = parser->getRuleInvocationStack();
    IntervalSet expectedTokens = e.getExpectedTokens();
    Token* offendingToken = e.getOffendingToken();

    reverse(stack.begin(), stack.end());
    recognizer->getTokenErrorDisplay(e.getOffendingToken());

    msg.append("Parser stack: [");

    for (const auto& item : stack)
    {
      msg.append(item);
      msg.append(", ");
    }

    if (!stack.empty())
    {
      msg.erase(msg.length() - 2);
    }

    msg.append("]. Valid alternatives are: ");
    msg.append(expectedTokens.toString(recognizer->getVocabulary()));

    ast_ctx.addError(YonaError(TokenLocation(*offendingToken, *recognizer), YonaError::Type::SYNTAX, msg));

    recognizer->getErrorListenerDispatch().syntaxError(recognizer, offendingToken, offendingToken->getLine(),
                                                       offendingToken->getCharPositionInLine(), msg,
                                                       make_exception_ptr(e));
  }
} // yona::parser
