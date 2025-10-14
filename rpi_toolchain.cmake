set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_SYSROOT "$ENV{HOME}/rpi-sysroot")
set(CMAKE_QT_RASPI "$ENV{HOME}/qt-raspi")

set(CMAKE_C_COMPILER   /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT};${CMAKE_QT_RASPI}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# CPU/OpenMP for Pi 4 (Cortex-A72)
set(CMAKE_C_FLAGS_INIT   "-O3 -pipe -march=armv8-a+crc+crypto -mtune=cortex-a72 -fopenmp")
set(CMAKE_CXX_FLAGS_INIT "-O3 -pipe -march=armv8-a+crc+crypto -mtune=cortex-a72 -fopenmp")

list(PREPEND CMAKE_PREFIX_PATH
    "${CMAKE_SYSROOT}/usr/local"
    "${CMAKE_SYSROOT}/usr"
)