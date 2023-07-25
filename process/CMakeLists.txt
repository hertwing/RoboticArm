add_subdirectory(joypad_manager)
add_subdirectory(tcp_server_test)
add_subdirectory(tcp_client_test)

if (BUILD_ARM)
    add_subdirectory(servo_manager)
    # add_subdirectory(led_manager)
    # add_subdirectory(ultrasonic_sensor_manager)
endif ()