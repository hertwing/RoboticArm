SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR arm)
SET(CMAKE_SYSTEM_VERSION 1)
SET(CMAKE_CORSSCOMPILING TRUE)

# Where is the target environment
SET(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)

# TODO: Create scripts for gcc installation
# Specify the cross compiler
SET(CMAKE_C_COMPILER   /usr/bin/arm-linux-gnueabihf-gcc-12)
SET(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++-12)
SET(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)

# Search for programs only in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers only in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_SYSROOT $ENV{HOME}/tools/rootfs/)

link_directories(
    $ENV{HOME}/tools/rootfs/usr/local/lib/
    $ENV{HOME}/tools/rootfs/usr/lib/
    $ENV{HOME}/tools/rootfs/lib/
    $ENV{HOME}/tools/rootfs/lib/gcc/arm-linux-gnueabihf/
)

include_directories(
    $ENV{HOME}/tools/rootfs/usr/include/
    $ENV{HOME}/tools/rootfs/usr/include/arm-linux-gnueabihf/
    $ENV{HOME}/tools/rootfs/usr/local/include/
)