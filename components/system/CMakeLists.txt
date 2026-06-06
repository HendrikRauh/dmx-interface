idf_component_register(
  SRCS
  "src/system.c"
  INCLUDE_DIRS
  "include"
  REQUIRES
  app_update
  driver
  logger
  esp_timer)

set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/system_version.h")

# 1. Create a dummy file that is never actually created to force the command to
#   run
add_custom_command(
  OUTPUT "${VERSION_HEADER}" "force_version_rebuild"
  COMMAND
    ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DHEADER_FILE=${VERSION_HEADER} -P
    "${CMAKE_CURRENT_LIST_DIR}/gen_version.cmake"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Checking/Generating dynamic system version header..."
  VERBATIM)

# 1. Define the header as a dependency
set_source_files_properties(
  "${CMAKE_CURRENT_BINARY_DIR}/include/system_version.h" PROPERTIES GENERATED
                                                                    TRUE)

# Target to force generation
add_custom_target(generate_version_header DEPENDS "${VERSION_HEADER}")
add_dependencies(${COMPONENT_LIB} generate_version_header)

# Add the binary include directory to the include path
target_include_directories(${COMPONENT_LIB}
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
