project(video_transfer_manager)

add_executable(${PROJECT_NAME}
    main.cpp
    private/VideoTransferManager.cpp
    )

target_include_directories(${PROJECT_NAME}
    PRIVATE public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_video_handler
    PRIVATE pthread
    )

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticArm)