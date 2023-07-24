#!/bin/bash
set -e

mkdir -p _build_arm
cmake . -DCMAKE_TOOLCHAIN_FILE=/home/isaac/mm_toolchain.cmake -DBUILD_FOR_MM=ON -B _build_arm
VERBOSE=1 cmake --build _build_arm
