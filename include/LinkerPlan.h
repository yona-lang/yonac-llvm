#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace yona::toolchain {

enum class LinkerMode {
    Auto,
    Bundled,
    System,
    InProcess,
};

struct LinkerPlan {
    LinkerMode requested_mode = LinkerMode::Auto;
    bool use_bundled_lld = false;
    bool use_inprocess_lld = false;
    std::filesystem::path bundled_lld_path;
};

std::string linker_mode_name(LinkerMode mode);
bool parse_linker_mode(const std::string& raw, LinkerMode& out_mode);

std::vector<std::string> bundled_lld_candidate_names();
std::filesystem::path discover_bundled_lld(const std::vector<std::filesystem::path>& sysroots);

bool resolve_linker_plan(const std::string& mode_raw,
                         const std::vector<std::filesystem::path>& sysroots,
                         LinkerPlan& out_plan,
                         std::string& error);

bool inprocess_lld_available();
std::string inprocess_lld_unavailable_reason();
bool require_inprocess_lld_from_env();

} // namespace yona::toolchain
