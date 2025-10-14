project(odin_steps_io_handler)

set(HEADERS
    public/odin/odin_steps_io_handler/OdinStepsIOHandler.h
    )

set(SOURCES
    private/OdinStepsIOHandler.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

target_link_libraries(library_${PROJECT_NAME}
    PUBLIC stdc++fs
    PUBLIC library_inet_data
    )