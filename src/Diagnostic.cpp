#include "Diagnostic.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

namespace yona::compiler {

namespace {

bool stderr_is_tty() {
    static const bool is_tty = isatty(fileno(stderr));
    return is_tty;
}

const char* color_reset()   { return stderr_is_tty() ? "\033[0m"  : ""; }
const char* color_bold()    { return stderr_is_tty() ? "\033[1m"  : ""; }
const char* color_red()     { return stderr_is_tty() ? "\033[1;31m" : ""; }
const char* color_yellow()  { return stderr_is_tty() ? "\033[1;35m" : ""; }
const char* color_cyan()    { return stderr_is_tty() ? "\033[1;36m" : ""; }
const char* color_green()   { return stderr_is_tty() ? "\033[1;32m" : ""; }

} // anonymous namespace

DiagnosticEngine::DiagnosticEngine() = default;

void DiagnosticEngine::enable_wall() {
    enable_warning(WarningFlag::UnusedVariable);
    enable_warning(WarningFlag::IncompletePatterns);
    enable_warning(WarningFlag::OverlappingPatterns);
}

void DiagnosticEngine::enable_wextra() {
    enable_wall();
    enable_warning(WarningFlag::Shadow);
    enable_warning(WarningFlag::MissingSignature);
    enable_warning(WarningFlag::UnusedImport);
}

void DiagnosticEngine::set_warnings_as_errors(bool v) {
    warnings_as_errors_ = v;
}

void DiagnosticEngine::suppress_all_warnings() {
    suppress_warnings_ = true;
}

void DiagnosticEngine::enable_warning(WarningFlag f) {
    enabled_warnings_.insert(f);
}

void DiagnosticEngine::disable_warning(WarningFlag f) {
    enabled_warnings_.erase(f);
}

void DiagnosticEngine::set_source(std::string_view source, std::string_view filename) {
    source_ = std::string(source);
    filename_ = std::string(filename);

    // Build line offset index
    line_offsets_.clear();
    line_offsets_.push_back(0); // line 1 starts at offset 0
    for (size_t i = 0; i < source_.size(); ++i) {
        if (source_[i] == '\n') {
            line_offsets_.push_back(i + 1);
        }
    }
}

void DiagnosticEngine::error(const SourceLocation& loc, const std::string& message) {
    ++error_count_;
    emit(DiagLevel::Error, loc, message);
}

void DiagnosticEngine::warning(const SourceLocation& loc, const std::string& message, WarningFlag flag) {
    if (suppress_warnings_) return;
    if (enabled_warnings_.find(flag) == enabled_warnings_.end()) return;

    std::string flag_str = flag_name(flag);

    if (warnings_as_errors_) {
        ++error_count_;
        emit(DiagLevel::Error, loc, message, flag_str);
    } else {
        ++warning_count_;
        emit(DiagLevel::Warning, loc, message, flag_str);
    }
}

void DiagnosticEngine::note(const SourceLocation& loc, const std::string& message) {
    emit(DiagLevel::Note, loc, message);
}

std::string DiagnosticEngine::flag_name(WarningFlag f) {
    switch (f) {
        case WarningFlag::UnusedVariable:     return "unused-variable";
        case WarningFlag::UnusedImport:       return "unused-import";
        case WarningFlag::Shadow:             return "shadow";
        case WarningFlag::MissingSignature:   return "missing-signature";
        case WarningFlag::IncompletePatterns: return "incomplete-patterns";
        case WarningFlag::OverlappingPatterns:return "overlapping-patterns";
    }
    return "unknown";
}

void DiagnosticEngine::emit(DiagLevel level, const SourceLocation& loc, const std::string& message,
                            const std::string& flag_str) {
    // Determine level string and color
    const char* level_color = "";
    const char* level_str = "";
    switch (level) {
        case DiagLevel::Error:
            level_color = color_red();
            level_str = "error";
            break;
        case DiagLevel::Warning:
            level_color = color_yellow();
            level_str = "warning";
            break;
        case DiagLevel::Note:
            level_color = color_cyan();
            level_str = "note";
            break;
    }

    // Use the filename from the diagnostic engine if the location doesn't have one
    std::string_view fname = loc.filename.empty() ? std::string_view(filename_) : loc.filename;

    // Print: filename:line:col: level: message [-Wflag]
    std::fprintf(stderr, "%s%s%s:%zu:%zu: %s%s%s: %s",
                 color_bold(), fname.data(), color_reset(),
                 loc.line, loc.column,
                 level_color, level_str, color_reset(),
                 message.c_str());

    if (!flag_str.empty()) {
        std::fprintf(stderr, " %s[-W%s]%s", level_color, flag_str.c_str(), color_reset());
    }
    std::fprintf(stderr, "\n");

    // Show source line with caret if we have source context
    if (!source_.empty() && loc.line > 0 && loc.line <= line_offsets_.size()) {
        std::string src_line = get_source_line(static_cast<int>(loc.line));
        if (!src_line.empty()) {
            // Print the source line with line number
            std::fprintf(stderr, " %4zu | %s\n", loc.line, src_line.c_str());

            // Print the caret line
            std::fprintf(stderr, "      | ");
            // Spaces up to the column, then caret
            for (size_t i = 1; i < loc.column; ++i) {
                // Preserve tabs for alignment
                if (i <= src_line.size() && src_line[i - 1] == '\t') {
                    std::fputc('\t', stderr);
                } else {
                    std::fputc(' ', stderr);
                }
            }
            std::fprintf(stderr, "%s^", color_green());
            // Underline the rest of the token length if available
            if (loc.length > 1) {
                for (size_t i = 1; i < loc.length; ++i) {
                    std::fputc('~', stderr);
                }
            }
            std::fprintf(stderr, "%s\n", color_reset());
        }
    }
}

std::string DiagnosticEngine::get_source_line(int line) const {
    if (line < 1 || static_cast<size_t>(line) > line_offsets_.size()) {
        return "";
    }

    size_t start = line_offsets_[line - 1];
    size_t end;
    if (static_cast<size_t>(line) < line_offsets_.size()) {
        end = line_offsets_[line] - 1; // exclude the newline
    } else {
        end = source_.size();
    }

    if (start >= source_.size()) return "";
    if (end > source_.size()) end = source_.size();

    return source_.substr(start, end - start);
}

} // namespace yona::compiler
