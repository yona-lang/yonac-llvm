//
// Created by akovari on 17.11.24.
//

#include <boost/log/trivial.hpp>
#include "Interpreter.h"

namespace yona::interp
{
    result_t Interpreter::visit(AstNode node) { BOOST_LOG_TRIVIAL(info) << "node" << endl; }
    result_t Interpreter::visit(BinaryOpExpr node) { BOOST_LOG_TRIVIAL(info) << "binary" << endl; }
    result_t Interpreter::visit(AddExpr node) { BOOST_LOG_TRIVIAL(info) << "adding" << endl; }

} // yona::interp
