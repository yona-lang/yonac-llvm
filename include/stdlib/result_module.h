#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API ResultModule : public NativeModule {
public:
    ResultModule();
    void initialize() override;

private:
    static RuntimeObjectPtr ok(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr err(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isOk(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isErr(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr unwrapOr(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr map(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr mapErr(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
