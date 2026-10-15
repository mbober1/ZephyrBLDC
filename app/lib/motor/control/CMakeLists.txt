# SPDX-License-Identifier: Apache-2.0

add_library(control STATIC voltage_control.c position_control.c speed_control.c)
target_include_directories(control PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(control PRIVATE arithmetic zephyr_interface)
add_dependencies(control zephyr_generated_headers)
