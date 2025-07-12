#include "interpreter_facade.h"
#include "Interpreter.h"
#include "ast_visitor_impl.h"
#include <any>

namespace yona::interp {

// Implementation class that stays entirely within the DLL
class InterpreterFacade::Impl {
public:
    Interpreter interpreter;

    runtime::RuntimeObjectPtr evaluate(ast::AstNode* node) {
        // All std::any operations happen here in the DLL
        auto result = interpreter.visit(node);
        return std::any_cast<runtime::RuntimeObjectPtr>(result);
    }

    void enable_type_checking(bool enable) {
        interpreter.enable_type_checking(enable);
    }
};

InterpreterFacade::InterpreterFacade() : pImpl(std::make_unique<Impl>()) {}

// Define destructor in cpp file to ensure Impl is complete
InterpreterFacade::~InterpreterFacade() = default;

runtime::RuntimeObjectPtr InterpreterFacade::evaluate(ast::AstNode* node) {
    return pImpl->evaluate(node);
}

void InterpreterFacade::enable_type_checking(bool enable) {
    pImpl->enable_type_checking(enable);
}

} // namespace yona::interp
