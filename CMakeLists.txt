# CMakeList.txt : CMake project for yonac-llvm, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.8)

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
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif ()
cmake_policy(SET CMP0167 NEW)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost REQUIRED COMPONENTS program_options log) # header only libraries must not be added here

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
        "${PROJECT_SOURCE_DIR}/yonac/*.cpp"
        "${PROJECT_SOURCE_DIR}/yonac/*.c"
)

# Add source to this project's executable.
add_library(yona_lib SHARED ${all_lib_SRCS})

target_include_directories(yona_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yona_lib PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(yona_lib PRIVATE ${LLVM_INCLUDE_DIRS})

# Find the libraries that correspond to the LLVM components that we wish to use
llvm_map_components_to_libnames(llvm_libs Support Core IRReader)

target_link_libraries(yona_lib PRIVATE Boost::boost Boost::log ${llvm_libs})

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET yona_lib PROPERTY CXX_STANDARD 23)
endif ()

# REPL executable
add_executable(yona ${all_repl_SRCS})
target_include_directories(yona PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yona PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(yona PRIVATE yona_lib Boost::boost Boost::program_options Boost::log)

# Compiler executable
add_executable(yonac ${all_yonac_SRCS})
target_include_directories(yonac PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yonac PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_link_libraries(yonac PRIVATE yona_lib Boost::boost Boost::program_options Boost::log ${llvm_libs})


enable_testing()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(
        tests
        ${all_TESTS}
)
target_link_libraries(tests PRIVATE yona_lib GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main Boost::boost Boost::log)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)

gtest_discover_tests(tests)