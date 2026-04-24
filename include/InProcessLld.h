#pragma once

#include <string>
#include <vector>

namespace yona::toolchain {

struct InProcessLldResult {
    bool ok = false;
    int ret_code = 1;
    bool can_run_again = true;
    std::string stdout_text;
    std::string stderr_text;
};

bool run_inprocess_lld(const std::vector<std::string>& args, InProcessLldResult& result);

} // namespace yona::toolchain
