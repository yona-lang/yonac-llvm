#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API RandomModule : public NativeModule {
public:
    RandomModule();
    void initialize() override;

private:
    static RuntimeObjectPtr random_int(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr random_float(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr random_choice(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr random_shuffle(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
