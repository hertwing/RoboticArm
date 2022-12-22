project(sonar_process)

add_executable(${PROJECT_NAME}
    private/libSonarSample.cpp
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_sonar
    PUBLIC wiringPi
    PUBLIC rt
    PUBLIC crypt
    PUBLIC pthread
    )