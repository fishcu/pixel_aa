#!/bin/bash
set -e

mkdir -p build_arm
cmake . -DBUILD_FOR_MM=ON -B build_arm
VERBOSE=1 cmake --build build_arm
