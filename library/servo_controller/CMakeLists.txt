project(servo_library)

set(HEADERS
    public/pca9685.h
    public/ServoController.h
    )

set(SOURCES
    private/pca9685.c
    private/ServoController.cpp
    )

add_library(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC public
    )