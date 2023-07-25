project(inet_comm_handler)

set(HEADERS
    public/InetCommData.h
    public/InetCommHandler.h
    )

set(SOURCES
    private/InetCommHandler.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

# target_link_libraries(library_${PROJECT_NAME}
#     PUBLIC library_shmem_handler
#     PUBLIC stdc++fs
#     PUBLIC rt
#     PUBLIC pthread
#     )