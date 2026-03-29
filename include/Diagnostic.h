#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <functional>
#include "SourceLocation.h"
#include "yona_export.h"

namespace yona::compiler {

enum class DiagLevel { Error, Warning, Note };

enum class WarningFlag {
    UnusedVariable,
    UnusedImport,
    Shadow,
    MissingSignature,
    IncompletePatterns,
    OverlappingPatterns,
};

class YONA_API DiagnosticEngine {
public:
    DiagnosticEngine();

    // Configuration
    void enable_wall();                  // -Wall: common warnings
    void enable_wextra();                // -Wextra: all warnings
    void set_warnings_as_errors(bool v); // -Werror
    void suppress_all_warnings();        // -w
    void enable_warning(WarningFlag f);
    void disable_warning(WarningFlag f);

    // Source context for line display
    void set_source(std::string_view source, std::string_view filename);

    // Emission
    void error(const SourceLocation& loc, const std::string& message);
    void warning(const SourceLocation& loc, const std::string& message, WarningFlag flag);
    void note(const SourceLocation& loc, const std::string& message);

    // Counters
    int error_count() const { return error_count_; }
    int warning_count() const { return warning_count_; }
    bool has_errors() const { return error_count_ > 0; }

    // Warning flag name for display (e.g., "unused-variable")
    static std::string flag_name(WarningFlag f);

private:
    void emit(DiagLevel level, const SourceLocation& loc, const std::string& message,
              const std::string& flag_str = "");
    std::string get_source_line(int line) const;

    std::unordered_set<WarningFlag> enabled_warnings_;
    bool warnings_as_errors_ = false;
    bool suppress_warnings_ = false;
    int error_count_ = 0;
    int warning_count_ = 0;

    std::string source_;
    std::string filename_;
    std::vector<size_t> line_offsets_; // byte offset of each line start
};

} // namespace yona::compiler
