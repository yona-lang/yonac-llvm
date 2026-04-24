#include "LinkerPlan.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace yona::toolchain {

static std::string lowercase_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return s;
}

static std::filesystem::path canonical_if_exists(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return {};
    auto c = std::filesystem::weakly_canonical(p, ec);
    return ec ? p : c;
}

std::string linker_mode_name(LinkerMode mode) {
    switch (mode) {
        case LinkerMode::Auto: return "auto";
        case LinkerMode::Bundled: return "bundled";
        case LinkerMode::System: return "system";
        case LinkerMode::InProcess: return "inprocess";
    }
    return "auto";
}

bool parse_linker_mode(const std::string& raw, LinkerMode& out_mode) {
    std::string mode = lowercase_copy(raw);
    if (mode == "auto") {
        out_mode = LinkerMode::Auto;
        return true;
    }
    if (mode == "bundled") {
        out_mode = LinkerMode::Bundled;
        return true;
    }
    if (mode == "system") {
        out_mode = LinkerMode::System;
        return true;
    }
    if (mode == "inprocess" || mode == "in-process") {
        out_mode = LinkerMode::InProcess;
        return true;
    }
    return false;
}

std::vector<std::string> bundled_lld_candidate_names() {
#ifdef _WIN32
    return {"lld-link.exe", "ld.lld.exe"};
#else
    return {"ld.lld", "lld"};
#endif
}

std::filesystem::path discover_bundled_lld(const std::vector<std::filesystem::path>& sysroots) {
    const auto names = bundled_lld_candidate_names();
    for (const auto& root : sysroots) {
        for (const auto& name : names) {
            for (const auto& rel : {std::filesystem::path("bin"), std::filesystem::path("llvm") / "bin"}) {
                auto c = canonical_if_exists(root / rel / name);
                if (!c.empty()) return c;
            }
        }
    }
    return {};
}

bool resolve_linker_plan(const std::string& mode_raw,
                         const std::vector<std::filesystem::path>& sysroots,
                         LinkerPlan& out_plan,
                         std::string& error) {
    std::string normalized = mode_raw.empty() ? "auto" : lowercase_copy(mode_raw);

    LinkerMode requested = LinkerMode::Auto;
    if (!parse_linker_mode(normalized, requested)) {
        error = "invalid linker mode '" + mode_raw + "' (expected auto|bundled|system|inprocess)";
        return false;
    }

    out_plan = {};
    out_plan.requested_mode = requested;
    out_plan.bundled_lld_path = discover_bundled_lld(sysroots);

    if (requested == LinkerMode::Bundled) {
        if (out_plan.bundled_lld_path.empty()) {
            error = "requested bundled linker mode but no bundled lld was found under sysroots";
            return false;
        }
        out_plan.use_bundled_lld = true;
        return true;
    }

    if (requested == LinkerMode::System) {
        out_plan.use_bundled_lld = false;
        return true;
    }

    if (requested == LinkerMode::InProcess) {
        out_plan.use_inprocess_lld = true;
        // External fallback policy mirrors auto mode.
        out_plan.use_bundled_lld = !out_plan.bundled_lld_path.empty();
        return true;
    }

    out_plan.use_bundled_lld = !out_plan.bundled_lld_path.empty();
    return true;
}

bool inprocess_lld_available() {
#if defined(YONA_ENABLE_INPROCESS_LLD) && YONA_ENABLE_INPROCESS_LLD
    return true;
#else
    return false;
#endif
}

std::string inprocess_lld_unavailable_reason() {
    if (inprocess_lld_available()) return {};
    return "this build was compiled without embedded LLD support";
}

bool require_inprocess_lld_from_env() {
    const char* e = std::getenv("YONAC_REQUIRE_INPROCESS_LLD");
    if (!e || !*e) return false;
    std::string v = lowercase_copy(std::string(e));
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

} // namespace yona::toolchain
