# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(comm_router_test)

target_sources(app PRIVATE test_comm_router.c ../app/lib/comm_router/comm_router.c
	../app/lib/uart_transport/uart_transport.c ../app/lib/uart_transport/beaver_protocol.c)
if(CONFIG_UART_TRANSPORT_GENERIC_API)
	target_sources(app PRIVATE ../app/lib/uart_transport/uart_transport_generic.c)
endif()
target_include_directories(app PRIVATE ../app/lib/comm_router ../app/lib/uart_transport)
zephyr_linker_sources(SECTIONS ../app/lib/comm_router/comm_router_iterable.ld)