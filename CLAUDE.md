# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Configure build (required before building)
```bash
# For macOS
cmake --preset x64-debug-macos    # Debug build
cmake --preset x64-release-macos  # Release build

# For Linux
cmake --preset x64-debug-linux    # Debug build
cmake --preset x64-release-linux  # Release build

# For Windows
cmake --preset x64-debug         # Debug build
cmake --preset x64-release       # Release build
```

### Build the project
```bash
# Using CMake presets
cmake --build --preset build-debug-macos    # macOS Debug
cmake --build --preset build-release-macos  # macOS Release
cmake --build --preset build-debug-linux    # Linux Debug
cmake --build --preset build-release-linux  # Linux Release
cmake --build --preset build-debug          # Windows Debug
cmake --build --preset build-release        # Windows Release

# Or directly from build directory
cmake --build out/build/x64-debug-macos
```

### Run tests
```bash
# Using CTest presets
ctest --preset unit-tests-macos    # macOS
ctest --preset unit-tests-linux    # Linux
ctest --preset unit-tests-windows  # Windows

# Or directly
./out/build/x64-debug-macos/tests

# Run specific test
./out/build/x64-debug-macos/tests --gtest_filter="TestName"
```

### Format code
```bash
./scripts/format.sh
```

## Architecture Overview

This is a Yona language compiler/interpreter project using LLVM as the backend. The codebase follows a traditional compiler architecture:

### Core Components

1. **Lexer** (`include/Lexer.h`, `src/Lexer.cpp`)
   - Custom lexer implementation with proper tokenization
   - Handles special tokens like underscore `_` for wildcards
   - Location tracking for error reporting

2. **Parser** (`include/Parser.h`, `src/Parser.cpp`)
   - Recursive descent parser with Pratt parsing for expressions
   - Context-aware parsing for pattern matching
   - Produces AST nodes defined in `include/ast.h`

3. **AST** (`include/ast.h`, `src/ast.cpp`)
   - Comprehensive AST node hierarchy with visitor pattern support
   - Base class `AstNode` with various expression and pattern node types
   - Type system integrated via `include/types.h`

4. **Interpreter** (`include/Interpreter.h`, `src/Interpreter.cpp`)
   - Tree-walking interpreter implementing `AstVisitor`
   - Runtime object system defined in `include/runtime.h`
   - Frame-based execution with symbol management
   - Pattern matching implementation with proper variable binding

5. **Code Generation** (`include/Codegen.h`, `src/Codegen.cpp`)
   - LLVM-based code generation (work in progress)
   - Will generate LLVM IR from AST

6. **Type System** (`include/types.h`)
   - Supports various types including primitives, functions, modules, collections
   - Type inference and checking capabilities

### Key Dependencies

- **LLVM**: For code generation backend
- **Boost**: For program options and logging
- **GTest**: For unit testing framework
- **vcpkg**: For C++ dependency management

### Build System

- Uses CMake with presets for different platforms (Windows, Linux, macOS)
- vcpkg integration for dependency management
- Platform-specific compiler configurations (MSVC on Windows, Clang on Linux/macOS)

### Development Workflow

1. AST modifications should update both header and visitor methods
2. New language features typically require updates to: lexer → parser → AST → interpreter/codegen
3. Tests use GTest framework and are located in `test/` directory
4. Pattern matching requires careful handling of variable binding and frame management

### Recent Changes

- **Lexer**: Fixed tokenization of single underscore `_` to return UNDERSCORE token instead of identifier
- **Parser**: Added context-aware parsing for pattern matching to handle expression boundaries correctly
- **AST**: Restructured pattern matching with new `CaseClause` class that properly associates patterns with body expressions
- **Interpreter**: Fixed pattern matching to return correct body values and properly handle list patterns `[h | t]`
- **Syntax**: Case expressions don't use pipes (`|`) before patterns, following Yona language specification