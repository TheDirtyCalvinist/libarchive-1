#! /bin/bash

# make sure you have NDK + both toolchains (arm, x86)
# android-ndk-r9d/docs/STANDALONE-TOOLCHAIN.html
#$NDK/build/tools/make-standalone-toolchain.sh --platform=android-15 --install-dir=/tmp/my-android-toolchain-x86 --toolchain=x86-4.6  
#$NDK/build/tools/make-standalone-toolchain.sh --platform=android-15 --install-dir=/tmp/my-android-toolchain-arm --toolchain=arm-linux-androideabi-4.6 

HOME=${PWD}
XZ_SRC_PATH=${PWD}/xz-5.2.1

export PATH=/tmp/my-android-toolchain-arm/bin:/tmp/my-android-toolchain-x86/bin:$PATH

# compile arm first
export CC=arm-linux-androideabi-gcc   
export CXX=arm-linux-androideabi-g++ 

cd $XZ_SRC_PATH

./$configure --prefix=${HOME}/lzma --host=arm-linux-androideabi --target=arm-linux-androideabi --enable-static
make clean install

mkdir ${HOME}/armeabi-v7a~
cp -r ${HOME}/lzma  ${HOME}/armeabi-v7a/.

# then x86
export CC=i686-linux-android-gcc  
export CXX=i686-linux-android-g++ 

./configure --prefix=${HOME}/lzma --host=i686-linux-android --target=i686-linux-android--enable-static
make clean install

mkdir ${HOME}/x86
cp -r ${HOME}/lzma  ${HOME}/x86/.

