#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API TupleModule : public NativeModule {
public:
    TupleModule();
    void initialize() override;

private:
    static RuntimeObjectPtr fst(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr snd(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr swap(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr mapBoth(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr zip(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr unzip(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
