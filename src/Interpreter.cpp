//
// Created by akovari on 17.11.24.
//

#include "Interpreter.h"
#include <boost/log/trivial.hpp>

namespace yona::interp
{
    result_t Interpreter::visit(const AstNode& node)
    {
        BOOST_LOG_TRIVIAL(info) << "node" << endl;
        return make_any<int>(0);
    }
    result_t Interpreter::visit(const BinaryOpExpr& node)
    {
        BOOST_LOG_TRIVIAL(info) << "binary" << endl;
        return make_any<int>(0);
    }
    result_t Interpreter::visit(const AddExpr& node)
    {
        BOOST_LOG_TRIVIAL(info) << "adding" << endl;
        return make_any<int>(0);
    }

} // yona::interp
