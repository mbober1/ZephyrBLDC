# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(comm_router_test)

target_sources(app PRIVATE test_comm_router.c test_comm_router_support.c test_commander.c
	test_uart_transport.c test_arithmetic.c ../app/lib/commander/commander.c
	../app/lib/comm_router/comm_router.c
	../app/lib/uart_transport/uart_transport.c ../app/lib/uart_transport/beaver_protocol.c
	../app/lib/motor/arithmetic/arithmetic_software.c)
if(CONFIG_UART_TRANSPORT_GENERIC_API)
	target_sources(app PRIVATE ../app/lib/uart_transport/uart_transport_generic.c)
endif()
target_include_directories(app PRIVATE ../app/lib/commander ../app/lib/comm_router
	../app/lib/uart_transport ../app/lib/motor/arithmetic)
zephyr_linker_sources(SECTIONS ../app/lib/comm_router/comm_router_iterable.ld)