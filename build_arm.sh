#!/bin/bash
set -e

mkdir -p _build_arm
cmake . -DBUILD_FOR_MM=ON -B _build_arm
VERBOSE=1 cmake --build _build_arm
