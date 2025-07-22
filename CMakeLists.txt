# CMakeList.txt : CMake project for yonac-llvm, include source and define
# project specific logic here.
#
cmake_minimum_required(VERSION 3.10..4.0)

# Set policy for MSVC runtime library selection
if(POLICY CMP0091)
	cmake_policy(SET CMP0091 NEW)
endif()

# Ensure consistent MSVC runtime library usage across all targets
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

	# Force all targets to use the same runtime
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MD$<$<CONFIG:Debug>:d>")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MD$<$<CONFIG:Debug>:d>")

	# Enable RTTI and exception handling consistently
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR /EHsc")

	# Remove any conflicting runtime flags
	string(REGEX REPLACE "/M[TD]d?" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REGEX REPLACE "/M[TD]d?" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	string(REGEX REPLACE "/M[TD]d?" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	string(REGEX REPLACE "/M[TD]d?" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()

if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
	set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
endif()

cmake_policy(SET CMP0167 NEW)

project(YONAC_LLVM LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
		"${PROJECT_SOURCE_DIR}/include/stdlib/*.h"
		"${PROJECT_SOURCE_DIR}/src/stdlib/*.cpp"
)

file(GLOB all_TESTS
		"${PROJECT_SOURCE_DIR}/test/*.h"
		"${PROJECT_SOURCE_DIR}/test/*.cpp"
		"${PROJECT_SOURCE_DIR}/test/*.c"
		"${PROJECT_SOURCE_DIR}/test/stdlib/*.h"
		"${PROJECT_SOURCE_DIR}/test/stdlib/*.cpp"
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

# Find the libraries that correspond to the LLVM components that we wish to use
llvm_map_components_to_libnames(llvm_libs Support Core IRReader)

# Add source to this project's executable.
add_library(yona_lib SHARED ${all_lib_SRCS})

# Create a static version of the library for tests only
add_library(yona_lib_static STATIC ${all_lib_SRCS})
target_include_directories(yona_lib_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yona_lib_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(yona_lib_static PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(yona_lib_static PRIVATE ${llvm_libs})
target_compile_definitions(yona_lib_static PRIVATE YONA_STATIC_BUILD)

target_include_directories(yona_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yona_lib PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(yona_lib PRIVATE ${LLVM_INCLUDE_DIRS})

target_link_libraries(yona_lib PRIVATE ${llvm_libs})

# For DLL builds on Windows, we need to export symbols (only for SHARED libraries)
if(WIN32 AND BUILD_SHARED_LIBS)
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
target_link_libraries(yonac PRIVATE yona_lib CLI11::CLI11)

enable_testing()

FetchContent_Declare(
	doctest
	GIT_REPOSITORY https://github.com/doctest/doctest.git
	GIT_TAG        v2.4.12
)

FetchContent_MakeAvailable(doctest)

add_executable(
	tests
	${all_TESTS}
)

# We provide our own main for consistency across all platforms
target_sources(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test/doctest_main.cpp)

# Link with doctest and yona_lib_static for tests
target_link_libraries(tests PRIVATE doctest::doctest yona_lib_static)

# Define YONA_STATIC_BUILD for tests to avoid DLL export/import issues
target_compile_definitions(tests PRIVATE YONA_STATIC_BUILD)

# Include CTest
include(CTest)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Register the test executable with CTest
add_test(NAME doctest_tests COMMAND tests)

# Set working directory for the test to the binary directory
set_tests_properties(doctest_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    LABELS "unit"
    ENVIRONMENT "YONA_PATH=${CMAKE_CURRENT_SOURCE_DIR}/test/code"
)

# Set up test environment
if(APPLE)
	set_tests_properties(doctest_tests PROPERTIES
		ENVIRONMENT "DYLD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}:$ENV{DYLD_LIBRARY_PATH};YONA_PATH=${CMAKE_CURRENT_SOURCE_DIR}/test/code"
	)
elseif(UNIX)
	set_tests_properties(doctest_tests PROPERTIES
		ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_CURRENT_BINARY_DIR}:$ENV{LD_LIBRARY_PATH};YONA_PATH=${CMAKE_CURRENT_SOURCE_DIR}/test/code"
	)
endif()

# Enable doctest discovery for Visual Studio Test Explorer
set_target_properties(tests PROPERTIES
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	VS_DEBUGGER_COMMAND_ARGUMENTS "--reporters=xml"
)

# Add labels for better test organization
set_tests_properties(doctest_tests PROPERTIES LABELS "unit")

# Removed temporary test executables - tests are now using doctest
