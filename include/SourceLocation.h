#pragma once

#include <string>
#include <string_view>
#include <format>

namespace yona {

// Source location information for AST nodes
struct SourceLocation {
    size_t line = 1;
    size_t column = 1;
    size_t offset = 0;
    size_t length = 0;
    std::string_view filename;
    
    [[nodiscard]] std::string to_string() const {
        return std::format("{}:{}:{}", filename, line, column);
    }
    
    [[nodiscard]] bool is_valid() const noexcept {
        return line > 0 && column > 0;
    }
    
    // Create an invalid/unknown location
    static SourceLocation unknown() {
        return {0, 0, 0, 0, "<unknown>"};
    }
    
    // Create a location spanning from start to end
    static SourceLocation span(const SourceLocation& start, const SourceLocation& end) {
        return {
            start.line, 
            start.column, 
            start.offset, 
            end.offset + end.length - start.offset,
            start.filename
        };
    }
};

// For backward compatibility during migration
using SourceContext = SourceLocation;
using SourceInfo = SourceLocation;

// Empty source location for default construction
inline const SourceLocation EMPTY_SOURCE_LOCATION = SourceLocation::unknown();

} // namespace yona