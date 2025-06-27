#include "any_helpers.h"
#include <any>

namespace yona::interp {

// Implementation of helper functions to ensure std::any operations happen in the DLL

std::any make_any_runtime_object(runtime::RuntimeObjectPtr obj) {
    return std::any(std::move(obj));
}

runtime::RuntimeObjectPtr extract_runtime_object(const std::any& any_obj) {
    return std::any_cast<runtime::RuntimeObjectPtr>(any_obj);
}

runtime::RuntimeObjectPtr extract_runtime_object(std::any& any_obj) {
    return std::any_cast<runtime::RuntimeObjectPtr>(any_obj);
}

runtime::RuntimeObjectPtr extract_runtime_object(std::any&& any_obj) {
    return std::any_cast<runtime::RuntimeObjectPtr>(std::move(any_obj));
}

} // namespace yona::interp
