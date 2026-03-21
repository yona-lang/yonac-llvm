#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API RangeModule : public NativeModule {
public:
    RangeModule();
    void initialize() override;

private:
    static RuntimeObjectPtr range(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toList(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr contains(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr length(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr take(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr drop(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
