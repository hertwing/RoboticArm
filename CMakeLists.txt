cmake_minimum_required(VERSION 3.22)

project(robotic_arm LANGUAGES CXX)

include(CPackConfig.cmake)
include(CPack)

set(CMAKE_CXX_STANDARD 17)

set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(library)
add_subdirectory(process)
add_subdirectory(startup)

SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lstdc++fscd")