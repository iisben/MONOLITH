#!/bin/zsh
# Build the Linux x86_64 VST3 + Standalone of MONOLITH in an Ubuntu 22.04 container, run the headless QA suites
# there, and copy the plugin bundles to build/dist/linux. Needs Docker (e.g. `colima start --vz-rosetta`).
#
#   Tools/build_linux_docker.sh [path/to/JUCE-8.0.15]    (default: the JUCE copy CMake fetched into build/release)
set -eu
REPO="${0:A:h:h}"
JUCE="${1:-$REPO/build/release/_deps/juce-src}"
OUT="$REPO/build/dist/linux"
IMG=monolith-linux-build:22.04
[[ -f "$JUCE/CMakeLists.txt" ]] || { echo "JUCE not found at $JUCE (configure a macOS build first or pass a path)"; exit 1; }

docker build --platform linux/amd64 -t $IMG "$REPO/Tools/linux"
cid=$(docker run -d --platform linux/amd64 $IMG sleep infinity)
trap 'docker rm -f $cid >/dev/null' EXIT
tar -C "$REPO" --exclude ./build --exclude ./.git --exclude '*.zip' -cf - . | docker exec -i $cid tar -C /src -xf -
tar -C "$JUCE" --exclude ./.git -cf - . | docker exec -i $cid sh -c 'mkdir -p /juce && tar -C /juce -xf -'

docker exec $cid cmake -G Ninja -S /src -B /build -DCMAKE_BUILD_TYPE=Release -DMONOLITH_JUCE_DIR=/juce
docker exec $cid cmake --build /build

# Headless QA (Xvfb for the GUI-initialising tools). The CPU gate depends on the host and is reported, not trusted.
docker exec $cid sh -c 'cd /build && mkdir -p /src/build/v2_qa \
  && ./MonolithDspAudit_artefacts/Release/MonolithDspAudit | tail -1 \
  && ./MonolithQA_artefacts/Release/MonolithQA --phase v2.4 | grep -E "^TOTAL|FAIL" \
  ; ./MonolithPresetQA_artefacts/Release/MonolithPresetQA --no-audition | grep "^TOTAL" \
  ; ./MonolithPresetQA_artefacts/Release/MonolithPresetQA --dc-probe | tail -1 \
  ; xvfb-run -a ./MonolithUISnapshot_artefacts/Release/MonolithUISnapshot /src/build/v2_qa/ui_snapshots_linux | tail -1'

rm -rf "$OUT"; mkdir -p "$OUT"
docker cp $cid:/build/MONOLITH_artefacts/Release/VST3/MONOLITH.vst3 "$OUT/"
docker cp $cid:/build/MONOLITH_artefacts/Release/Standalone/MONOLITH "$OUT/"
docker cp $cid:/src/build/v2_qa/ui_snapshots_linux "$REPO/build/v2_qa/" 2>/dev/null || true
docker exec $cid sh -c 'file /build/MONOLITH_artefacts/Release/Standalone/MONOLITH /build/MONOLITH_artefacts/Release/VST3/MONOLITH.vst3/Contents/x86_64-linux/MONOLITH.so'
echo "Linux build -> $OUT"
