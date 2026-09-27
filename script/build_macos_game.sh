#!/usr/bin/env bash
# Builds the cross-platform game (SexyAppFramework on SDL3 + Metal) for macOS
# and optionally runs it.
#
#   script/build_macos_game.sh [build|run|debug|bundle] [game arguments...]
#
#   build   configure and build only
#   run     build, then run with the game data directory (default)
#   debug   build, then run under lldb
#   bundle  build, then copy the game data into the .app so it runs standalone
#
# Environment:
#   PVZ_GAME_DIR   directory holding main.pak and properties/ (default: gamedata/)
#   PVZ_CONFIG     CMake build type (default: Release)
set -euo pipefail

MODE="${1:-run}"
if [[ $# -gt 0 ]]; then
  shift
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG="${PVZ_CONFIG:-Release}"
BUILD_DIR="$ROOT_DIR/build/game-$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')"
APP_BUNDLE="$BUILD_DIR/SexyAppFramework/PlantsVsZombies.app"
APP_BINARY="$APP_BUNDLE/Contents/MacOS/PlantsVsZombies"
GAME_DIR="${PVZ_GAME_DIR:-$ROOT_DIR/gamedata}"

cmake \
  -S "$ROOT_DIR" \
  -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DPVZ_BUILD_GAME=ON \
  -DPVZ_BUILD_PORTABLE_ENGINE=OFF \
  -DPVZ_BUILD_MODULE_MUSIC=OFF
cmake --build "$BUILD_DIR" --target PlantsVsZombies

require_game_data() {
  if [[ ! -f "$GAME_DIR/main.pak" ]]; then
    echo "Game data not found: $GAME_DIR/main.pak" >&2
    echo "Set PVZ_GAME_DIR to the folder containing main.pak and properties/." >&2
    exit 1
  fi
}

case "$MODE" in
  build)
    ;;
  run)
    require_game_data
    PVZ_GAME_DIR="$GAME_DIR" exec "$APP_BINARY" "$@"
    ;;
  debug)
    require_game_data
    PVZ_GAME_DIR="$GAME_DIR" exec lldb -- "$APP_BINARY" "$@"
    ;;
  bundle)
    require_game_data
    RESOURCES_DIR="$APP_BUNDLE/Contents/Resources"
    mkdir -p "$RESOURCES_DIR"
    cp "$GAME_DIR/main.pak" "$RESOURCES_DIR/"
    rsync -a "$GAME_DIR/properties" "$RESOURCES_DIR/"
    codesign --force --sign - "$APP_BUNDLE"
    echo "Bundled game data into $APP_BUNDLE"
    ;;
  *)
    echo "usage: $0 [build|run|debug|bundle] [game arguments...]" >&2
    exit 2
    ;;
esac
