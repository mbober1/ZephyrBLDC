add_subdirectory(arithmetic)
add_subdirectory(control)
add_subdirectory(params)

add_library(motor STATIC motor.c)
target_include_directories(motor PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(motor PRIVATE control arithmetic motor_params zephyr_interface)

if(CONFIG_MOTOR_CALIBRATION)
	add_subdirectory(calibration)
	target_link_libraries(motor PRIVATE calibration)
endif()
