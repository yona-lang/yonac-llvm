#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API DictModule : public NativeModule {
public:
    DictModule();
    void initialize() override;

private:
    static RuntimeObjectPtr get(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr put(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr remove(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr containsKey(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr keys(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr values(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr size(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toList(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr fromList(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr merge(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
