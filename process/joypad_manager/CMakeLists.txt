project(joypad_manager)

if (BUILD_ARM)
    include($ENV{ROBOTICARM_TOOLCHAIN_ROOT}/rpi_toolchain.cmake)
endif (BUILD_ARM)

add_executable(${PROJECT_NAME}
    main.cpp
    private/JoypadManager.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_shmem_handler
    PRIVATE library_joypad_handler
    )

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticArm)