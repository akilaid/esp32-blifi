# Standalone ESP-IDF example for the blifi component (Phase 2: Wi-Fi core).
cmake_minimum_required(VERSION 3.16)

# Pick up the blifi component from firmware/components/.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blifi_wifi_example)
