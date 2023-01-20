add_subdirectory(ultrasonic_sensor)
add_subdirectory(joypad)
add_subdirectory(shmem_handler)

if (BUILD_ARM)
    add_subdirectory(servo_controller)
endif ()