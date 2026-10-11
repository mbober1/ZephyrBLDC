# SPDX-License-Identifier: Apache-2.0

add_library(commander OBJECT commander.c)
target_include_directories(commander PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(commander PRIVATE zephyr_interface)