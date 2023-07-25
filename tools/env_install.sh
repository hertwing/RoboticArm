#!/bin/bash

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

mkdir build; cd build; cmake -D BUILD_SHARED=OFF -D BUILD_TEST=ON ..; make -j4; make install; cd ..;

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

# pigpio
# ssh $rpi_username@$rpi_ip sudo apt-get install pigpio
# scp $rpi_username@$rpi_ip:/usr/lib/libpigpio.so.1 /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/
# ln -s /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/libpigpio.so.1 /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/libpigpio.so

# scp $rpi_username@$rpi_ip:/usr/include/pigpio.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/

# Libs
# sudo apt-get install libpthread-stubs0-dev
# sudi apt-get install libboost-all-dev

# WiringPi
if [ ! -d "./WiringPi" ]
then
    git clone https://github.com/WiringPi/WiringPi.git
fi

cd ./WiringPi
git pull origin

export CC=/opt/cross-pi-gcc-10.2.0-64/bin/aarch64-linux-gnu-gcc
export CXX=/opt/cross-pi-gcc-10.2.0-64/bin/aarch64-linux-gnu-g++

cd wiringPi; make -j4; make install; cd ..;
cp ./wiringPi/libwiringPi.so.2.70 /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/
ln -s /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/libwiringPi.so.2.70 /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/libwiringPi.so

#TODO: copy to RPi

cp ./wiringPi/*.h /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/include/

cd devLib; make -j4; make install; cd ..;
cp ./devLib/libwiringPiDev.so.2.70 /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/
ln -s /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/libwiringPiDev.so.2.70 /opt/cross-pi-gcc-10.2.0-64/aarch64-linux-gnu/libc/usr/lib/libwiringPiDev.so
cd ..