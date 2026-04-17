#pragma once

#include "yona_test_config.h"
#include <filesystem>

namespace yona::test {

/// Repository root (CMake source directory), valid for all test binaries.
inline std::filesystem::path repo_root() { return std::filesystem::path(YONA_SOURCE_DIR); }

inline std::filesystem::path lib_dir() { return repo_root() / "lib"; }

inline std::filesystem::path src_dir() { return repo_root() / "src"; }

inline std::filesystem::path codegen_fixtures_dir() { return repo_root() / "test" / "codegen"; }

} // namespace yona::test
