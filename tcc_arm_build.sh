#!/bin/bash
set -e
cd ./deps/tinycc/
./configure --cpu=armv7 --extra-cflags="-marm -march=armv7ve+simd -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard" --cc=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc --triplet=arm-linux-gnueabihf --libpaths=/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/lib --sysincludepaths="/opt/miyoomini-toolchain/lib/gcc/arm-linux-gnueabihf/8.3.0/include:/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include:/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include/linux"
make arm-libtcc1-usegcc=yes
make arm-libtcc1-usegcc=yes cross-arm-eabihf
