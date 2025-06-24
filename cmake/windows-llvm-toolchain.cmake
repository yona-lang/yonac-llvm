# Toolchain file for building with LLVM on Windows

# Set LLVM installation path
if(NOT DEFINED LLVM_INSTALL_PREFIX)
    set(LLVM_INSTALL_PREFIX "C:/local/LLVM")
endif()

message(STATUS "Using LLVM toolchain for Windows from ${LLVM_INSTALL_PREFIX}")

# Only set compiler paths if not already building with vcpkg
if(NOT VCPKG_TOOLCHAIN)
    # Use regular clang for our project
    set(CMAKE_C_COMPILER "${LLVM_INSTALL_PREFIX}/bin/clang.exe" CACHE FILEPATH "C compiler")
    set(CMAKE_CXX_COMPILER "${LLVM_INSTALL_PREFIX}/bin/clang++.exe" CACHE FILEPATH "C++ compiler")
    set(CMAKE_AR "${LLVM_INSTALL_PREFIX}/bin/llvm-ar.exe" CACHE FILEPATH "Archiver")
    set(CMAKE_LINKER "${LLVM_INSTALL_PREFIX}/bin/lld-link.exe" CACHE FILEPATH "Linker")

    # Ensure CMake uses our LLVM, not Visual Studio's
    set(CMAKE_C_COMPILER_FORCED TRUE)
    set(CMAKE_CXX_COMPILER_FORCED TRUE)
endif()

# First try to find LLVM using standard paths
list(APPEND CMAKE_PREFIX_PATH "${LLVM_INSTALL_PREFIX}")
list(APPEND CMAKE_PREFIX_PATH "${LLVM_INSTALL_PREFIX}/lib/cmake/llvm")

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

# Windows specific flags
if(WIN32)
    if(MSVC)
        # MSVC specific flags
        add_compile_options(/EHsc)  # Enable C++ exceptions
        add_compile_options(/utf-8) # Set source and execution character sets to UTF-8

        # Disable specific warnings
        add_compile_options(/wd4996) # Disable deprecation warnings
    endif()
endif()
