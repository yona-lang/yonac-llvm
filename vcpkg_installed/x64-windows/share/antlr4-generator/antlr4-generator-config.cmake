set(ANTLR_VERSION 4.13.1)


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was antlr4-generator.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

if (NOT ANTLR4_CPP_GENERATED_SRC_DIR)
  set(ANTLR4_GENERATED_SRC_DIR ${CMAKE_BINARY_DIR}/antlr4_generated_src)
endif()

FIND_PACKAGE(Java COMPONENTS Runtime REQUIRED)

#
# The ANTLR generator will output the following files given the input file f.g4
#
# Input  -> f.g4
# Output -> f.h
#        -> f.cpp
#
# the following files will only be produced if there is a parser contained
# Flag -visitor active
# Output -> <f>BaseVisitor.h
#        -> <f>BaseVisitor.cpp
#        -> <f>Visitor.h
#        -> <f>Visitor.cpp
#
# Flag -listener active
# Output -> <f>BaseListener.h
#        -> <f>BaseListener.cpp
#        -> <f>Listener.h
#        -> <f>Listener.cpp
#
# See documentation in markup
#
function(antlr4_generate
    Antlr4_ProjectTarget
    Antlr4_InputFile
    Antlr4_GeneratorType
    )

  set( Antlr4_GeneratedSrcDir ${ANTLR4_GENERATED_SRC_DIR}/${Antlr4_ProjectTarget} )

  get_filename_component(Antlr4_InputFileBaseName ${Antlr4_InputFile} NAME_WE )

  list( APPEND Antlr4_GeneratorStatusMessage "Common Include-, Source- and Tokenfiles" )

  if ( ${Antlr4_GeneratorType} STREQUAL "LEXER")
    set(Antlr4_LexerBaseName "${Antlr4_InputFileBaseName}")
    set(Antlr4_ParserBaseName "")
  else()
    if ( ${Antlr4_GeneratorType} STREQUAL "PARSER")
      set(Antlr4_LexerBaseName "")
      set(Antlr4_ParserBaseName "${Antlr4_InputFileBaseName}")
    else()
      if ( ${Antlr4_GeneratorType} STREQUAL "BOTH")
        set(Antlr4_LexerBaseName "${Antlr4_InputFileBaseName}Lexer")
        set(Antlr4_ParserBaseName "${Antlr4_InputFileBaseName}Parser")
      else()
        message(FATAL_ERROR "The third parameter must be LEXER, PARSER or BOTH")
      endif ()
    endif ()
  endif ()

  # Prepare list of generated targets
  list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}.tokens" )
  list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}.interp" )
  list( APPEND DependentTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}.tokens" )

  if ( NOT ${Antlr4_LexerBaseName} STREQUAL "" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_LexerBaseName}.h" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_LexerBaseName}.cpp" )
  endif ()

  if ( NOT ${Antlr4_ParserBaseName} STREQUAL "" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_ParserBaseName}.h" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_ParserBaseName}.cpp" )
  endif ()

  # process optional arguments ...

  if ( ( ARGC GREATER_EQUAL 4 ) AND ARGV3 )
    set(Antlr4_BuildListenerOption "-listener")

    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}BaseListener.h" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}BaseListener.cpp" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}Listener.h" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}Listener.cpp" )

    list( APPEND Antlr4_GeneratorStatusMessage ", Listener Include- and Sourcefiles" )
  else()
    set(Antlr4_BuildListenerOption "-no-listener")
  endif ()

  if ( ( ARGC GREATER_EQUAL 5 ) AND ARGV4 )
    set(Antlr4_BuildVisitorOption "-visitor")

    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}BaseVisitor.h" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}BaseVisitor.cpp" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}Visitor.h" )
    list( APPEND Antlr4_GeneratedTargets "${Antlr4_GeneratedSrcDir}/${Antlr4_InputFileBaseName}Visitor.cpp" )

    list( APPEND Antlr4_GeneratorStatusMessage ", Visitor Include- and Sourcefiles" )
  else()
    set(Antlr4_BuildVisitorOption "-no-visitor")
  endif ()

  if ( (ARGC GREATER_EQUAL 6 ) AND (NOT ${ARGV5} STREQUAL "") )
    set(Antlr4_NamespaceOption "-package;${ARGV5}")

    list( APPEND Antlr4_GeneratorStatusMessage " in Namespace ${ARGV5}" )
  else()
    set(Antlr4_NamespaceOption "")
  endif ()

  if ( (ARGC GREATER_EQUAL 7 ) AND (NOT ${ARGV6} STREQUAL "") )
    set(Antlr4_AdditionalDependencies ${ARGV6})
  else()
    set(Antlr4_AdditionalDependencies "")
  endif ()

  if ( (ARGC GREATER_EQUAL 8 ) AND (NOT ${ARGV7} STREQUAL "") )
    set(Antlr4_LibOption "-lib;${ARGV7}")

    list( APPEND Antlr4_GeneratorStatusMessage " using Library ${ARGV7}" )
  else()
    set(Antlr4_LibOption "")
  endif ()

  if(NOT Java_FOUND)
    message(FATAL_ERROR "Java is required to process grammar or lexer files! - Use 'FIND_PACKAGE(Java COMPONENTS Runtime REQUIRED)'")
  endif()

  if(NOT EXISTS "${ANTLR4_JAR_LOCATION}")
    message(FATAL_ERROR "Unable to find antlr tool. ANTLR4_JAR_LOCATION:${ANTLR4_JAR_LOCATION}")
  endif()

  # The call to generate the files
  add_custom_command(
    OUTPUT ${Antlr4_GeneratedTargets}
    # Remove target directory
    COMMAND
    ${CMAKE_COMMAND} -E remove_directory ${Antlr4_GeneratedSrcDir}
    # Create target directory
    COMMAND
    ${CMAKE_COMMAND} -E make_directory ${Antlr4_GeneratedSrcDir}
    COMMAND
    # Generate files
    "${Java_JAVA_EXECUTABLE}" -jar "${ANTLR4_JAR_LOCATION}" -Werror -Dlanguage=Cpp ${Antlr4_BuildListenerOption} ${Antlr4_BuildVisitorOption} ${Antlr4_LibOption} ${ANTLR4_GENERATED_OPTIONS} -o "${Antlr4_GeneratedSrcDir}" ${Antlr4_NamespaceOption} "${Antlr4_InputFile}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    MAIN_DEPENDENCY "${Antlr4_InputFile}"
    DEPENDS ${Antlr4_AdditionalDependencies}
    )

  # set output variables in parent scope
  set( ANTLR4_INCLUDE_DIR_${Antlr4_ProjectTarget} ${Antlr4_GeneratedSrcDir} PARENT_SCOPE)
  set( ANTLR4_SRC_FILES_${Antlr4_ProjectTarget} ${Antlr4_GeneratedTargets} PARENT_SCOPE)
  set( ANTLR4_TOKEN_FILES_${Antlr4_ProjectTarget} ${DependentTargets} PARENT_SCOPE)
  set( ANTLR4_TOKEN_DIRECTORY_${Antlr4_ProjectTarget} ${Antlr4_GeneratedSrcDir} PARENT_SCOPE)

  # export generated cpp files into list
  foreach(generated_file ${Antlr4_GeneratedTargets})

    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      set_source_files_properties(
        ${generated_file}
        PROPERTIES
        COMPILE_FLAGS -Wno-overloaded-virtual
        )
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      set_source_files_properties(
        ${generated_file}
        PROPERTIES
        COMPILE_FLAGS -wd4251
        )
    endif ()

  endforeach(generated_file)

message(STATUS "Antlr4 ${Antlr4_ProjectTarget} - Building " ${Antlr4_GeneratorStatusMessage} )

endfunction()
