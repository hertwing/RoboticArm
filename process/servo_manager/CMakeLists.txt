project(servo_manager)

add_executable(${PROJECT_NAME}
    main.cpp
    private/ServoManager.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_tcp_handler
    PRIVATE library_joypad_handler
    PRIVATE library_servo_controller
    PRIVATE library_led_handler
    )

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticArm)