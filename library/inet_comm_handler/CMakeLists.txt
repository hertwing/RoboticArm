project(inet_comm_handler)

set(HEADERS
    public/InetCommData.h
    public/InetCommHandler.hpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

set_target_properties(library_${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

 target_link_libraries(library_${PROJECT_NAME}
     PUBLIC stdc++fs
     )
