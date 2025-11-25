#!/bin/bash

# TODO: Need to rewrite this entirely to use only QT cross-compiler

while getopts u:i:h flag
do
    case "${flag}" in
        u) rpi_username=${OPTARG};;
        i) rpi_ip=${OPTARG};;
        h) echo "To properly install project environment, please connect your RPi\n
                 to network and provide RPi username (-u option) and RPi IP (-i option).\n
                 E.g. ./env_install -u pi -i 192.168.1.2"
           exit;;
       \?) echo "Unknown argument. Please use -h option for help.";;
    esac
done
if [ $OPTIND -eq 1 ]
then 
    echo "No options were passed. Please use -h option for help."
    exit
fi

# Toolchain
if [ ! -d "/opt/cross-pi-gcc-10.2.0-64" ]
then
    if [ ! -f "cross-gcc-10.2.0-pi_64.tar.gz" ]
    then
        wget https://sourceforge.net/projects/raspberry-pi-cross-compilers/files/Bonus%20Raspberry%20Pi%20GCC%2064-Bit%20Toolchains/Raspberry%20Pi%20GCC%2064-Bit%20Cross-Compiler%20Toolchains/Bullseye/GCC%2010.2.0/cross-gcc-10.2.0-pi_64.tar.gz
    fi
    tar -xf cross-gcc-10.2.0-pi_64.tar.gz
    rm -rf cross-gcc-10.2.0-pi_64.tar.gz
fi

sudo cp -r cross-pi-gcc-10.2.0-64 /opt
echo 'export PATH=/opt/cross-pi-gcc-10.2.0-64/bin:$PATH' >> ~/.profile
echo 'export LD_LIBRARY_PATH=/opt/cross-pi-gcc-10.2.0-64/lib:$LD_LIBRARY_PATH' >> ~/.profile
source ~/.profile

echo 'export PATH=/opt/cross-pi-gcc-10.2.0-64/bin:$PATH' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/opt/cross-pi-gcc-10.2.0-64/lib:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc

# rpi_ws281x
if [ ! -d "./rpi_ws281x" ]
then
    git clone https://github.com/jgarff/rpi_ws281x.git
fi

cd ./rpi_ws281x
git pull origin

export CC=/opt/cross-pi-gcc-10.2.0-64/bin/aarch64-linux-gnu-gcc
export CXX=/opt/cross-pi-gcc-10.2.0-64/bin/aarch64-linux-gnu-g++

mkdir build; cd build; cmake -D BUILD_SHARED=OFF -D BUILD_TEST=ON ..; make -j4; cd ..;

cp ./build/libws2811.a /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/
mkdir /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/pkgconfig
cp build/libws2811.pc /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/pkgconfig/

mkdir /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811
cp ws2811.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811/
cp rpihw.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811/
cp pwm.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811/
cp clk.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811/
cp dma.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811/
cp gpio.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811/
cp mailbox.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811/
cp pcm.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/ws2811/
cd ..

# Libs
# sudo apt-get install libpthread-stubs0-dev
# sudi apt-get install libboost-all-dev

#sudo apt update
#sudo apt install -y libopencv-dev python3-opencv

# git clone https://github.com/jgarff/rpi_ws281x.git
# cd rpi_ws281x
# 
# cat > toolchain-arm.cmake <<'EOF'
# set(CMAKE_SYSTEM_NAME Linux)
# set(CMAKE_SYSTEM_PROCESSOR aarch64)

# set(CMAKE_C_COMPILER   /usr/bin/aarch64-linux-gnu-gcc)
# set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)

# set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# EOF
# mkdir -p build-aarch64 && cd build-aarch64
# cmake -S .. -B . \
#   -DCMAKE_TOOLCHAIN_FILE=../toolchain-aarch64.cmake \
#   -DCMAKE_BUILD_TYPE=Release \
#   -DCMAKE_C_FLAGS="-fPIC"          # pas bezpieczeństwa, CMake PIC już włączony
# cmake --build . -j
# cp "$(find . -maxdepth 3 -name libws2811.a | head -n1)" "$HOME/rpi-sysroot/usr/lib/"
# cp ../ws2811.h ../rpihw.h ../pwm.h  $HOME/rpi-sysroot/usr/include/

