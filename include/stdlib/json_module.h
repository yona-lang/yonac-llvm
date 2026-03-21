#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API JsonModule : public NativeModule {
public:
    JsonModule();
    void initialize() override;

private:
    static RuntimeObjectPtr parse(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr stringify(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
