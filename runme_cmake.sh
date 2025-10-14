#!/bin/bash
set -euo pipefail

PROJECT_PATH=$(pwd)
BUILD_PATH="${PROJECT_PATH}/build/x86"

SYSROOT="${HOME}/rpi-sysroot"
QT_PREFIX="${HOME}/qt-raspi"

RPI_TOOLCHAIN_CMAKE=''
EXTRA_ARGS=''

usage() {
  echo "Use:"
  echo "  --arm               cross-build for RPi (aarch64)"
  echo "  -h|--help"
}

while [[ $# -gt 0 ]]; do
  case $1 in
    --arm)
      RPI_TOOLCHAIN_CMAKE="-DCMAKE_TOOLCHAIN_FILE=${PROJECT_PATH}/rpi_toolchain.cmake"
      BUILD_PATH="${PROJECT_PATH}/build/arm"
      EXTRA_ARGS="${EXTRA_ARGS} -DBUILD_ARM=ON"
      shift;;
    -h|--help)
      usage;
      exit 0;;
    *)
      echo "Unknown option $1"; exit 1;;
  esac
done

mkdir -p "$BUILD_PATH"
cd "$BUILD_PATH"

# ===== SYSROOT & OpenCV =====
OPENCV_DIR_SYSROOT="${SYSROOT}/usr/local/lib/cmake/opencv4"
# if using openCV from RPi repo:
# OPENCV_DIR_SYSROOT="${SYSROOT}/usr/lib/aarch64-linux-gnu/cmake/opencv4"

# ===== CMAKE_PREFIX_PATH =====
CMAKE_PREFIX_PATH_ALL="${QT_PREFIX}:${SYSROOT}/usr/local:${SYSROOT}/usr"

# ===== PKG-CONFIG =====
export PKG_CONFIG_DIR=""
export PKG_CONFIG_SYSROOT_DIR="${SYSROOT}"
PKG_PATHS=()
PKG_PATHS+=("${SYSROOT}/usr/local/lib/aarch64-linux-gnu/pkgconfig"
            "${SYSROOT}/usr/local/lib/pkgconfig"
            "${SYSROOT}/usr/local/share/pkgconfig"
            "${SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig"
            "${SYSROOT}/usr/lib/pkgconfig"
            "${SYSROOT}/usr/share/pkgconfig")
export PKG_CONFIG_LIBDIR="$(IFS=:; echo "${PKG_PATHS[*]}")"

"${QT_PREFIX}/bin/qt-cmake" \
  ${RPI_TOOLCHAIN_CMAKE} \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
  -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH_ALL}" \
  -DQt6_DIR="${QT_PREFIX}/lib/cmake/Qt6" \
  -DOpenCV_DIR="${OPENCV_DIR_SYSROOT}" \
  -DCMAKE_BUILD_TYPE=Release \
  ${EXTRA_ARGS} \
  "${PROJECT_PATH}"