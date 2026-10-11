# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bldc_app)

target_sources(app PRIVATE src/main.c)

if(CONFIG_COMMANDER)
	add_subdirectory(lib/commander)
	add_dependencies(commander zephyr_generated_headers)
	target_link_libraries(app PRIVATE commander)
endif()

if(CONFIG_DT_HAS_ST_STM32_SVPWM_ENABLED)
	add_subdirectory(lib/motor)
	add_dependencies(motor zephyr_generated_headers)
	target_link_libraries(app PRIVATE motor)
	if(CONFIG_COMMANDER)
		target_link_libraries(motor PRIVATE commander)
	endif()
endif()

if(CONFIG_COMM_ROUTER)
	add_subdirectory(lib/comm_router)
	target_link_libraries(app PRIVATE comm_router)
endif()

if(CONFIG_UART_TRANSPORT)
	add_subdirectory(lib/uart_transport)
	target_link_libraries(app PRIVATE uart_transport)
endif()

if(CONFIG_TELEMETRY)
	add_subdirectory(lib/telemetry)
	target_link_libraries(app PRIVATE telemetry)
endif()
