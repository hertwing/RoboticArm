project(led_library)

set(HEADERS
    public/tanos/led_handler/LedHandler.h
    public/tanos/led_handler/DataTypes.h
    )

set(SOURCES
    private/LedHandler.cpp
    )

add_library(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_link_libraries(${PROJECT_NAME}
    PUBLIC ws2811
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC public
    )