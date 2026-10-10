# SPDX-License-Identifier: Apache-2.0

add_library(comm_router OBJECT comm_router.c)
target_include_directories(comm_router PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(comm_router PRIVATE zephyr_interface)
add_dependencies(comm_router zephyr_generated_headers)
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR})
zephyr_linker_sources(SECTIONS comm_router_iterable.ld)