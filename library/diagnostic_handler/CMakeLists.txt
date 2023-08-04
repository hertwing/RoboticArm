project(diagnostic_handler)

if (BUILD_ARM)
    include($ENV{ROBOTICARM_TOOLCHAIN_ROOT}/rpi_toolchain.cmake)
endif (BUILD_ARM)

set(HEADERS
    public/odin/diagnostic_handler/DiagnosticHandler.h
    public/odin/diagnostic_handler/DataTypes.h
    )

set(SOURCES
    private/DiagnosticHandler.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

target_link_libraries(library_${PROJECT_NAME}
    PUBLIC library_shmem_handler
    )