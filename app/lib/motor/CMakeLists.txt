add_library(motor STATIC motor.c voltage_control.c utils.c)

if(CONFIG_MOTOR_ARITHMETIC_CORDIC)
	target_sources(motor PRIVATE arithmetic/arithmetic_cordic.c)
elseif(CONFIG_MOTOR_ARITHMETIC_CMSIS_DSP)
	target_sources(motor PRIVATE arithmetic/arithmetic_cmsis_dsp.c)
else()
	target_sources(motor PRIVATE arithmetic/arithmetic_software.c)
endif()

target_include_directories(motor PUBLIC ${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/arithmetic)
target_link_libraries(motor PRIVATE zephyr_interface)
