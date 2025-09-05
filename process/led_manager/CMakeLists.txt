project(led_manager)

add_executable(${PROJECT_NAME}
    main.cpp
    private/LedManager.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE led_library
    )

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticArm)