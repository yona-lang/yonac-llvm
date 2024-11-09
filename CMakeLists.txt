# CMakeList.txt : CMake project for yonac-llvm, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("yonac-llvm" CXX C)
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)

# Set path to generator
set(ANTLR4_JAR_LOCATION ${PROJECT_SOURCE_DIR}/lib/antlr-4.13.2-complete.jar)

find_package(antlr4-runtime CONFIG REQUIRED)
find_package(antlr4-generator REQUIRED)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS program_options) # header only libraries must not be added here

# generate lexer
 antlr4_generate(
   YONA_LEXER
   ${CMAKE_CURRENT_SOURCE_DIR}/parser/YonaLexer.g4
   LEXER
   FALSE
   FALSE
   "yonac"
   )

 # generate parser
 antlr4_generate(
   YONA_PARSER
   ${CMAKE_CURRENT_SOURCE_DIR}/parser/YonaParser.g4
   PARSER
   FALSE
   TRUE
   "yonac"
   "${ANTLR4_TOKEN_FILES_YONA_LEXER}"
   "${ANTLR4_TOKEN_DIRECTORY_YONA_LEXER}"
   )

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.c"
        )

# Add source to this project's executable.
add_executable (yonac-llvm ${all_SRCS} ${ANTLR4_SRC_FILES_YONA_LEXER} ${ANTLR4_SRC_FILES_YONA_PARSER})

target_include_directories(yonac-llvm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(yonac-llvm PRIVATE ${ANTLR4_INCLUDE_DIR})
target_include_directories(yonac-llvm PRIVATE ${ANTLR4_INCLUDE_DIR_YONA_LEXER})
target_include_directories(yonac-llvm PRIVATE ${ANTLR4_INCLUDE_DIR_YONA_PARSER})

add_dependencies(yonac-llvm antlr4_static)

target_link_libraries(yonac-llvm PRIVATE antlr4_static Boost::boost Boost::program_options)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET yonac-llvm PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
