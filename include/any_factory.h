#pragma once

#include <any>
#include <memory>
#include "runtime.h"
#include "yona_export.h"

namespace yona::interp {

// Factory to create std::any objects in the DLL
// This ensures type_info is registered in the DLL's type system
class YONA_API AnyFactory {
public:
    // Create std::any containing RuntimeObjectPtr in the DLL
    static std::any create(runtime::RuntimeObjectPtr obj);

    // Extract RuntimeObjectPtr from std::any in the DLL
    static runtime::RuntimeObjectPtr extract(const std::any& a);

    // Check if any contains RuntimeObjectPtr
    static bool contains_runtime_object(const std::any& a);
};

} // namespace yona::interp
