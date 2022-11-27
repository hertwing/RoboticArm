add_subdirectory(camera_rotor)
add_subdirectory(joypad_manager)

if (BUILD_ARM)
    add_subdirectory(servo_manager)
endif ()