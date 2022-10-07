project(joypad_handler)

set(HEADERS
    public/JoypadHandler.h
    public/JoypadData.h
    )

set(SOURCES
    private/JoypadHandler.cpp
    private/JoypadData.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

target_link_libraries(library_${PROJECT_NAME}
    PUBLIC c++fs
    )