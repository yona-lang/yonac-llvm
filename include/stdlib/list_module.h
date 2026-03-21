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
    static RuntimeObjectPtr take(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr drop(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr flatten(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr zip(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr foldl(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr foldr(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr lookup(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr splitAt(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr any(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr all(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr contains(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isEmpty(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
