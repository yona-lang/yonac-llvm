#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API SetModule : public NativeModule {
public:
    SetModule();
    void initialize() override;

private:
    static RuntimeObjectPtr contains(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr add(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr remove(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr size(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toList(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr fromList(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr union_(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr intersection(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr difference(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
