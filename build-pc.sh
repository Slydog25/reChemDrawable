#!/usr/bin/env sh
set -eu

build_dir="build-pc"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

