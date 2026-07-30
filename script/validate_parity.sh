#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PARITY_BUILD_DIR="${PVZ_PARITY_BUILD_DIR:-"$ROOT_DIR/out/parity"}"

cmake \
  -S "$ROOT_DIR" \
  -B "$PARITY_BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPVZ_BUILD_PORTABLE_ENGINE=ON \
  -DPVZ_BUILD_LEGACY_WINDOWS=OFF \
  -DPVZ_BUILD_MACOS_APP=OFF \
  -DBUILD_TESTING=ON
cmake \
  --build "$PARITY_BUILD_DIR" \
  --target pvz_game_legacy_parity_tests \
  --parallel
ctest \
  --test-dir "$PARITY_BUILD_DIR" \
  --tests-regex '^pvz_game_legacy_parity_tests$' \
  --output-on-failure
