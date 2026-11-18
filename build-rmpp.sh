#!/usr/bin/env sh
set -eu

if [ "$(uname -m)" = "x86_64" ] && ! command -v aarch64-remarkable-linux-g++ >/dev/null 2>&1 && ! command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
    echo "Load the Paper Pro SDK environment before running this script." >&2
    exit 1
fi

build_dir="build-rmpp"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DRECHEMDRAW_EXTERNAL_QML=ON
cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-1}"
