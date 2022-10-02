project(camera_rotor_process)

add_executable(${PROJECT_NAME}
    private/CameraRotorProcess.cpp
    )

target_include_directories(${PROJECT_NAME}
    PUBLIC public
    )

target_link_libraries(${PROJECT_NAME}
    PRIVATE library_camera_rotor
    )