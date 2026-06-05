execute_process(
  COMMAND git rev-parse --short=7 HEAD
  WORKING_DIRECTORY ${SOURCE_DIR}
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

if(NOT GIT_HASH)
  set(GIT_HASH "unknown")
endif()

execute_process(
  COMMAND git status --porcelain
  WORKING_DIRECTORY ${SOURCE_DIR}
  OUTPUT_VARIABLE GIT_STATUS
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT "${GIT_STATUS}" STREQUAL "")
  set(VERSION_STR "${GIT_HASH}-d")
else()
  set(VERSION_STR "${GIT_HASH}")
endif()

# Only write the header if it has changed (saves build time)
set(HEADER_CONTENT "#pragma once\n#define SYS_VERSION \"${VERSION_STR}\"\n")
if(EXISTS "${HEADER_FILE}")
  file(READ "${HEADER_FILE}" EXISTING_CONTENT)
endif()

if(NOT "${HEADER_CONTENT}" STREQUAL "${EXISTING_CONTENT}")
  file(WRITE "${HEADER_FILE}" "${HEADER_CONTENT}")
endif()
