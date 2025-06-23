# yonac-llvm

A Yona language compiler/interpreter using LLVM as the backend.

## Features

- **Parser**: Custom recursive descent parser with Pratt parsing for expressions
- **AST**: Comprehensive Abstract Syntax Tree with visitor pattern support
- **Interpreter**: Tree-walking interpreter with runtime object system
- **Type System**: Support for primitives, functions, modules, collections
- **Pattern Matching**: Full pattern matching support in case expressions
- **Currying**: Automatic currying of multi-parameter functions
- **List Patterns**: Head/tail destructuring with `[h | t]` syntax

## Setup

### Red Hat / Fedora

```bash
sudo dnf install llvm llvm-devel llvm-libs cmake ninja-build
```

### macOS

```bash
brew install llvm boost vcpkg cmake ninja
```

### Windows

Ensure Visual Studio 2022 or later is installed with C++ development tools.

## Building

### Configure (required before building)

```bash
# macOS
cmake --preset x64-debug-macos    # Debug build
cmake --preset x64-release-macos  # Release build

# Linux
cmake --preset x64-debug-linux    # Debug build
cmake --preset x64-release-linux  # Release build

# Windows
cmake --preset x64-debug         # Debug build
cmake --preset x64-release       # Release build
```

### Build

```bash
# Using CMake presets
cmake --build --preset build-debug-macos    # macOS Debug
cmake --build --preset build-release-macos  # macOS Release
cmake --build --preset build-debug-linux    # Linux Debug
cmake --build --preset build-release-linux  # Linux Release
cmake --build --preset build-debug          # Windows Debug
cmake --build --preset build-release        # Windows Release
```

### Run Tests

```bash
# Using CTest presets
ctest --preset unit-tests-macos    # macOS
ctest --preset unit-tests-linux    # Linux
ctest --preset unit-tests-windows  # Windows

# Or directly
./out/build/x64-debug-macos/tests
```

## Language Features

### Pattern Matching

```yona
case expr of
pattern1 -> body1
pattern2 -> body2
_ -> default
end
```

Supported patterns:
- Wildcards: `_`
- Variables: `x`, `name`
- Lists: `[]`, `[x]`, `[h | t]`
- Tuples: `(x, y)`
- Literals: `42`, `"string"`, `:symbol`

### Currying

Functions are automatically curried:

```yona
let add = \(x) -> \(y) -> x + y in
let add5 = add(5) in
add5(3)  # Returns 8
```

### List Operations

```yona
# List construction
[1, 2, 3]

# Cons operator
1 :: [2, 3]  # Returns [1, 2, 3]

# Pattern matching on lists
case list of
[] -> "empty"
[x] -> "single element"
[h | t] -> "head and tail"
end
```

## Code Formatting

```bash
./scripts/format.sh
```

## Development

See [CLAUDE.md](CLAUDE.md) for detailed development instructions and architecture overview.
