# SPDX-License-Identifier: Apache-2.0

add_library(motor_params STATIC motor_params.c)
target_include_directories(motor_params PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(motor_params PRIVATE zephyr_interface)
add_dependencies(motor_params zephyr_generated_headers)
