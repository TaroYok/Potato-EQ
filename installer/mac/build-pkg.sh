#!/bin/bash
# Builds "PotatoEQ-<version>-macOS.pkg" from the plugin bundles in dist/.
# Usage: installer/mac/build-pkg.sh <version>
set -euo pipefail
VER="$1"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
DIST="$ROOT/dist"
WORK="$ROOT/build-pkg"
OUT="$ROOT/installers"
rm -rf "$WORK"; mkdir -p "$WORK/pkgs" "$OUT"

component() {   # id  bundle  install-location  [scripts-dir]
    local id="$1" bundle="$2" dest="$3" scripts="${4:-}"
    local stage="$WORK/stage-$id"
    mkdir -p "$stage"
    cp -R "$DIST/$bundle" "$stage/"
    # don't let the installer "relocate" the bundle to some other copy it finds on disk
    pkgbuild --analyze --root "$stage" "$WORK/$id.plist"
    plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$id.plist"
    local args=(--root "$stage" --component-plist "$WORK/$id.plist" --identifier "com.taroyaudio.potatoeq.$id"
                --version "$VER" --install-location "$dest")
    if [ -n "$scripts" ]; then args+=(--scripts "$scripts"); fi
    pkgbuild "${args[@]}" "$WORK/pkgs/$id.pkg"
}

component vst3 "Potato EQ.vst3"      "/Library/Audio/Plug-Ins/VST3"
component au   "Potato EQ.component" "/Library/Audio/Plug-Ins/Components" "$HERE/scripts-au"
component app  "Potato EQ.app"       "/Applications"

sed "s/__VERSION__/$VER/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" --resources "$HERE/resources" \
             "$OUT/PotatoEQ-$VER-macOS.pkg"
echo "Built $OUT/PotatoEQ-$VER-macOS.pkg"
