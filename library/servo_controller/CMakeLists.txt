project(servo_controller)

set(HEADERS
    public/PCA9685.h
    public/ServoController.h
    )

set(SOURCES
    private/ServoController.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_link_libraries(library_${PROJECT_NAME}
    PRIVATE pthread
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )