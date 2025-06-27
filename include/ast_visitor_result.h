#pragma once

#include <memory>
#include "runtime.h"
#include "yona_export.h"

namespace yona::ast {

// Result type wrapper for visitor pattern
// This avoids std::any and provides type safety
struct YONA_API InterpreterResult {
    yona::interp::runtime::RuntimeObjectPtr value;

    InterpreterResult() = default;
    explicit InterpreterResult(yona::interp::runtime::RuntimeObjectPtr v) : value(std::move(v)) {}

    // Allow implicit conversion from RuntimeObjectPtr
    InterpreterResult& operator=(yona::interp::runtime::RuntimeObjectPtr v) {
        value = std::move(v);
        return *this;
    }

    // Access the underlying value
    yona::interp::runtime::RuntimeObjectPtr& operator*() { return value; }
    const yona::interp::runtime::RuntimeObjectPtr& operator*() const { return value; }

    yona::interp::runtime::RuntimeObjectPtr* operator->() { return &value; }
    const yona::interp::runtime::RuntimeObjectPtr* operator->() const { return &value; }

    // Check if has value
    explicit operator bool() const { return value != nullptr; }
};

} // namespace yona::ast
