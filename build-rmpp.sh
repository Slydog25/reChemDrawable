#!/usr/bin/env sh
set -eu

# The Paper Pro SDK environment script sets CXX to its aarch64 cross-compiler.
# Check what that compiler actually targets instead of the host architecture,
# so an Apple Silicon or other arm64 host cannot slip a native build through.
# shellcheck disable=SC2086 # CXX may carry SDK flags such as --sysroot.
target=$(${CXX:-c++} -dumpmachine 2>/dev/null || true)
case "$target" in
    aarch64-*linux*) ;;
    *)
        echo "The C++ compiler targets '${target:-unknown}', not aarch64 Linux." >&2
        echo "Load the Paper Pro SDK environment before running this script." >&2
        exit 1
        ;;
esac

build_dir="build-rmpp"
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DRECHEMDRAW_EXTERNAL_QML=ON
cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-1}"
