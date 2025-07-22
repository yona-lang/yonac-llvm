#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API IOModule : public NativeModule {
public:
    IOModule();
    void initialize() override;

private:
    // IO functions
    static RuntimeObjectPtr print(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr println(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr readFile(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr writeFile(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr appendFile(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr fileExists(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr deleteFile(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr readLine(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr readChar(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
