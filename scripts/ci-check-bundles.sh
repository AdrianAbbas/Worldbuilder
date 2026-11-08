#!/usr/bin/env bash
# CI gate for the built macOS bundles: both architecture slices present, the samples
# in the plugin bundles, AU identity and sandboxSafe in Info.plist, minimum macOS version
# reported.
# Usage: scripts/ci-check-bundles.sh <artefacts dir, e.g. build/WORLDBUILDER_artefacts/Release>
set -euo pipefail
artefacts="$1"
status=0
for bundle in "$artefacts/AU/WORLDBUILDER.component" "$artefacts/VST3/WORLDBUILDER.vst3" "$artefacts/Standalone/WORLDBUILDER.app"; do
    binary="$bundle/Contents/MacOS/WORLDBUILDER"
    if [[ ! -f "$binary" ]]; then
        echo "::error::missing $binary"; status=1; continue
    fi
    archs=$(/usr/bin/lipo -archs "$binary")
    printf '%-60s %s\n' "$binary" "$archs"
    if [[ " $archs " != *" arm64 "* || " $archs " != *" x86_64 "* ]]; then
        echo "::error::$binary must contain arm64 and x86_64 slices, has: $archs"; status=1
    fi
    for arch in x86_64 arm64; do
        minos=$(/usr/bin/otool -arch "$arch" -l "$binary" | /usr/bin/awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; exit}')
        printf '    %-7s minimum macOS %s\n' "$arch" "${minos:-unknown}"
    done
done

# The developer's samples (Milestone 15) inside each plugin bundle.
for bundle in "$artefacts/AU/WORLDBUILDER.component" "$artefacts/VST3/WORLDBUILDER.vst3"; do
    samples="$bundle/Contents/Resources/Samples"
    if [[ -f "$samples/index.json" ]]; then
        printf '%-60s %s notes, %s (bundle %s)\n' "$samples" "$(find "$samples" -name '*.flac' | wc -l | tr -d ' ')" "$(du -sh "$samples" | cut -f1)" "$(du -sh "$bundle" | cut -f1)"
    else
        echo "::error::$bundle has no samples ($samples/index.json missing)"; status=1
    fi
done

plist="$artefacts/AU/WORLDBUILDER.component/Contents/Info.plist"
check() {
    local key="$1" expected="$2" actual
    actual=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$key" "$plist" 2>/dev/null || echo "(absent)")
    if [[ "$actual" == "$expected" ]]; then
        printf 'Info.plist %-16s %s\n' "$key" "$actual"
    else
        echo "::error::Info.plist $key expected '$expected', got '$actual'"; status=1
    fi
}
check type aumu
check subtype Wbld
check manufacturer Veil
check factoryFunction WORLDBUILDERAUFactory
check sandboxSafe true
# The company rename: hosts show "Windward Audio: WORLDBUILDER" (AU) and the vendor
# Windward Audio (VST3); the bundle IDs are new; the codes above stay.
check name "Windward Audio: WORLDBUILDER"
for bundle in "$artefacts/AU/WORLDBUILDER.component" "$artefacts/VST3/WORLDBUILDER.vst3" "$artefacts/Standalone/WORLDBUILDER.app"; do
    id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || echo "(absent)")
    if [[ "$id" == "com.windwardaudio.worldbuilder" ]]; then
        printf '%-60s CFBundleIdentifier %s\n' "$bundle" "$id"
    else
        echo "::error::$bundle CFBundleIdentifier expected 'com.windwardaudio.worldbuilder', got '$id'"; status=1
    fi
    if /usr/bin/grep -rqi 'veil audio\|elsewhere audio' "$bundle/Contents/Info.plist"; then
        echo "::error::$bundle/Contents/Info.plist still names the old company"; status=1
    fi
done
moduleinfo="$artefacts/VST3/WORLDBUILDER.vst3/Contents/Resources/moduleinfo.json"
if [[ -f "$moduleinfo" ]]; then
    vendors=$(/usr/bin/grep -E '"Vendor"' "$moduleinfo" || true)
    if [[ -n "$vendors" ]] && ! printf '%s\n' "$vendors" | /usr/bin/grep -vq 'Windward Audio' && ! /usr/bin/grep -qi 'veil audio' "$moduleinfo"; then
        echo "VST3 moduleinfo.json vendor: Windward Audio"
    else
        echo "::error::VST3 moduleinfo.json does not name the vendor Windward Audio"; /usr/bin/grep -i vendor "$moduleinfo" || true; status=1
    fi
else
    echo "VST3 moduleinfo.json: not generated (the vendor comes from the plug-in's factory, checked by the tests)"
fi
exit $status
