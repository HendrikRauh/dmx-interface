cmake_minimum_required(VERSION 3.16)

# Clear any stale EXTRA_COMPONENT_DIRS entries (e.g. leftover from previous runs
# or PlatformIO)
set(EXTRA_COMPONENT_DIRS "")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(dmx-interface)
