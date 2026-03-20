# Toolchain file for building with LLVM/Clang on Linux
# Uses the system default clang/LLVM and matches its C++ standard library

# --- Compiler ---
set(CMAKE_C_COMPILER "clang" CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER "clang++" CACHE FILEPATH "C++ compiler")

# --- LLVM discovery ---
# Let CMake find LLVM via llvm-config (works on Fedora, Ubuntu, Arch, etc.)
execute_process(
  COMMAND llvm-config --cmakedir
  OUTPUT_VARIABLE _LLVM_CMAKE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(_LLVM_CMAKE_DIR)
  set(LLVM_DIR "${_LLVM_CMAKE_DIR}" CACHE PATH "LLVM CMake directory")
endif()

# --- Linker ---
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "")
set(CMAKE_MODULE_LINKER_FLAGS "-fuse-ld=lld" CACHE STRING "")

# Set RPATH to find libraries at runtime
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
