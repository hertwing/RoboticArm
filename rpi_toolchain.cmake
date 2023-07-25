SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR arm)
SET(CMAKE_SYSTEM_VERSION 1)
SET(CMAKE_CORSSCOMPILING TRUE)

# Where is the target environment
SET(CMAKE_FIND_ROOT_PATH /opt/cross-pi-gcc-10.2.0-64)

# TODO: Create scripts for gcc installation
# Specify the cross compiler
SET(CMAKE_C_COMPILER   /opt/cross-pi-gcc-10.2.0-64/bin/aarch64-linux-gnu-gcc)
SET(CMAKE_CXX_COMPILER /opt/cross-pi-gcc-10.2.0-64/bin/aarch64-linux-gnu-g++)
SET(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

# Search for programs only in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers only in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
