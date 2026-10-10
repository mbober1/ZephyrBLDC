# SPDX-License-Identifier: Apache-2.0

add_library(uart_transport OBJECT uart_transport.c beaver_protocol.c)

if(CONFIG_UART_TRANSPORT_GENERIC_API)
	target_sources(uart_transport PRIVATE uart_transport_generic.c)
elseif(CONFIG_UART_TRANSPORT_STM32_API)
	target_sources(uart_transport PRIVATE uart_transport_stm32.c)
endif()

target_include_directories(uart_transport PUBLIC ${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/../comm_router)

target_link_libraries(uart_transport PRIVATE zephyr_interface)

add_dependencies(uart_transport zephyr_generated_headers)