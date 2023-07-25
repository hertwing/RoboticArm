project(shmem_handler)

set(HEADERS
    public/ShmemWrapper/ShmemHandler.hpp
    public/ShmemWrapper/DataTypes.h
    )


add_library(library_${PROJECT_NAME}
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