add_subdirectory(camera_rotor)
add_subdirectory(joypad)

if (BUILD_ARM)
    add_subdirectory(servo_controller)
endif ()