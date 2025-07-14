# Toolchain file for building with LLVM on Windows

# Set LLVM installation path
if(NOT DEFINED LLVM_INSTALL_PREFIX)
    # Check environment variable first (used in CI)
    if(DEFINED ENV{LLVM_INSTALL_PREFIX})
        set(LLVM_INSTALL_PREFIX "$ENV{LLVM_INSTALL_PREFIX}")
    else()
        set(LLVM_INSTALL_PREFIX "C:/local/LLVM")
    endif()
endif()

message(STATUS "Using LLVM toolchain for Windows from ${LLVM_INSTALL_PREFIX}")

# First try to find LLVM using standard paths
list(APPEND CMAKE_PREFIX_PATH "${LLVM_INSTALL_PREFIX}")
list(APPEND CMAKE_PREFIX_PATH "${LLVM_INSTALL_PREFIX}/lib/cmake/llvm")
list(APPEND CMAKE_PREFIX_PATH "${LLVM_INSTALL_PREFIX}/lib/cmake")

# If LLVM cmake files are missing, we need to manually configure LLVM
if(NOT EXISTS "${LLVM_INSTALL_PREFIX}/lib/cmake/llvm/LLVMConfig.cmake")
    message(WARNING "LLVMConfig.cmake not found. Using manual LLVM configuration.")

    # Set up LLVM manually
    set(LLVM_FOUND TRUE)
    set(LLVM_ROOT_DIR "${LLVM_INSTALL_PREFIX}")
    set(LLVM_INCLUDE_DIRS "${LLVM_INSTALL_PREFIX}/include")
    set(LLVM_LIBRARY_DIRS "${LLVM_INSTALL_PREFIX}/lib")
    set(LLVM_BINARY_DIR "${LLVM_INSTALL_PREFIX}/bin")

    # Add LLVM directories
    include_directories(${LLVM_INCLUDE_DIRS})
    link_directories(${LLVM_LIBRARY_DIRS})

    # Common LLVM libraries for Windows
    set(LLVM_AVAILABLE_LIBS
        LLVMCore
        LLVMSupport
        LLVMIRReader
        LLVMBinaryFormat
        LLVMDemangle
        LLVMRemarks
        LLVMBitstreamReader
    )
else()
    # Use the standard LLVM cmake config
    set(LLVM_DIR "${LLVM_INSTALL_PREFIX}/lib/cmake/llvm" CACHE PATH "Path to LLVMConfig.cmake")
endif()

# Set LLVM_DIR in cache for find_package
if(EXISTS "${LLVM_INSTALL_PREFIX}/lib/cmake/llvm/LLVMConfig.cmake")
    set(LLVM_DIR "${LLVM_INSTALL_PREFIX}/lib/cmake/llvm" CACHE PATH "Path to LLVMConfig.cmake" FORCE)
    message(STATUS "Setting LLVM_DIR to ${LLVM_DIR}")
endif()
