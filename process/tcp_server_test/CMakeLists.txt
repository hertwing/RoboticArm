project(serv_test)

add_executable(${PROJECT_NAME}
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_inet_comm_handler
    PRIVATE pthread
)