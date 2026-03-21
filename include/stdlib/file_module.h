#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API FileModule : public NativeModule {
public:
    FileModule();
    void initialize() override;

private:
    static RuntimeObjectPtr exists(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isDir(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr isFile(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr listDir(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr mkdir(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr remove(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr basename(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr dirname(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr extension(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr join(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr absolute(const std::vector<RuntimeObjectPtr>& args);
};

} // namespace yona::stdlib
