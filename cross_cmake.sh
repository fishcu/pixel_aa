#!/bin/bash
set -e
cmake . -DBUILD_FOR_MM=ON -B build
cmake --build build -j
