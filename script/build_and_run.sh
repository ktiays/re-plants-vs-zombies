#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-run}"
APP_NAME="PlantsVsZombies"
BUNDLE_ID="dev.ktiays.plants-vs-zombies"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/macos"
APP_BUNDLE="$BUILD_DIR/platform/macos/PlantsVsZombies.app"
APP_BINARY="$APP_BUNDLE/Contents/MacOS/$APP_NAME"
APP_ARGS=()
if [[ "${PVZ_RENDERER_SMOKE:-0}" == "1" ]]; then
  APP_ARGS+=("--renderer-smoke")
fi
if [[ -n "${PVZ_PAK_PATH:-}" ]]; then
  APP_ARGS+=("$PVZ_PAK_PATH")
fi

pkill -x "$APP_NAME" >/dev/null 2>&1 || true

cmake \
  -S "$ROOT_DIR" \
  -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPVZ_BUILD_LEGACY_WINDOWS=OFF \
  -DPVZ_BUILD_MACOS_APP=ON \
  -DBUILD_TESTING=ON
cmake --build "$BUILD_DIR" --target pvz_app_macos --parallel

open_app() {
  if [[ "${#APP_ARGS[@]}" -eq 0 ]]; then
    /usr/bin/open -n "$APP_BUNDLE"
  else
    /usr/bin/open -n "$APP_BUNDLE" --args "${APP_ARGS[@]}"
  fi
}

case "$MODE" in
  run)
    open_app
    ;;
  --debug|debug)
    lldb -- "$APP_BINARY" "${APP_ARGS[@]}"
    ;;
  --logs|logs)
    open_app
    /usr/bin/log stream \
      --info \
      --style compact \
      --predicate "process == \"$APP_NAME\""
    ;;
  --telemetry|telemetry)
    open_app
    /usr/bin/log stream \
      --info \
      --style compact \
      --predicate "subsystem == \"$BUNDLE_ID\""
    ;;
  --verify|verify)
    open_app
    sleep 1
    pgrep -x "$APP_NAME" >/dev/null
    ;;
  *)
    echo \
      "usage: $0 [run|--debug|--logs|--telemetry|--verify]" \
      >&2
    exit 2
    ;;
esac
