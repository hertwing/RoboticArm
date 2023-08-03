cmake_minimum_required(VERSION 3.5)

project(gui VERSION 0.1 LANGUAGES CXX)

if (BUILD_ARM)
    # include($ENV{HOME}/qt-raspi/lib/cmake/Qt6/qt.toolchain.cmake)
    # include($ENV{HOME}/RoboticArm/process/build-gui-RoboGuiKit-Release/.qtc/package-manager/auto-setup.cmake)
    set(TARGET_SYSROOT ${HOME}/rpi-sysroot)
    set(CMAKE_SYSROOT ${TARGET_SYSROOT})

    set(ENV{PKG_CONFIG_PATH} $PKG_CONFIG_PATH:${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig)
    set(ENV{PKG_CONFIG_LIBDIR} /usr/lib/pkgconfig:/usr/share/pkgconfig/:${TARGET_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${TARGET_SYSROOT}/usr/lib/pkgconfig)
    set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
    
    set(CMAKE_TOOLCHAIN_FILE $ENV{HOME}/qt-raspi/lib/cmake/Qt6/qt.toolchain.cmake)
    set(CMAKE_PREFIX_PATH $ENV{HOME}/qt-raspi)
    set(CMAKE_SYSROOT $ENV{HOME}/rpi-sysroot)
    # set(CMAKE_FIND_ROOT_PATH $ENV{HOME}/rpi-sysroot/usr/lib)
    # set(CMAKE_GENERATOR Ninja)
    # set(QT_QMAKE_EXECUTABLE $ENV{HOME}/qt-raspi/bin/qmake)
    # set(QT_DIR $ENV{HOME}/qt-raspi)
    # set(Qt6CoreTools_DIR $ENV{HOME}/qt-host/lib/cmake/Qt6CoreTools)
    # set(Qt6Core_DIR $ENV{HOME}/qt-raspi/lib/cmake/Qt6Core)
    # set(Qt6GuiTools_DIR $ENV{HOME}/qt-host/lib/cmake/Qt6GuiTools)
    # set(Qt6Gui_DIR $ENV{HOME}/qt-raspi/lib/cmake/Qt6Gui)
    # set(Qt6HostInfo_DIR $ENV{HOME}/qt-host/lib/cmake/Qt6HostInfo)
    # set(Qt6WidgetsTools_DIR $ENV{HOME}/qt-host/lib/cmake/Qt6WidgetsTools)
    # set(Qt6Widgets_DIR $ENV{HOME}/qt-raspi/lib/cmake/Qt6Widgets)
    # set(Qt6_DIR $ENV{HOME}/qt-raspi/)
    # set(XKB_INCLUDE_DIR $ENV{HOME}/rpi-sysroot/usr/include)
    # set(XKB_LIBRARY $ENV{HOME}/rpi-sysroot/usr/lib/aarch64-linux-gnu/libxkbcommon.so)
    # set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)
    # set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
    # set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
    set(GLESv2_DIR $ENV{HOME}/rpi-sysroot/usr/lib/aarch64-linux-gnu/libGLESv2.so)
    set(EGL_LIBRARY $ENV{HOME}/rpi-sysroot/usr/lib/aarch64-linux-gnu/libEGL.so)
    set(OPENGL_egl_LIBRARY $ENV{HOME}/rpi-sysroot/usr/lib/aarch64-linux-gnu/libEGL.so)
else()
    set(CMAKE_PREFIX_PATH $ENV{HOME}/qt-host)
endif (BUILD_ARM)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_DEBUG_FIND_PACKAGE ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(gui
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        resources.qrc


    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET gui APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(gui SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(gui
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(gui 
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
    # PRIVATE shmem_handler)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.gui)
endif()
set_target_properties(gui PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS gui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(gui)
endif()
