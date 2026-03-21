#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API ListModule : public NativeModule {
public:
    ListModule();
    void initialize() override;

private:
    static RuntimeObjectPtr map(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr filter(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr fold(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr length(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr head(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr tail(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr reverse(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
