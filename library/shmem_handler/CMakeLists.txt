project(shmem_handler)

set(HEADERS
    public/ShmemHandler.hpp
    public/DataTypes.h
    # private/ShmemHandler.hpp
    )

# set(SOURCES
#     private/ShmemHandler.cpp
#     )

add_library(library_${PROJECT_NAME}
    # ${SOURCES}
    ${HEADERS}
    )

set_target_properties(library_${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

target_link_libraries(library_${PROJECT_NAME}
    PUBLIC stdc++fs
    PUBLIC rt
    PUBLIC pthread
    )