#pragma once

#include <memory>
#include "runtime.h"
#include "yona_export.h"

namespace yona::ast {

// Forward declaration
class AstVisitor;

// DLL-safe visitor result type that avoids std::any
class YONA_API InterpreterResult {
private:
    runtime::RuntimeObjectPtr value_;

public:
    InterpreterResult() = default;
    explicit InterpreterResult(runtime::RuntimeObjectPtr val) : value_(std::move(val)) {}

    // Copy and move constructors
    InterpreterResult(const InterpreterResult&) = default;
    InterpreterResult(InterpreterResult&&) noexcept = default;
    InterpreterResult& operator=(const InterpreterResult&) = default;
    InterpreterResult& operator=(InterpreterResult&&) noexcept = default;

    // Destructor
    ~InterpreterResult() = default;

    // Get the underlying value
    runtime::RuntimeObjectPtr get() const { return value_; }
    runtime::RuntimeObjectPtr& get_mutable() { return value_; }

    // Implicit conversion from RuntimeObjectPtr
    InterpreterResult(const runtime::RuntimeObjectPtr& val) : value_(val) {}
    InterpreterResult(runtime::RuntimeObjectPtr&& val) : value_(std::move(val)) {}

    // Implicit conversion to RuntimeObjectPtr
    operator runtime::RuntimeObjectPtr() const { return value_; }

    // Check if has value
    bool has_value() const { return value_ != nullptr; }
    explicit operator bool() const { return has_value(); }

    // Direct access
    runtime::RuntimeObjectPtr operator->() const { return value_; }
    runtime::RuntimeObject& operator*() const { return *value_; }
};

} // namespace yona::ast
