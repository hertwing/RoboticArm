#!/bin/bash

PROJECT_PATH=$(pwd)
RPI_TOOLCHAIN_CMAKE=''
BUILD_PATH=~/robotic_arm_build/x86

while [[ $# -gt 0 ]]; do
    case $1 in
    --arm)
        RPI_TOOLCHAIN_CMAKE="-D CMAKE_TOOLCHAIN_FILE=${PROJECT_PATH}/rpi_toolchain.cmake"
        BUILD_PATH=~/robotic_arm_build/arm
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
cmake $RPI_TOOLCHAIN_CMAKE $PROJECT_PATH 