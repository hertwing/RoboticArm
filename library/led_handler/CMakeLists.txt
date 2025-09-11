project(led_handler)

set(HEADERS
    public/odin/led_handler/LedHandler.h
    public/odin/led_handler/DataTypes.h
    )

set(SOURCES
    private/LedHandler.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_link_libraries(library_${PROJECT_NAME}
    PUBLIC library_shmem_handler
    PUBLIC ws2811
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )