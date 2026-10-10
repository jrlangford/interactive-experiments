#!/bin/bash
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
mkdir -p "$BUILD_DIR"
xcrun clang++ -std=c++17 -O2 -g -Wall -Wextra -Wpedantic -Werror \
    -mmacosx-version-min=14.0 "$PROJECT_DIR/tools/benchmark_parallel.cpp" \
    "$PROJECT_DIR/src/fluid_sim.cpp" -o "$BUILD_DIR/benchmark-parallel-physics"
exec "$BUILD_DIR/benchmark-parallel-physics" "$@"
