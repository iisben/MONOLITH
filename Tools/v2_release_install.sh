#!/bin/zsh
# MONOLITH II (V2 Stage 6): ad-hoc codesign the Release AU / VST3 / Standalone bundles and install them for the
# current user. Previously installed bundles are moved once into build/install_backup_previous/ (never deleted).
#
#   [ART=<artefacts dir>] Tools/v2_release_install.sh [identity]     identity defaults to "-" (ad-hoc); pass a Developer ID to sign for release
#
# Close any DAW that has MONOLITH loaded before running. Afterwards hosts pick the new build up on their next rescan.
set -u
REPO="${0:A:h:h}"
ART="${ART:-$REPO/build/release/MONOLITH_artefacts/Release}"   # override: ART=build/dist-mac/MONOLITH_artefacts/Release
BK="$REPO/build/install_backup_previous"
COMP="$HOME/Library/Audio/Plug-Ins/Components"
VST="$HOME/Library/Audio/Plug-Ins/VST3"
APPS="$HOME/Applications"
ID="${1:--}"
fail=0

for b in "$ART/AU/MONOLITH.component" "$ART/VST3/MONOLITH.vst3" "$ART/Standalone/MONOLITH.app"; do
  [[ -d "$b" ]] || { echo "MISSING $b (build Release first)"; exit 1; }
  codesign --force --deep --sign "$ID" --timestamp=none "$b" >/dev/null 2>&1 || { echo "SIGN FAIL $b"; fail=1; }
  codesign --verify --deep --strict "$b" && echo "signed   $(basename "$b")" || { echo "VERIFY FAIL $b"; fail=1; }
done

mkdir -p "$BK" "$COMP" "$VST" "$APPS"
for old in "$COMP/MONOLITH.component" "$VST/MONOLITH.vst3" "$APPS/MONOLITH.app"; do
  if [[ -e "$old" ]]; then
    dest="$BK/$(basename "$old")"
    if [[ -e "$dest" ]]; then rm -rf "$old"; else mv "$old" "$dest" && echo "backup   $(basename "$old") -> $dest"; fi
  fi
done

ditto "$ART/AU/MONOLITH.component" "$COMP/MONOLITH.component" || fail=1
ditto "$ART/VST3/MONOLITH.vst3"    "$VST/MONOLITH.vst3"       || fail=1
ditto "$ART/Standalone/MONOLITH.app" "$APPS/MONOLITH.app"     || fail=1
for b in "$COMP/MONOLITH.component" "$VST/MONOLITH.vst3" "$APPS/MONOLITH.app"; do
  codesign --verify --deep --strict "$b" && echo "installed $b" || { echo "INSTALLED VERIFY FAIL $b"; fail=1; }
done

# Refresh AudioComponent registration. Query instruments only: a full `auval -a` listing can be cut short by
# third-party plugins that print to stdout while being scanned.
killall -9 AudioComponentRegistrar 2>/dev/null
reg=""
for i in {1..10}; do
  reg=$(auval -s aumu 2>/dev/null | grep "Mnch Mnlt")
  [[ -n "$reg" ]] && break
  sleep 2
done
[[ -n "$reg" ]] && echo "registered: $reg" || { echo "AU NOT REGISTERED"; fail=1; }
exit $fail
