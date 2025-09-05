#!/bin/bash
set -e

PROJECT_PATH=$(pwd)
RPI_TOOLCHAIN_CMAKE=''
BUILD_PATH="${PROJECT_PATH}/build/x86"
EXTRA_ARGS=''

while [[ $# -gt 0 ]]; do
  case $1 in
    --arm)
      RPI_TOOLCHAIN_CMAKE="-DCMAKE_TOOLCHAIN_FILE=${PROJECT_PATH}/rpi_toolchain.cmake"
      BUILD_PATH="${PROJECT_PATH}/build/arm"
      EXTRA_ARGS="${EXTRA_ARGS} -DCMAKE_PREFIX_PATH=$HOME/qt-raspi"
      EXTRA_ARGS="${EXTRA_ARGS} -DQt6_DIR=$HOME/qt-raspi/lib/cmake/Qt6"
      EXTRA_ARGS="${EXTRA_ARGS} -DOpenCV_DIR=$HOME/rpi-sysroot/usr/lib/aarch64-linux-gnu/cmake/opencv4"
      shift;;
    -h|--help)
      echo "Use --arm for RPi build."
      exit 0;;
    *)
      echo "Unknown option $1"; exit 1;;
  esac
done

mkdir -p "$BUILD_PATH"
cd "$BUILD_PATH"

$HOME/qt-raspi/bin/qt-cmake \
  ${RPI_TOOLCHAIN_CMAKE} \
  ${EXTRA_ARGS} \
  -DCMAKE_BUILD_TYPE=Release \
  "$PROJECT_PATH"
