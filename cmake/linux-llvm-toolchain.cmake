# Toolchain file for building with LLVM on Linux (Ubuntu/Debian)
# This is used by GitHub Actions and developers who want to use LLVM instead of GCC

# Set the compilers - using versioned binaries as installed by apt
set(CMAKE_C_COMPILER "/usr/bin/clang-20" CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++-20" CACHE FILEPATH "C++ compiler")

# Ensure we use LLVM 20 headers and libraries
set(LLVM_DIR "/usr/lib/llvm-20/lib/cmake/llvm" CACHE PATH "LLVM CMake directory")
set(LLVM_INCLUDE_DIRS "/usr/lib/llvm-20/include" CACHE PATH "LLVM include directory")

# Use LLVM's libc++ instead of GNU libstdc++
set(CMAKE_CXX_FLAGS "-stdlib=libc++" CACHE STRING "")

# Use lld as the linker for faster linking
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld -stdlib=libc++" CACHE STRING "")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld -stdlib=libc++" CACHE STRING "")
set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=lld -stdlib=libc++" CACHE STRING "")

# Ensure we link against libc++abi as well
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lc++abi" CACHE STRING "" FORCE)

# Set RPATH to find libc++ at runtime if it's not in standard locations
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
