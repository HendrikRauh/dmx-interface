# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Clear any stale EXTRA_COMPONENT_DIRS entries (e.g. leftover from previous runs or PlatformIO)
set(EXTRA_COMPONENT_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(dmx-interface)

# Disable filesystem image creation for now
# Can be enabled later when data/ folder is properly configured
if(FALSE)
  if(COMMAND littlefs_create_partition_image)
    littlefs_create_partition_image(storage ${CMAKE_SOURCE_DIR}/data FLASH_IN_PROJECT)
  endif()
endif()
