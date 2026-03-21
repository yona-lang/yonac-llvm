#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API TimeModule : public NativeModule {
public:
    TimeModule();
    void initialize() override;

private:
    static RuntimeObjectPtr now(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr nowMillis(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr format(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr parse(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
