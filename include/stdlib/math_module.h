#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API MathModule : public NativeModule {
public:
    MathModule();
    void initialize() override;

private:
    // Math functions
    static RuntimeObjectPtr sin(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr cos(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr tan(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr asin(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr acos(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr atan(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr atan2(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr exp(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr log(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr log10(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr pow(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr sqrt(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr ceil(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr floor(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr round(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr abs(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr pi(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr e(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
