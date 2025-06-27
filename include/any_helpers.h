#pragma once

#include <any>
#include <memory>
#include "runtime.h"
#include "yona_export.h"

namespace yona::interp {

// Helper functions to ensure std::any operations happen in the DLL
// This avoids RTTI issues across DLL boundaries on Windows

// Create std::any containing RuntimeObjectPtr in the DLL
YONA_API std::any make_any_runtime_object(runtime::RuntimeObjectPtr obj);

// Extract RuntimeObjectPtr from std::any in the DLL
YONA_API runtime::RuntimeObjectPtr extract_runtime_object(const std::any& any_obj);
YONA_API runtime::RuntimeObjectPtr extract_runtime_object(std::any& any_obj);
YONA_API runtime::RuntimeObjectPtr extract_runtime_object(std::any&& any_obj);

} // namespace yona::interp
