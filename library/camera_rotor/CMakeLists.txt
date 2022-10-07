project(camera_rotor)

set(HEADERS
    public/CameraRotor.h
    )

set(SOURCES
    private/CameraRotor.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )