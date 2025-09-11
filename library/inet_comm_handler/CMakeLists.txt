project(inet_data)

set(HEADERS
    public/InetCommData.h
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

project(tcp_handler)

set(HEADERS
    public/TcpHandler.hpp
    )

add_library(library_${PROJECT_NAME}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

set_target_properties(library_${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(library_${PROJECT_NAME}
    PUBLIC stdc++fs
    PUBLIC library_inet_data
    )

project(udp_handler)

set(HEADERS 
    public/UdpHandler.hpp
    )

add_library(library_${PROJECT_NAME}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

set_target_properties(library_${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)