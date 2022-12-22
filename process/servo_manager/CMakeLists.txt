project(servo_manager)

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
    PRIVATE wiringPi
    PRIVATE rt
    PRIVATE crypt
    PRIVATE pthread
    )

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticArm)