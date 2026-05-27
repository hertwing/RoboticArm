#!/bin/bash
set -euo pipefail

PROJECT_PATH=$(pwd)
BUILD_PATH="${PROJECT_PATH}/build/x86"

usage() {
  echo "Use:"
  echo "  --arm               cross-build for Robotic Arm (aarch64)"
  echo "  --gui               cross-build for GUI (aarch64)"
  echo "  -h|--help"
}

SYSROOT=""
EXTRA_ARGS=""
RPI_TOOLCHAIN_CMAKE=""

while [[ $# -gt 0 ]]; do
  case $1 in
    --arm)
      SYSROOT="${HOME}/odin/rpi-sysroot"
      RPI_TOOLCHAIN_CMAKE="-DCMAKE_TOOLCHAIN_FILE=${PROJECT_PATH}/rpi5_aarch64_toolchain.cmake"
      BUILD_PATH="${PROJECT_PATH}/build/arm_aarch64"
      EXTRA_ARGS="${EXTRA_ARGS} -DBUILD_ARM_AARCH64:BOOL=ON"
      CMAKE_PREFIX_PATH_ALL="${SYSROOT}/usr/local:${SYSROOT}/usr"
      ;;
    # --pi5)
    #   SYSROOT="${HOME}/rpi-5-sysroot"
    #   RPI_TOOLCHAIN_CMAKE="-DCMAKE_TOOLCHAIN_FILE=${PROJECT_PATH}/rpi5_aarch64_toolchain.cmake"
    #   BUILD_PATH="${PROJECT_PATH}/build/gui_aarch64"
    #   EXTRA_ARGS="${EXTRA_ARGS} -DBUILD_GUI_AARCH64:BOOL=ON"

    #   QT_TARGET="${HOME}/qt-raspi"
    #   QT_HOST="${HOME}/qt-host"

    #   EXTRA_ARGS+=" -DQT_HOST_PATH=${QT_HOST}"
    #   EXTRA_ARGS+=" -DQt6_DIR=${QT_TARGET}/lib/cmake/Qt6"

    #   OPENCV_DIR_SYSROOT="${SYSROOT}/usr/local/lib/cmake/opencv4"
    #   EXTRA_ARGS+=" -DOpenCV_DIR=${OPENCV_DIR_SYSROOT}"

    #   CMAKE_PREFIX_PATH_ALL="${QT_TARGET}:${SYSROOT}/usr/local:${SYSROOT}/usr"
    #   EXTRA_ARGS+=" -DCMAKE_MODULE_PATH=${HOME}/qt-raspi/lib/cmake/Qt6/3rdparty/extra-cmake-modules/find-modules;${HOME}/qt-raspi/lib/cmake/Qt6/3rdparty/kwin"
    #   ;;
    -h|--help)
      usage;
      exit 0;;
    *)
      echo "Unknown option $1"; exit 1;;
  esac
  shift
done

mkdir -p "$BUILD_PATH"
cd "$BUILD_PATH"

# # PKG-CONFIG w trybie cross (target + sysroot)
# export PKG_CONFIG_DIR=""
# export PKG_CONFIG_SYSROOT_DIR="${SYSROOT}"
# PKG_PATHS=(
#   "${HOME}/qt-raspi/lib/pkgconfig"
#   "${SYSROOT}/usr/local/lib/aarch64-linux-gnu/pkgconfig"
#   "${SYSROOT}/usr/local/lib/pkgconfig"
#   "${SYSROOT}/usr/local/share/pkgconfig"
#   "${SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig"
#   "${SYSROOT}/usr/lib/pkgconfig"
#   "${SYSROOT}/usr/share/pkgconfig"
# )
# export PKG_CONFIG_LIBDIR="$(IFS=:; echo "${PKG_PATHS[*]}")"

# export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
# export PKG_CONFIG_LIBDIR="$HOME/qt-raspi/lib/pkgconfig:$SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig:$SYSROOT/lib/aarch64-linux-gnu/pkgconfig:$SYSROOT/usr/local/lib/aarch64-linux-gnu/pkgconfig:$SYSROOT/usr/local/lib/pkgconfig:$SYSROOT/usr/local/share/pkgconfig:$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"

# pkg-config --modversion egl
# pkg-config --modversion glesv2
# pkg-config --cflags egl glesv2
# pkg-config --libs   egl glesv2


cmake \
  ${RPI_TOOLCHAIN_CMAKE} \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_ALL}" \
  -DCMAKE_BUILD_TYPE=Release \
  ${EXTRA_ARGS} \
  "${PROJECT_PATH}"