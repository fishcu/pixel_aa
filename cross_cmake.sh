#!/bin/bash
set -e
cmake . -DCROSS_COMPILE=$CROSS_COMPILE -DBUILD_FOR_MM=ON -B build
