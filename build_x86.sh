#!/bin/bash
set -e

mkdir -p _build_x86
cmake . -B _build_x86
VERBOSE=1 cmake --build _build_x86
