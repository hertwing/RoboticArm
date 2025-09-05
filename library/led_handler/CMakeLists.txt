project(led_library)

set(HEADERS
    public/odin/led_handler/LedHandler.h
    public/odin/led_handler/DataTypes.h
    )

set(SOURCES
    private/LedHandler.cpp
    )

add_library(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_link_libraries(${PROJECT_NAME}
    PUBLIC library_shmem_handler
    PUBLIC ws2811
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC public
    )