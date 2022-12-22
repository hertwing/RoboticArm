project(sonar_process)

add_executable(${PROJECT_NAME}
    private/libSonarSample.cpp
    )

target_include_directories(${PROJECT_NAME}
    PRIVATE public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_sonar
    PRIVATE wiringPi
    PRIVATE rt
    PRIVATE crypt
    PRIVATE pthread
    )