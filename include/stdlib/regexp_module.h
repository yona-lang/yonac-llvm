#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API RegexpModule : public NativeModule {
public:
    RegexpModule();
    void initialize() override;

private:
    static RuntimeObjectPtr match(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr matchAll(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr replace(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr split(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr test(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
