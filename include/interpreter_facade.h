#pragma once

#include <memory>
#include "runtime.h"
#include "yona_export.h"

namespace yona {
namespace ast {
    class AstNode;
}

namespace interp {

// Facade class that provides a DLL-safe interface to the interpreter
// This avoids exposing std::any across DLL boundaries
class YONA_API InterpreterFacade {
public:
    InterpreterFacade();
    ~InterpreterFacade();

    // No copy/move to avoid DLL boundary issues
    InterpreterFacade(const InterpreterFacade&) = delete;
    InterpreterFacade& operator=(const InterpreterFacade&) = delete;
    InterpreterFacade(InterpreterFacade&&) = delete;
    InterpreterFacade& operator=(InterpreterFacade&&) = delete;

    // Evaluate an AST node and return the result
    // This method handles all std::any operations internally
    runtime::RuntimeObjectPtr evaluate(ast::AstNode* node);

    // Enable/disable type checking
    void enable_type_checking(bool enable = true);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace interp
} // namespace yona
