project(servo_manager)

if (BUILD_ARM)
    include($ENV{ROBOTICARM_TOOLCHAIN_ROOT}/rpi_toolchain.cmake)
endif (BUILD_ARM)

add_executable(${PROJECT_NAME}
    main.cpp
    private/ServoManager.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_joypad_handler
    PRIVATE servo_library
    PRIVATE led_library
    )

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticArm)