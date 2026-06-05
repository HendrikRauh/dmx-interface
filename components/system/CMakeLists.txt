idf_component_register(
  SRCS
  "src/system.c"
  INCLUDE_DIRS
  "include"
  REQUIRES
  driver
  logger
  esp_timer)

set(VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/system_version.h")

# 1. Get the short commit hash (8 characters)
# 2. Check for local modifications (Dirty check) Custom Command to check for
#    version changes on every build
add_custom_command(
  OUTPUT "${VERSION_HEADER}"
  COMMAND
    ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DHEADER_FILE=${VERSION_HEADER} -P
    "${CMAKE_CURRENT_LIST_DIR}/gen_version.cmake"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating dynamic system version header..."
  VERBATIM)

# Target to force generation
add_custom_target(generate_version_header DEPENDS "${VERSION_HEADER}")
add_dependencies(${COMPONENT_LIB} generate_version_header)

# Add the binary include directory to the include path
target_include_directories(${COMPONENT_LIB}
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
