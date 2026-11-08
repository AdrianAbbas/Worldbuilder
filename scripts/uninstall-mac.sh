#!/usr/bin/env bash
# Remove WORLDBUILDER for the current user.
#   bash uninstall-mac.sh           remove the plug-ins, keep your saved worlds and presets
#   bash uninstall-mac.sh --purge   also remove ~/Library/Windward Audio/WORLDBUILDER and
#                                   the host's presets (~/Library/Audio/Presets/Windward Audio/WORLDBUILDER,
#                                   and those saved under the earlier company name)
set -euo pipefail
purge=0
[[ "${1:-}" == --purge ]] && purge=1

components="$HOME/Library/Audio/Plug-Ins/Components"
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
# The bundle IDs: now, and before the company rename (a copy of either is removed).
ids="com.windwardaudio.worldbuilder audio.veil.worldbuilder"

removed=0
for bundle in "$components/WORLDBUILDER.component" "$vst3/WORLDBUILDER.vst3"; do
    if [[ -e "$bundle" ]]; then
        rm -rf "$bundle"
        echo "Removed $bundle"
        removed=1
    fi
done
for bundle in "$components"/*.component "$vst3"/*.vst3; do
    [[ -d "$bundle" ]] || continue
    id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)
    for known in $ids; do
        if [[ "$id" == "$known" ]]; then
            rm -rf "$bundle"
            echo "Removed $bundle"
            removed=1
        fi
    done
done
[[ $removed == 1 ]] || echo "No WORLDBUILDER plug-ins found in ~/Library/Audio/Plug-Ins."

for folder in "$HOME/Library/Windward Audio/WORLDBUILDER" "$HOME/Library/Audio/Presets/Windward Audio/WORLDBUILDER"; do
    if [[ $purge == 1 && -d "$folder" ]]; then
        rm -rf "$folder"
        echo "Removed $folder"
    elif [[ -d "$folder" ]]; then
        echo "Kept your worlds and presets in $folder (use --purge to remove them)."
    fi
done
# The host's presets saved under the earlier company name (copied to the folders above on
# the first launch after the rename; the originals were kept).
old_presets="$HOME/Library/Audio/Presets/Veil Audio/WORLDBUILDER"
if [[ $purge == 1 && -d "$old_presets" ]]; then
    rm -rf "$old_presets"
    echo "Removed the host presets saved under the earlier company name."
fi

for system_copy in "/Library/Audio/Plug-Ins/Components/WORLDBUILDER.component" "/Library/Audio/Plug-Ins/VST3/WORLDBUILDER.vst3"; do
    [[ -e "$system_copy" ]] && echo "A system-wide copy remains: sudo rm -rf \"$system_copy\""
done

/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Audio Unit registry reset. Restart GarageBand or Logic."
