#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API OptionModule : public NativeModule {
public:
    OptionModule();
    void initialize() override;

private:
    static RuntimeObjectPtr some(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr none(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isSome(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isNone(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr unwrapOr(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr map(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
