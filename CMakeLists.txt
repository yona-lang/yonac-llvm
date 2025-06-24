# CMakeList.txt : CMake project for yonac-llvm, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.8...4.0)

# Set policy for MSVC runtime library selection
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(YONAC_LLVM CXX C)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_STRING)
string(STRIP "${VERSION_STRING}" VERSION_STRING)

# Parse version components
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${VERSION_STRING}")
set(YONAC_LLVM_VERSION_MAJOR ${CMAKE_MATCH_1})
set(YONAC_LLVM_VERSION_MINOR ${CMAKE_MATCH_2})
set(YONAC_LLVM_VERSION_PATCH ${CMAKE_MATCH_3})
set(YONAC_LLVM_VERSION_STRING "${VERSION_STRING}")

# Configure version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/version.h"
    @ONLY
)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141 AND MSVC)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif ()

# Use libc++ instead of platform's default STL
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")

    # Use lld linker for faster linking
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld")

    # On Windows, use Clang's runtime libraries instead of MSVC's
    if(WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rtlib=compiler-rt")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -rtlib=compiler-rt")
    endif()

    # On Linux, we need to link against libc++abi
    if(UNIX AND NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -lc++abi")
    endif()
endif()

cmake_policy(SET CMP0167 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    cli11
    QUIET
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.5.0
)

FetchContent_MakeAvailable(cli11)

find_package(LLVM CONFIG REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Setup LLVM
include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})


#set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

file(GLOB all_lib_SRCS
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.c"
)

file(GLOB all_TESTS
        "${PROJECT_SOURCE_DIR}/test/*.h"
        "${PROJECT_SOURCE_DIR}/test/*.cpp"
        "${PROJECT_SOURCE_DIR}/test/*.c"
)

file(GLOB all_repl_SRCS
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/repl/*.cpp"
        "${PROJECT_SOURCE_DIR}/repl/*.c"
)

file(GLOB all_yonac_SRCS
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/cli/*.cpp"
        "${PROJECT_SOURCE_DIR}/cli/*.c"
)

# Add source to this project's executable.
add_library(yona_lib SHARED ${all_lib_SRCS})

target_include_directories(yona_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yona_lib PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(yona_lib PRIVATE ${LLVM_INCLUDE_DIRS})


# Find the libraries that correspond to the LLVM components that we wish to use
llvm_map_components_to_libnames(llvm_libs Support Core IRReader)

target_link_libraries(yona_lib PRIVATE ${llvm_libs})

# For DLL builds on Windows, we need to export symbols
if(WIN32)
    set_target_properties(yona_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET yona_lib PROPERTY CXX_STANDARD 23)
endif ()

# REPL executable
add_executable(yona ${all_repl_SRCS})
target_include_directories(yona PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yona PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(yona PRIVATE yona_lib CLI11::CLI11)

# Compiler executable
add_executable(yonac ${all_yonac_SRCS})
target_include_directories(yonac PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yonac PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(yonac PRIVATE yona_lib CLI11::CLI11 ${llvm_libs})

enable_testing()

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(
        tests
        ${all_TESTS}
)
target_link_libraries(tests PRIVATE yona_lib GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)

gtest_discover_tests(tests)
