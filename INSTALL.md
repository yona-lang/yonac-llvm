# Installing Yona

## Prerequisites

All platforms require:
- **LLVM 16+** (for code generation backend)
- **CMake 3.10+** and **Ninja** (for building from source)
- **C++23 capable compiler** (clang recommended)
- **PCRE2** (optional, for Std\Regex module)

## Linux (Fedora/RHEL)

```bash
# Install dependencies
sudo dnf install llvm llvm-devel llvm-libs llvm-static \
    clang lld cmake ninja-build pcre2-devel

# Build
git clone https://github.com/yona-lang/yonac-llvm.git
cd yonac-llvm
cmake --preset x64-release-linux
cmake --build --preset build-release-linux

# Install (optional)
sudo install -m755 out/build/x64-release-linux/yonac /usr/local/bin/
sudo install -m755 out/build/x64-release-linux/yona /usr/local/bin/
sudo cp -r lib/Std /usr/local/lib/yona/lib/
```

## Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt install llvm-dev clang lld cmake ninja-build libpcre2-dev

# Build
git clone https://github.com/yona-lang/yonac-llvm.git
cd yonac-llvm
cmake --preset x64-release-linux
cmake --build --preset build-release-linux
```

## macOS (Homebrew)

```bash
# Install dependencies
brew install llvm cmake ninja pcre2

# Option A: Build from source
git clone https://github.com/yona-lang/yonac-llvm.git
cd yonac-llvm
cmake --preset x64-release-macos
cmake --build --preset build-release-macos

# Option B: Install via Homebrew (when available)
brew install yona-lang/tap/yona
```

## Windows

```powershell
# Install LLVM from https://releases.llvm.org/
# Install CMake from https://cmake.org/download/
# Install Ninja from https://ninja-build.org/

git clone https://github.com/yona-lang/yonac-llvm.git
cd yonac-llvm
cmake --preset x64-release
cmake --build --preset build-release
```

## Docker

```bash
docker build -t yona .
docker run --rm yona yonac -e '"hello world"'
docker run --rm -it yona  # interactive REPL
```

## Verifying the Installation

```bash
# Compile and run a simple expression
yonac -e '1 + 2'
# Output: 3

# Compile a file
echo 'let fib n = if n <= 1 then n else fib (n-1) + fib (n-2) in fib 10' > fib.yona
yonac fib.yona -o fib
./fib
# Output: 55

# Interactive REPL
yona
```

## Running Tests

```bash
# Via CTest
ctest --preset unit-tests-linux

# Directly
./out/build/x64-release-linux/tests
```
