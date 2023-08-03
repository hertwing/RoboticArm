project(servo_library)

if (BUILD_ARM)
    include($ENV{ROBOTICARM_TOOLCHAIN_ROOT}/rpi_toolchain.cmake)
endif (BUILD_ARM)

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

target_link_libraries(${PROJECT_NAME}
    PRIVATE wiringPi
    PRIVATE rt
    PRIVATE crypt
    PRIVATE pthread
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC public
    )