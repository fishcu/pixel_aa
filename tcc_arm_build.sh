#!/bin/bash
set -e
cd ./deps/tinycc/
./configure --cpu=arm --extra-cflags="-marm -march=armv7ve+simd -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard" --cross-prefix=$CROSS_COMPILE --sysroot=/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc
ln -s /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/lib/* /lib/
make arm-libtcc1-usegcc=yes
make arm-libtcc1-usegcc=yes cross-arm
