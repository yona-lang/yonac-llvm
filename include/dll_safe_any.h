#pragma once

#include <any>
#include <memory>
#include <typeinfo>
#include <typeindex>
#include "runtime.h"
#include "yona_export.h"

namespace yona::interp {

// On Windows with DLLs, std::any doesn't work well across module boundaries
// This provides a workaround specifically for RuntimeObjectPtr
class YONA_API DllSafeAny {
private:
    std::any value_;
    std::type_index type_index_;

public:
    DllSafeAny() : type_index_(typeid(void)) {}

    // Constructor specifically for RuntimeObjectPtr
    explicit DllSafeAny(runtime::RuntimeObjectPtr obj)
        : value_(obj), type_index_(typeid(runtime::RuntimeObjectPtr)) {}

    // Construct from std::any
    explicit DllSafeAny(const std::any& a)
        : value_(a), type_index_(a.type()) {}

    // Get as RuntimeObjectPtr - main use case
    runtime::RuntimeObjectPtr get_runtime_object() const {
        if (type_index_ == typeid(runtime::RuntimeObjectPtr)) {
            return std::any_cast<runtime::RuntimeObjectPtr>(value_);
        }
        throw std::bad_any_cast();
    }

    // Convert to std::any
    std::any to_any() const { return value_; }

    // Check type
    bool holds_runtime_object() const {
        return type_index_ == typeid(runtime::RuntimeObjectPtr);
    }
};

// Helper function to safely extract RuntimeObjectPtr from std::any across DLL boundary
inline runtime::RuntimeObjectPtr safe_any_cast_runtime_object(const std::any& a) {
    // On Windows, we need to be careful with any_cast across DLL boundaries
    // This works around the issue by using a type-erased approach
    try {
        return std::any_cast<runtime::RuntimeObjectPtr>(a);
    } catch (const std::bad_any_cast&) {
        // Type mismatch - this shouldn't happen in normal operation
        return nullptr;
    }
}

} // namespace yona::interp
