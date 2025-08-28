project(servo_library)

if (BUILD_ARM)
    include($ENV{ROBOTICARM_TOOLCHAIN_ROOT}/rpi_toolchain.cmake)
endif (BUILD_ARM)

set(HEADERS
    public/PCA9685.h
    public/ServoController.h
    )

set(SOURCES
    private/ServoController.cpp
    )

add_library(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE pthread
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC public
    )