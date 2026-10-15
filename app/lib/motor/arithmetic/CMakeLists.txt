# SPDX-License-Identifier: Apache-2.0

if(CONFIG_MOTOR_ARITHMETIC_CORDIC)
	set(ARITHMETIC_SOURCES arithmetic_cordic.c)
elseif(CONFIG_MOTOR_ARITHMETIC_CMSIS_DSP)
	set(ARITHMETIC_SOURCES arithmetic_cmsis_dsp.c)
else()
	set(ARITHMETIC_SOURCES arithmetic_software.c)
endif()

add_library(arithmetic STATIC ${ARITHMETIC_SOURCES})
target_include_directories(arithmetic PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(arithmetic PRIVATE zephyr_interface)
add_dependencies(arithmetic zephyr_generated_headers)
