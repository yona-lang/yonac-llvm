# CMakeList.txt : CMake project for yonac-llvm, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

find_package(antlr4-generator REQUIRED)
find_package(antlr4-runtime REQUIRED)

project ("yonac-llvm")

# add runtime include directories on this project.
include_directories( ${ANTLR4_INCLUDE_DIR} )

# add runtime to project dependencies
add_dependencies( yonac-llvm antlr4_shared )

# add runtime to project link libraries
target_link_libraries( yonac-llvm PRIVATE
                       antlr4_shared)

# Add source to this project's executable.
add_executable (yonac-llvm "yonac-llvm.cpp" "yonac-llvm.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET yonac-llvm PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