### Video handling on RPI arm
# sudo apt update
# sudo apt install -y \
#   gstreamer1.0-tools \
#   gstreamer1.0-plugins-base \
#   gstreamer1.0-plugins-good \
#   gstreamer1.0-plugins-bad \
#   gstreamer1.0-libav \
#   libcamera-apps

# rsync -avz pi@roboarm:/lib pi@roboarm:/usr pi@roboarm:/opt rpi-sysroot

### OpenCV on RPI gui
# sudo apt update
# sudo apt -y install build-essential pkg-config \
#   libjpeg-dev libpng-dev libtiff-dev libwebp-dev \
#   libavcodec-dev libavformat-dev libswscale-dev \
#   libv4l-dev v4l-utils \
#   libxvidcore-dev libx264-dev \
#   libgtk-3-dev \
#   libopenblas-dev liblapacke-dev \
#   libtbb-dev \
#   libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
#   libeigen3-dev

# rsync -avz gui@robogui:/lib gui@robogui:/usr gui@robogui:/opt rpi-sysroot

# export RPI_SYSROOT="$HOME/rpi-5-sysroot"

# export PKG_CONFIG_DIR=""
# export PKG_CONFIG_SYSROOT_DIR="$RPI_SYSROOT"
# export PKG_CONFIG_LIBDIR="$RPI_SYSROOT/usr/local/lib/aarch64-linux-gnu/pkgconfig:\
# $RPI_SYSROOT/usr/local/lib/pkgconfig:\
# $RPI_SYSROOT/usr/local/share/pkgconfig:\
# $RPI_SYSROOT/usr/lib/aarch64-linux-gnu/pkgconfig:\
# $RPI_SYSROOT/usr/lib/pkgconfig:\
# $RPI_SYSROOT/usr/share/pkgconfig"

# cd ~
# git clone --branch 4.9.0 --depth 1 https://github.com/opencv/opencv.git
# git clone --branch 4.9.0 --depth 1 https://github.com/opencv/opencv_contrib.git

# mkdir -p ~/build-opencv-aarch64 && cd ~/build-opencv-aarch64

cmake ../opencv \
  -D CMAKE_MAKE_PROGRAM=/usr/bin/make \
  -D BUILD_opencv_python3=OFF \
  -D BUILD_opencv_python_bindings_generator=OFF \
  -D OPENCV_PYTHON_SKIP_DETECTION=ON \
  -D CMAKE_TOOLCHAIN_FILE=/home/piotr/RoboticArm/rpi5_aarch64_toolchain.cmake \
  -D CMAKE_BUILD_TYPE=Release \
  -D CMAKE_INSTALL_PREFIX=/usr/local \
  -D OPENCV_GENERATE_PKGCONFIG=ON \
  -D BUILD_SHARED_LIBS=ON \
  \
  -D WITH_TBB=ON -D BUILD_TBB=OFF \
  -D WITH_OPENMP=ON \
  -D ENABLE_NEON=ON \
  -D WITH_GSTREAMER=ON \
  -D WITH_V4L=ON \
  -D WITH_FFMPEG=ON \
  -D WITH_EIGEN=ON \
  -D WITH_QT=OFF \
  -D WITH_OPENCL=OFF \
  \
  -D BUILD_TESTS=OFF -D BUILD_PERF_TESTS=OFF -D BUILD_EXAMPLES=OFF \
  -D WITH_JPEG=ON  -D BUILD_JPEG=OFF \
  -D WITH_PNG=ON   -D BUILD_PNG=OFF \
  -D WITH_TIFF=ON  -D BUILD_TIFF=OFF \
  -D WITH_WEBP=ON  -D BUILD_WEBP=OFF \
  \
  -D EIGEN_INCLUDE_PATH=${RPI_SYSROOT}/usr/include/eigen3 \
  -D OPENCV_EXTRA_MODULES_PATH=~/opencv_contrib/modules

make -j$(nproc)
DESTDIR="${RPI_SYSROOT}" make install

# export RPI_SYSROOT="$HOME/rpi-sysroot"
# export RPI_GUI_HOST="gui@192.168.1.103"
# tar -C "$RPI_SYSROOT" -czf opencv-aarch64-4.9.0.tgz usr/local
# scp opencv-aarch64-4.9.0.tgz "$RPI_GUI_HOST:~/"
# ssh "$RPI_GUI_HOST" 'sudo tar -C / -xzf ~/opencv-aarch64-4.9.0.tgz'