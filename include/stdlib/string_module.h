#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API StringModule : public NativeModule {
public:
    StringModule();
    void initialize() override;

private:
    static RuntimeObjectPtr length(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toUpperCase(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toLowerCase(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr trim(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr split(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr join(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr substring(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr indexOf(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr contains(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr replace(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr startsWith(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr endsWith(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toString(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr chars(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
