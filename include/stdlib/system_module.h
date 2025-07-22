#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API SystemModule : public NativeModule {
public:
    SystemModule();
    void initialize() override;

private:
    // System functions
    static RuntimeObjectPtr getEnv(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr setEnv(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr exit(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr currentTimeMillis(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr sleep(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr getArgs(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr getCwd(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr setCwd(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
