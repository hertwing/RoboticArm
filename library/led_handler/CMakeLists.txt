project(led_library)

if (BUILD_ARM)
    include($ENV{ROBOTICARM_TOOLCHAIN_ROOT}/rpi_toolchain.cmake)
endif (BUILD_ARM)

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