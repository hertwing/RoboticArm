project(joypad_manager)

add_executable(${PROJECT_NAME}
    main.cpp
    private/JoypadManager.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_joypad_handler
    )

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticArm)