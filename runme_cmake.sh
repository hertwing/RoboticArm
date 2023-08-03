#!/bin/bash

PROJECT_PATH=$(pwd)
RPI_TOOLCHAIN_CMAKE=''
ARM_BUILD_FLAG=''
BUILD_PATH="${PROJECT_PATH}/build/x86"

while [[ $# -gt 0 ]]; do
    case $1 in
    --arm)
        RPI_TOOLCHAIN_CMAKE="-DCMAKE_TOOLCHAIN_FILE=${PROJECT_PATH}/rpi_toolchain.cmake"
        BUILD_PATH="${PROJECT_PATH}/build/arm"
        ARM_BUILD_FLAG="-DBUILD_ARM=ON"
        shift # past argument
        ;;
    -h|--help)
        echo "Use --arm argument for RPI build."
        exit 1
        ;;
    -*|--*)
        echo "Unknown option $1"
        exit 1
        ;;
    *)
        echo "Unknown option $1"
        exit 1
        ;;
    esac
done

mkdir -p $BUILD_PATH
cd $BUILD_PATH
cmake $ARM_BUILD_FLAG $PROJECT_PATH 