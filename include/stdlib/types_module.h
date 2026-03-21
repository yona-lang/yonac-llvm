#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API TypesModule : public NativeModule {
public:
    TypesModule();
    void initialize() override;

private:
    static RuntimeObjectPtr typeOf(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isInt(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isFloat(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isString(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isBool(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isSeq(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isSet(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isDict(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isTuple(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isFunction(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toInt(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toFloat(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr toStr(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
