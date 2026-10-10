# SPDX-License-Identifier: Apache-2.0

add_library(telemetry STATIC telemetry.c)
target_include_directories(telemetry PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(telemetry PRIVATE zephyr_interface)
add_dependencies(telemetry zephyr_generated_headers)