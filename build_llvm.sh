###################
# LLVM COMPILATION
###################

mkdir -p ~/workspace/stage

mkdir -p ~/sysroot/bin
mkdir -p ~/sysroot/include
mkdir -p ~/sysroot/lib
ln -s /opt/miyoomini-toolchain/usr/bin/* ~/sysroot/bin/
ln -s /opt/miyoomini-toolchain/usr/include/* ~/sysroot/include/
ln -s /opt/miyoomini-toolchain/usr/lib/* ~/sysroot/lib/
ln -s /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/bin/* ~/sysroot/bin/
ln -s /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/include/* ~/sysroot/include/
ln -s /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/lib/* ~/sysroot/lib/
ln -s /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/bin/* ~/sysroot/bin/
ln -s /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include/* ~/sysroot/include/
ln -s /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/lib/* ~/sysroot/lib/

apt update
apt install -y libncurses5-dev libssl-dev ninja-build

version=3.27
build=0
mkdir ./cmake_build
cd ./cmake_build
wget https://cmake.org/files/v$version/cmake-$version.$build.tar.gz
tar -xzvf cmake-$version.$build.tar.gz
cd cmake-$version.$build/

./configure
make -j$(nproc)
make install
hash -r

cmake -S llvm -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE="$HOME/workspace/arm_toolchain.cmake" -DCMAKE_BUILD_TYPE="MinSizeRel" -DLLVM_HOST_TRIPLE="arm-linux-gnueabihf" -DLLVM_TARGETS_TO_BUILD="arm" -DLLVM_BUILD_TOOLS="NO" -DLLVM_BUILD_LLVM_DYLIB="YES" -DCMAKE_INSTALL_PREFIX="./install/" -DLLVM_TARGET_ARCH="ARM"
