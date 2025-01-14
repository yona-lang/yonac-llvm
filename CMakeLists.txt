﻿# CMakeList.txt : CMake project for yonac-llvm, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif ()
cmake_policy(SET CMP0167 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

project(YONAC_LLVM CXX C)
set(YONAC_LLVM_VERSION_MAJOR 0)
set(YONAC_LLVM_VERSION_MINOR 1)
set(YONAC_LLVM_VERSION_PATCH 0)

# Set path to generator
set(ANTLR4_JAR_LOCATION ${PROJECT_SOURCE_DIR}/lib/antlr-4.13.2-complete.jar)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(antlr4-runtime CONFIG REQUIRED)
find_package(antlr4-generator REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options log tokenizer) # header only libraries must not be added here
find_package(LLVM CONFIG REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})

# generate lexer
antlr4_generate(
        YONA_LEXER
        ${CMAKE_CURRENT_SOURCE_DIR}/parser/YonaLexer.g4
        LEXER
        FALSE
        FALSE
        "yona"
)

#set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

# generate parser
antlr4_generate(
        YONA_PARSER
        ${CMAKE_CURRENT_SOURCE_DIR}/parser/YonaParser.g4
        PARSER
        FALSE
        TRUE
        "yona"
        "${ANTLR4_TOKEN_FILES_YONA_LEXER}"
        "${ANTLR4_TOKEN_DIRECTORY_YONA_LEXER}"
)

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

file(GLOB all_cli_SRCS
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/cli/*.cpp"
        "${PROJECT_SOURCE_DIR}/cli/*.c"
)

# Add source to this project's executable.
add_library(yona SHARED ${all_lib_SRCS} ${ANTLR4_SRC_FILES_YONA_LEXER} ${ANTLR4_SRC_FILES_YONA_PARSER})

target_include_directories(yona PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yona PRIVATE ${ANTLR4_INCLUDE_DIR})
target_include_directories(yona PRIVATE ${ANTLR4_INCLUDE_DIR_YONA_LEXER})
target_include_directories(yona PRIVATE ${ANTLR4_INCLUDE_DIR_YONA_PARSER})
target_include_directories(yona PRIVATE ${LLVM_INCLUDE_DIRS})

# Find the libraries that correspond to the LLVM components that we wish to use
llvm_map_components_to_libnames(llvm_libs Support Core IRReader)

add_dependencies(yona antlr4_static)

target_link_libraries(yona PRIVATE antlr4_static Boost::boost Boost::log ${llvm_libs})

if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET yona PROPERTY CXX_STANDARD 23)
endif ()

add_executable(yonac ${all_cli_SRCS})
target_include_directories(yonac PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yonac PRIVATE ${ANTLR4_INCLUDE_DIR})

target_link_libraries(yonac PRIVATE yona Boost::boost Boost::program_options Boost::log Boost::tokenizer)

enable_testing()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(
        tests
        ${all_TESTS}
)
target_link_libraries(tests PRIVATE yona GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main Boost::boost Boost::log)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(tests PRIVATE ${ANTLR4_INCLUDE_DIR})

gtest_discover_tests(tests)
