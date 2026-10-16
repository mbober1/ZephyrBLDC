# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(calibration_app)

add_subdirectory(../../app/lib/motor/arithmetic arithmetic)
add_subdirectory(../../app/lib/motor/calibration calibration)

target_sources(app PRIVATE src/main.c src/calib_store.c)
target_link_libraries(app PRIVATE calibration)
