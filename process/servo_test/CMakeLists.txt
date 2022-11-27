project(servo_test)

add_executable(${PROJECT_NAME}
    servo.cpp
    )

target_link_libraries(${PROJECT_NAME}
    PUBLIC library_joypad_handler
    PUBLIC servo_library
    PUBLIC wiringPi
    # PUBLIC wiringPiDev
    PUBLIC rt
    crypt
    pthread
    )