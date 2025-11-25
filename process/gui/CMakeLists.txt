cmake_minimum_required(VERSION 3.5)

project(odin_gui VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_DEBUG_FIND_PACKAGE ON)

# Pomóż FindEGL/FindGLESv2 przejść testy kompilacyjne
# (użyje tego CMake w HAVE_EGL/HAVE_GLESv2).
set(CMAKE_REQUIRED_FLAGS     "--sysroot=${CMAKE_SYSROOT}")
set(CMAKE_REQUIRED_INCLUDES  "${CMAKE_SYSROOT}/usr/include")
set(CMAKE_REQUIRED_LIBRARIES
    "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libEGL.so"
    "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libGLESv2.so"
)

# Preseed wartości wyszukiwane przez Find-moduły (jako CACHE),
# żeby nie musiał zgadywać ścieżek:
set(EGL_INCLUDE_DIR     "${CMAKE_SYSROOT}/usr/include"                             CACHE PATH     "EGL include")
set(EGL_LIBRARY         "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libEGL.so"    CACHE FILEPATH "EGL lib")
set(GLESv2_INCLUDE_DIR  "${CMAKE_SYSROOT}/usr/include"                             CACHE PATH     "GLESv2 include")
set(GLESv2_LIBRARY      "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libGLESv2.so" CACHE FILEPATH "GLESv2 lib")

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Charts)
find_package(OpenCV REQUIRED COMPONENTS core imgproc objdetect tracking)

set(HEADERS
    mainwindow.h
    public/ChartsPanel.h
    public/CvWorker.h
    public/OdinStepsModel.h
    public/ScriptedMotionWorker.h
    public/UdpMjpegReceiver.h
    )

set(SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.ui
    private/ChartsPanel.cpp
    private/CvWorker.cpp
    private/OdinStepsModel.cpp
    private/ScriptedMotionWorker.cpp
    private/UdpMjpegReceiver.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${HEADERS}
        ${SOURCES}
        resources.qrc
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED
            ${HEADERS}
            ${SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(${PROJECT_NAME}
            ${HEADERS}
            ${SOURCES}
        )
    endif()
endif()

target_include_directories(${PROJECT_NAME}
        PRIVATE private
        PUBLIC public
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    PRIVATE Qt${QT_VERSION_MAJOR}::Charts
    # TODO: MOVE LATER:
    PRIVATE library_servo_controller
    #
    PRIVATE library_diagnostic_handler
    PRIVATE library_odin_steps_io_handler
    PRIVATE library_shmem_handler
    PRIVATE library_tcp_handler
    PRIVATE library_udp_handler
    PRIVATE library_video_handler
    PRIVATE stdc++fs
    PRIVATE ${OpenCV_LIBS})

set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
set(CMAKE_INSTALL_RPATH "/opt/qt-raspi/lib;/opt/opencv/lib;/usr/local/lib")
set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH   "/opt/qt-raspi/lib;/opt/opencv/lib;/usr/local/lib"
    INSTALL_RPATH "${CMAKE_INSTALL_RPATH}"
)

set(CMAKE_SKIP_INSTALL_RPATH OFF)
set(CMAKE_SKIP_RPATH OFF)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.gui)
endif()
# set_target_properties(${PROJECT_NAME} PROPERTIES
#     ${BUNDLE_ID_OPTION}
#     BUILD_RPATH "/usr/local/lib"
#     INSTALL_RPATH "/usr/local/lib"
#     MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
#     MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
#     MACOSX_BUNDLE TRUE
#     WIN32_EXECUTABLE TRUE
# )

# include(GNUInstallDirs)
# install(TARGETS ${PROJECT_NAME}
#     BUNDLE DESTINATION .
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

install(TARGETS ${PROJECT_NAME} COMPONENT RoboticGui)
