# Install m4, required for bison
wget https://ftp.gnu.org/gnu/m4/m4-1.4.19.tar.gz
tar -xvzf m4-latest.tar.gz
cd m4-1.4.19/
./configure --prefix=/usr/local/m4
make
make install
export PATH=$PATH:/usr/local/m4/bin

# Install bison, required for gcc
wget http://ftp.gnu.org/gnu/bison/bison-3.8.tar.gz
tar xf bison-3.8.tar.gz
cd bison-3.8
./configure --prefix=/usr/local/bison
make
make install
export PATH=$PATH:/usr/local/bison/bin

# Install texinfo, flex, automake-1.15
apt update
apt install -y texinfo flex automake-1.15

# Set up gcc and g++ to point to cross compiler versions because some of the makefiles ignore the CXX flag
update-alternatives --install /usr/bin/gcc gcc /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc 69
update-alternatives --install /usr/bin/c++ g++ /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-g++ 69
update-alternatives --install /usr/bin/ld ld /opt/miyoomini-toolchain/arm-linux-gnueabihf/bin/ld 69


export PATH=$PATH:/opt/miyoomini-toolchain/usr/bin
export CC=arm-linux-gnueabihf-gcc
export CFLAGS="-Os -marm -march=armv7ve+simd -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
export CXX=arm-linux-gnueabihf-g++
export CXXFLAGS="-Os -marm -march=armv7ve+simd -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
export LDFLAGS=-L/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/lib
export CPPFLAGS=-I/opt/miyoomini-toolchain/lib/gcc/arm-linux-gnueabihf/8.3.0/include:/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include
# ../gcc/configure --enable-bootstrap --host=arm-linux-gnueabihf --target=arm-linux-gnueabihf --prefix=/root/workspace/gcc_src/install_gcc --program-prefix=fc_ --enable-languages=c,c++ --disable-multilib --disable-libstdcxx-pch
../gcc/configure --disable-bootstrap --host=arm-linux-gnueabihf --target=arm-linux-gnueabihf --prefix=/root/workspace/gcc_src/install_gcc --program-prefix=fc_ --enable-languages=c --disable-nls --disable-multilib

make

# During build process: It will trip up when trying to build build-x86_64-pc-linux-gnu/libcpp
# Solution: Go in and do "make" manually, removing CFLAGS as required.
cd build_.../libcpp
make
cd ../..
# Continue build.
make

