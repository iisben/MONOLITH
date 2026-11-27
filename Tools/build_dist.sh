#!/bin/zsh
# Build and package MONOLITH release zips into build/dist/ from a Mac.
#
#   Tools/build_dist.sh mac        universal (arm64 + x86_64), macOS 11+, AU / VST3 / Standalone, ad-hoc signed
#   Tools/build_dist.sh windows    x64 VST3 / Standalone via clang-cl + xwin (XWIN_DIR=<xwin splat dir>)
#   Tools/build_dist.sh linux      x86_64 VST3 / Standalone in an Ubuntu 22.04 container (Tools/build_linux_docker.sh)
#   Tools/build_dist.sh all
#
# JUCE: reuses the copy CMake fetched into build/release (configure a normal macOS build once first),
# or set MONOLITH_JUCE_DIR.
set -eu
REPO="${0:A:h:h}"
cd "$REPO"
VER=$(sed -n 's/^project(MONOLITH VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
JUCE="${MONOLITH_JUCE_DIR:-$REPO/build/release/_deps/juce-src}"
DIST="$REPO/build/dist"
mkdir -p "$DIST"

build_mac() {
  local B=build/dist-mac
  cmake -G Ninja -S . -B $B -DCMAKE_BUILD_TYPE=Release -DMONOLITH_JUCE_DIR="$JUCE" \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 >/dev/null
  cmake --build $B --target MONOLITH_All
  local A=$B/MONOLITH_artefacts/Release P="$DIST/MONOLITH-$VER-macOS"
  rm -rf "$P" && mkdir -p "$P"
  for b in "$A/AU/MONOLITH.component" "$A/VST3/MONOLITH.vst3" "$A/Standalone/MONOLITH.app"; do
    codesign --force --deep --sign - --timestamp=none "$b"
    codesign --verify --deep --strict "$b"
    ditto "$b" "$P/$(basename "$b")"
  done
  lipo -archs "$P/MONOLITH.component/Contents/MacOS/MONOLITH"
  rm -f "$P.zip" && ditto -c -k --keepParent "$P" "$P.zip"
  echo "mac -> $P.zip"
}

build_windows() {
  [[ -n "${XWIN_DIR:-}" ]] || { echo "set XWIN_DIR (xwin splat output)"; exit 1; }
  # the toolchain passes XWIN_DIR inside plain flag strings, so give it a path without spaces
  local X="${TMPDIR:-/tmp}/monolith-xwin"
  ln -sfn "$XWIN_DIR" "$X"
  local B=build/dist-windows
  cmake -G Ninja -S . -B $B -DCMAKE_BUILD_TYPE=Release -DMONOLITH_JUCE_DIR="$JUCE" \
        -DCMAKE_TOOLCHAIN_FILE="$REPO/cmake/toolchains/windows-x64-clang-cl.cmake" -DXWIN_DIR="$X" >/dev/null
  cmake --build $B --target MONOLITH_VST3 MONOLITH_Standalone
  local A=$B/MONOLITH_artefacts/Release P="$DIST/MONOLITH-$VER-Windows-x64"
  rm -rf "$P" && mkdir -p "$P"
  cp -R "$A/VST3/MONOLITH.vst3" "$P/"
  cp "$A/Standalone/MONOLITH.exe" "$P/"
  rm -f "$P.zip" && (cd "$DIST" && zip -qr "$(basename "$P").zip" "$(basename "$P")")
  echo "windows -> $P.zip"
}

build_linux() {
  "$REPO/Tools/build_linux_docker.sh" "$JUCE"
  local P="$DIST/MONOLITH-$VER-Linux-x86_64"
  rm -rf "$P" && mkdir -p "$P"
  cp -R "$DIST/linux/MONOLITH.vst3" "$DIST/linux/MONOLITH" "$P/"
  rm -f "$P.tar.gz" && tar -C "$DIST" -czf "$P.tar.gz" "$(basename "$P")"
  echo "linux -> $P.tar.gz"
}

case "${1:-all}" in
  mac) build_mac ;;
  windows) build_windows ;;
  linux) build_linux ;;
  all) build_mac; build_windows; build_linux ;;
  *) echo "usage: $0 mac|windows|linux|all"; exit 1 ;;
esac
