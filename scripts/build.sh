#!/usr/bin/env bash
# Builds the Lineage VST3 plugin and (by default) installs it into your
# local VST3 folder so it shows up in Bitwig/Ableton without any manual
# copying. Run from anywhere; paths are relative to this script.
#
# Usage:
#   ./scripts/build.sh              # build + install
#   ./scripts/build.sh --no-install # build only
#   ./scripts/build.sh --clean      # wipe the build dir first (use after a
#                                    # JUCE/quickjs version bump, or if the
#                                    # build is in a weird state)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
PLUGIN_DIR="$REPO_ROOT/plugin"
BUILD_DIR="$PLUGIN_DIR/build"

INSTALL=1
CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --no-install) INSTALL=0 ;;
    --clean) CLEAN=1 ;;
    *) echo "Unknown argument: $arg" >&2; exit 1 ;;
  esac
done

if [ "$CLEAN" -eq 1 ]; then
  echo "Removing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

echo "==> npm install (engine deps, needed for the esbuild runtime bundle)"
(cd "$REPO_ROOT" && npm install)

echo "==> Configuring (this fetches JUCE + quickjs-ng on first run, which takes a while)"
cmake -S "$PLUGIN_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

NPROC=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
echo "==> Building (using $NPROC parallel jobs)"
cmake --build "$BUILD_DIR" --config Release --target Lineage_VST3 -j "$NPROC"

VST3_PATH="$BUILD_DIR/Lineage_artefacts/Release/VST3/Lineage.vst3"
if [ ! -d "$VST3_PATH" ]; then
  echo "Build finished but $VST3_PATH wasn't found — something's wrong." >&2
  exit 1
fi
echo "==> Built: $VST3_PATH"

if [ "$INSTALL" -eq 1 ]; then
  case "$(uname -s)" in
    Darwin) DEST="$HOME/Library/Audio/Plug-Ins/VST3" ;;
    Linux) DEST="$HOME/.vst3" ;;
    *) echo "Unrecognized OS $(uname -s) — skipping install, copy $VST3_PATH into your VST3 folder manually." >&2; DEST="" ;;
  esac
  if [ -n "$DEST" ]; then
    mkdir -p "$DEST"
    rm -rf "$DEST/Lineage.vst3"
    cp -R "$VST3_PATH" "$DEST/"
    echo "==> Installed to $DEST/Lineage.vst3"
    echo "    Rescan plugins in your DAW (or restart it) to pick it up."
  fi
else
  echo "==> Skipped install (--no-install). Copy $VST3_PATH into your VST3 folder manually."
fi
