#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <cstdio>
#include <cstdlib>

#include <doctest/doctest.h>

#include "LinkerPlan.h"
#include "InProcessLld.h"

namespace fs = std::filesystem;

TEST_SUITE("LinkerPlan") {

TEST_CASE("parse linker mode accepts valid values") {
    using yona::toolchain::LinkerMode;
    LinkerMode mode = LinkerMode::Auto;
    CHECK(yona::toolchain::parse_linker_mode("auto", mode));
    CHECK(mode == LinkerMode::Auto);
    CHECK(yona::toolchain::parse_linker_mode("bundled", mode));
    CHECK(mode == LinkerMode::Bundled);
    CHECK(yona::toolchain::parse_linker_mode("system", mode));
    CHECK(mode == LinkerMode::System);
    CHECK(yona::toolchain::parse_linker_mode("inprocess", mode));
    CHECK(mode == LinkerMode::InProcess);
    CHECK(yona::toolchain::parse_linker_mode("in-process", mode));
    CHECK(mode == LinkerMode::InProcess);
}

TEST_CASE("parse linker mode rejects unknown value") {
    using yona::toolchain::LinkerMode;
    LinkerMode mode = LinkerMode::Auto;
    CHECK_FALSE(yona::toolchain::parse_linker_mode("unknown", mode));
}

TEST_CASE("linker mode name round-trips expected spellings") {
    using yona::toolchain::LinkerMode;
    CHECK(yona::toolchain::linker_mode_name(LinkerMode::Auto) == "auto");
    CHECK(yona::toolchain::linker_mode_name(LinkerMode::Bundled) == "bundled");
    CHECK(yona::toolchain::linker_mode_name(LinkerMode::System) == "system");
    CHECK(yona::toolchain::linker_mode_name(LinkerMode::InProcess) == "inprocess");
}

TEST_CASE("parse linker mode is case-insensitive") {
    using yona::toolchain::LinkerMode;
    LinkerMode mode = LinkerMode::Auto;
    CHECK(yona::toolchain::parse_linker_mode("InPrOcEsS", mode));
    CHECK(mode == LinkerMode::InProcess);
}

TEST_CASE("resolve system mode never enables bundled linker") {
    yona::toolchain::LinkerPlan plan;
    std::string error;
    const std::vector<fs::path> roots = {};
    REQUIRE(yona::toolchain::resolve_linker_plan("system", roots, plan, error));
    CHECK_FALSE(plan.use_bundled_lld);
    CHECK(plan.requested_mode == yona::toolchain::LinkerMode::System);
}

TEST_CASE("resolve bundled mode requires bundled lld in sysroots") {
    yona::toolchain::LinkerPlan plan;
    std::string error;
    const std::vector<fs::path> roots = {};
    CHECK_FALSE(yona::toolchain::resolve_linker_plan("bundled", roots, plan, error));
    CHECK(error.find("bundled") != std::string::npos);
}

TEST_CASE("auto mode picks bundled lld when discovered") {
    auto tmp_base = fs::temp_directory_path() / "yona_linker_plan_test_auto";
    fs::remove_all(tmp_base);
    fs::create_directories(tmp_base / "bin");

    const auto candidates = yona::toolchain::bundled_lld_candidate_names();
    REQUIRE(!candidates.empty());
    const fs::path lld_path = tmp_base / "bin" / candidates.front();
    {
        std::ofstream out(lld_path.string(), std::ios::binary);
        REQUIRE(out.good());
        out << "stub";
    }

    yona::toolchain::LinkerPlan plan;
    std::string error;
    const std::vector<fs::path> roots = {tmp_base};
    REQUIRE(yona::toolchain::resolve_linker_plan("auto", roots, plan, error));
    CHECK(plan.requested_mode == yona::toolchain::LinkerMode::Auto);
    CHECK(plan.use_bundled_lld);
    CHECK_FALSE(plan.bundled_lld_path.empty());

    fs::remove_all(tmp_base);
}

TEST_CASE("discover bundled lld also checks llvm/bin") {
    auto tmp_base = fs::temp_directory_path() / "yona_linker_plan_test_llvm_bin";
    fs::remove_all(tmp_base);
    fs::create_directories(tmp_base / "llvm" / "bin");

    const auto candidates = yona::toolchain::bundled_lld_candidate_names();
    REQUIRE(!candidates.empty());
    const fs::path lld_path = tmp_base / "llvm" / "bin" / candidates.front();
    {
        std::ofstream out(lld_path.string(), std::ios::binary);
        REQUIRE(out.good());
        out << "stub";
    }

    const fs::path found = yona::toolchain::discover_bundled_lld({tmp_base});
    CHECK_FALSE(found.empty());
    CHECK(found.filename() == lld_path.filename());
    fs::remove_all(tmp_base);
}

TEST_CASE("inprocess mode resolves and keeps external fallback policy") {
    yona::toolchain::LinkerPlan plan;
    std::string error;
    const std::vector<fs::path> roots = {};
    REQUIRE(yona::toolchain::resolve_linker_plan("inprocess", roots, plan, error));
    CHECK(plan.requested_mode == yona::toolchain::LinkerMode::InProcess);
    CHECK(plan.use_inprocess_lld);
    CHECK_FALSE(plan.use_bundled_lld);
}

TEST_CASE("inprocess mode keeps bundled fallback when bundled lld exists") {
    auto tmp_base = fs::temp_directory_path() / "yona_linker_plan_test_inprocess_bundled";
    fs::remove_all(tmp_base);
    fs::create_directories(tmp_base / "bin");
    const auto candidates = yona::toolchain::bundled_lld_candidate_names();
    REQUIRE(!candidates.empty());
    {
        std::ofstream out((tmp_base / "bin" / candidates.front()).string(), std::ios::binary);
        REQUIRE(out.good());
        out << "stub";
    }

    yona::toolchain::LinkerPlan plan;
    std::string error;
    REQUIRE(yona::toolchain::resolve_linker_plan("inprocess", {tmp_base}, plan, error));
    CHECK(plan.use_inprocess_lld);
    CHECK(plan.use_bundled_lld);
    CHECK_FALSE(plan.bundled_lld_path.empty());
    fs::remove_all(tmp_base);
}

} // TEST_SUITE("LinkerPlan")

