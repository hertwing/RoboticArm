project(video_handler)

set(HEADERS
    public/odin/video_handler/VideoHandler.h
    )

set(SOURCES
    private/VideoHandler.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

target_link_libraries(library_${PROJECT_NAME}
    PUBLIC library_udp_handler
    PUBLIC stdc++fs
    )
