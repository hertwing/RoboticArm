project(shmem_handler)

if (BUILD_ARM)
    include($ENV{ROBOTICARM_TOOLCHAIN_ROOT}/rpi_toolchain.cmake)
endif (BUILD_ARM)

set(HEADERS
    public/odin/shmem_wrapper/ShmemHandler.hpp
    public/odin/shmem_wrapper/DataTypes.h
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