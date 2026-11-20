#!/usr/bin/env bash
# Build MONOLITH (AU + VST3 + Standalone) in Release and install it for the current user.
# Usage (from the repo root):   ./scripts/install_mac.sh
set -euo pipefail

if [[ "$(uname)" != "Darwin" ]]; then
    echo "This installer is for macOS." >&2
    exit 1
fi

cd "$(dirname "$0")/.."

GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)

cmake -B build-mac "${GEN[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

cmake --build build-mac --config Release --parallel \
    --target MONOLITH_AU MONOLITH_VST3 MONOLITH_Standalone MonolithDspAudit

ART=build-mac/MONOLITH_artefacts/Release
[[ -d "$ART" ]] || ART=build-mac/MONOLITH_artefacts

echo "== Running DSP audit (gain staging, clipping, squeal, runaway, clicks) =="
AUDIT=$(find build-mac -type f -name MonolithDspAudit -perm -u+x | head -n 1)
"$AUDIT"

AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$AU_DIR" "$VST3_DIR"

rm -rf "$AU_DIR/MONOLITH.component" "$VST3_DIR/MONOLITH.vst3"
cp -R "$ART/AU/MONOLITH.component"  "$AU_DIR/"
cp -R "$ART/VST3/MONOLITH.vst3"     "$VST3_DIR/"
rm -rf "/Applications/MONOLITH.app" 2>/dev/null || true
cp -R "$ART/Standalone/MONOLITH.app" "/Applications/" 2>/dev/null \
    || cp -R "$ART/Standalone/MONOLITH.app" "$HOME/Applications/"

# Ad-hoc sign + clear quarantine so Logic / Ableton load it without Gatekeeper prompts
for b in "$AU_DIR/MONOLITH.component" "$VST3_DIR/MONOLITH.vst3"; do
    codesign --force --deep --sign - "$b" >/dev/null 2>&1 || true
    xattr -dr com.apple.quarantine "$b" 2>/dev/null || true
done

# Force macOS to rescan Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "Installed:"
echo "  AU   -> $AU_DIR/MONOLITH.component"
echo "  VST3 -> $VST3_DIR/MONOLITH.vst3"
echo "  App  -> /Applications/MONOLITH.app"
echo "Validate AU (optional):  auval -v aumu Mnch Mnlt"
