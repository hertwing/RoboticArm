project(diagnostic_handler)

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
    PUBLIC library_inet_comm_handler
    )