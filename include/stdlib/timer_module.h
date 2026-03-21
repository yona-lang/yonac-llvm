#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API TimerModule : public NativeModule {
public:
    TimerModule();
    void initialize() override;

private:
    static RuntimeObjectPtr sleep(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr millis(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr nanos(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr measure(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
