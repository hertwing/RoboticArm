add_subdirectory(joypad_manager)

if (BUILD_ARM)
    add_subdirectory(servo_manager)
    add_subdirectory(ultrasonic_sensor_manager)
endif ()