TEST_SUITE("InProcessLld") {

TEST_CASE("inprocess availability API is self-consistent") {
    if (yona::toolchain::inprocess_lld_available()) {
        CHECK(yona::toolchain::inprocess_lld_unavailable_reason().empty());
    } else {
        CHECK_FALSE(yona::toolchain::inprocess_lld_unavailable_reason().empty());
    }
}

TEST_CASE("inprocess runner reports unavailable backend clearly") {
    if (yona::toolchain::inprocess_lld_available()) {
        // Availability is toolchain-dependent; this branch is validated by CI
        // smoke tests that compile+run with --linker-mode inprocess.
        CHECK(true);
        return;
    }
    yona::toolchain::InProcessLldResult res;
    const bool ok = yona::toolchain::run_inprocess_lld({}, res);
    CHECK_FALSE(ok);
    CHECK_FALSE(res.stderr_text.empty());
}

TEST_CASE("require inprocess env parser accepts common truthy values") {
#ifdef _WIN32
    _putenv_s("YONAC_REQUIRE_INPROCESS_LLD", "1");
#else
    setenv("YONAC_REQUIRE_INPROCESS_LLD", "1", 1);
#endif
    CHECK(yona::toolchain::require_inprocess_lld_from_env());
#ifdef _WIN32
    _putenv_s("YONAC_REQUIRE_INPROCESS_LLD", "true");
#else
    setenv("YONAC_REQUIRE_INPROCESS_LLD", "true", 1);
#endif
    CHECK(yona::toolchain::require_inprocess_lld_from_env());
#ifdef _WIN32
    _putenv_s("YONAC_REQUIRE_INPROCESS_LLD", "on");
#else
    setenv("YONAC_REQUIRE_INPROCESS_LLD", "on", 1);
#endif
    CHECK(yona::toolchain::require_inprocess_lld_from_env());
}

TEST_CASE("require inprocess env parser treats missing/false as disabled") {
#ifdef _WIN32
    _putenv_s("YONAC_REQUIRE_INPROCESS_LLD", "0");
#else
    setenv("YONAC_REQUIRE_INPROCESS_LLD", "0", 1);
#endif
    CHECK_FALSE(yona::toolchain::require_inprocess_lld_from_env());
#ifdef _WIN32
    _putenv_s("YONAC_REQUIRE_INPROCESS_LLD", "");
#else
    unsetenv("YONAC_REQUIRE_INPROCESS_LLD");
#endif
    CHECK_FALSE(yona::toolchain::require_inprocess_lld_from_env());
}

} // TEST_SUITE("InProcessLld")
