#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
build_root="$repo_root/build/native/source"
install_dir="$build_root/install"

build_component() {
    local source_dir=$1
    local component=${source_dir##*/}
    shift

    cmake -S "$repo_root/$source_dir" -B "$build_root/$component" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$install_dir" \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_PREFIX_PATH="$install_dir" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        "$@"
    cmake --build "$build_root/$component" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-1}"
    cmake --install "$build_root/$component"
}

# Build the libraries in dependency order so each project can find the CMake
# packages installed by the projects it depends on.
for component in tklog tkassert tkrng tktokenswap tkwsm; do
    build_component "libs/$component" -DBUILD_SHARED_LIBS=OFF
done
build_component tket -DBUILD_SHARED_LIBS=OFF

# Keep the C API shared for callers such as QBlaze. Point it explicitly at the
# core we just installed so an older package elsewhere can't take its place.
build_component tket-c-api -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON \
    -Dtket_DIR="$install_dir/lib/cmake/tket"
ctest --test-dir "$build_root/tket-c-api" --output-on-failure
