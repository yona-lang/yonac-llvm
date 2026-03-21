#pragma once

#include "stdlib/native_module.h"

namespace yona::stdlib {

class YONA_API HttpModule : public NativeModule {
public:
    HttpModule();
    void initialize() override;

private:
    static RuntimeObjectPtr get(const std::vector<RuntimeObjectPtr>& args);
    static RuntimeObjectPtr post(const std::vector<RuntimeObjectPtr>& args);

    // Internal HTTP request implementation
    static RuntimeObjectPtr http_request(const std::string& method, const std::string& url, const std::string& body);
};

} // namespace yona::stdlib
