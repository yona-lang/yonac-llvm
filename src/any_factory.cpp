#include "any_factory.h"
#include <typeinfo>

namespace yona::interp {

std::any AnyFactory::create(runtime::RuntimeObjectPtr obj) {
    // Create the any object in the DLL's memory space
    // This ensures the type_info is registered here
    return std::any(std::in_place_type<runtime::RuntimeObjectPtr>, std::move(obj));
}

runtime::RuntimeObjectPtr AnyFactory::extract(const std::any& a) {
    // Extract in the DLL where the type_info is known
    try {
        return std::any_cast<runtime::RuntimeObjectPtr>(a);
    } catch (const std::bad_any_cast& e) {
        // Type mismatch - return null
        return nullptr;
    }
}

bool AnyFactory::contains_runtime_object(const std::any& a) {
    // Check type in the DLL where type_info is known
    return a.type() == typeid(runtime::RuntimeObjectPtr);
}

} // namespace yona::interp
