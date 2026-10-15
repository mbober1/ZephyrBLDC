# SPDX-License-Identifier: Apache-2.0

add_library(calibration STATIC calibration.c)
target_include_directories(calibration PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(calibration PRIVATE arithmetic zephyr_interface)
add_dependencies(calibration zephyr_generated_headers)
