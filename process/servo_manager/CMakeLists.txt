project(servo_manager)

add_executable(${PROJECT_NAME}
    main.cpp
    private/ServoManager.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC public
    )

target_link_libraries(${PROJECT_NAME}
    PUBLIC library_joypad_handler
    PUBLIC servo_library
    PUBLIC wiringPi
    PUBLIC rt
    PUBLIC crypt
    PUBLIC pthread
    )

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticArm)