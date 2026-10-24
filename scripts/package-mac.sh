#!/usr/bin/env bash
# Sign the built bundles ad hoc (whole bundle, --deep), verify them, and stage
# dist/WORLDBUILDER-macOS-Universal.zip with the AU, the VST3 and the install scripts.
# Usage: scripts/package-mac.sh <artefacts dir> <dist dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
artefacts="$1"
dist="$2"
stage="$dist/WORLDBUILDER-macOS-Universal"
rm -rf "$stage" "$dist/WORLDBUILDER-macOS-Universal.zip"
mkdir -p "$stage"

for bundle in "$artefacts/AU/WORLDBUILDER.component" "$artefacts/VST3/WORLDBUILDER.vst3" "$artefacts/Standalone/WORLDBUILDER.app"; do
    # Developer ID signing (optional CI path) runs before this script; keep it.
    if ! /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application"; then
        /usr/bin/codesign --force --deep --sign - --timestamp=none "$bundle"
    fi
    /usr/bin/codesign --verify --strict --verbose=4 "$bundle"
    /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -E "^(Identifier|Format|Signature|Authority|Sealed Resources)"
done

/usr/bin/ditto "$artefacts/AU/WORLDBUILDER.component" "$stage/WORLDBUILDER.component"
/usr/bin/ditto "$artefacts/VST3/WORLDBUILDER.vst3" "$stage/WORLDBUILDER.vst3"
install -m 755 "$here/scripts/install-mac.sh" "$here/scripts/uninstall-mac.sh" "$stage/"
install -m 644 "$here/packaging/INSTALL.txt" "$stage/"
# ditto keeps permissions and bundle metadata, unlike a plain zip of the artifact.
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$stage" "$dist/WORLDBUILDER-macOS-Universal.zip"
echo "Packaged $dist/WORLDBUILDER-macOS-Universal.zip"
/usr/bin/unzip -l "$dist/WORLDBUILDER-macOS-Universal.zip" | /usr/bin/grep -E "install|INSTALL|MacOS/WORLDBUILDER$"
