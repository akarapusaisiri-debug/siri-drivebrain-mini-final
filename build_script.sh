#!/usr/bin/env sh
set -e

# Cross compiles for the pi. Run it in the docker container -> build/mini-drivebrain
conan profile detect --force
conan install . --build=missing --profile:host=rpi_profile -of=build

cd build
. ./conanbuild.sh
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
make -j"$(nproc)"
