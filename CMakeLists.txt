cmake_minimum_required(VERSION 3.22)

project(robotic_arm LANGUAGES CXX)

include(CPackConfig.cmake)
include(CPack)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(library)
add_subdirectory(process)
add_subdirectory(startup)

set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lstdc++fscd")

add_compile_options(-Wall -Wextra -Wshadow -Wconversion -Wpedantic
                    -Werror=unused-variable -Werror=unused-parameter)
