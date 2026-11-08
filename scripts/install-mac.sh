#!/usr/bin/env bash
# Install WORLDBUILDER for the current user and make sure macOS will load it.
#
# Run it from the unzipped WindwardAudio-WORLDBUILDER-macOS-Universal folder:
#     bash install-mac.sh
# (Tip: type "bash " in Terminal, then drag install-mac.sh into the window.)
#
# What it does:
#   0. removes copies installed under the earlier company name (found by their
#      earlier bundle ID), so hosts do not list WORLDBUILDER twice
#   1. copies WORLDBUILDER.component to ~/Library/Audio/Plug-Ins/Components and
#      WORLDBUILDER.vst3 to ~/Library/Audio/Plug-Ins/VST3
#   2. removes the com.apple.quarantine flag your browser added. Without this,
#      macOS refuses to load the plug-in and GarageBand shows a silent slot
#   3. re-signs both bundles (ad hoc) and verifies the signatures
#   4. restarts the Audio Unit registrar and clears the hosts' Audio Unit cache so
#      they see the new copy under its new company name, once
#   5. runs Apple's Audio Unit validator (auval) and prints PASS or FAIL
#   6. prints the next steps for GarageBand and Logic
#
# Options:
#   --ci              non-interactive: no prompts, no check for running hosts
#   --out-of-process  remove the sandboxSafe flag from the installed copy, so
#                     GarageBand hosts WORLDBUILDER in a separate process (fallback only)
# (No `set -u`: macOS bash 3.2 treats an empty "$@" as unbound.)
set -eo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ci=0
out_of_process=0
for arg in "$@"; do
    case "$arg" in
        --ci) ci=1 ;;
        --out-of-process) out_of_process=1 ;;
        -h|--help) /usr/bin/sed -n '2,26p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 64 ;;
    esac
done

bold() { printf '\033[1m%s\033[0m\n' "$*"; }
ok()   { printf '  \033[32mOK\033[0m   %s\n' "$*"; }
warn() { printf '  \033[33mNOTE\033[0m %s\n' "$*"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$*"; }

if [[ "$(uname -s)" != Darwin ]]; then
    echo "install-mac.sh runs on macOS only." >&2
    exit 1
fi

component_src="$here/WORLDBUILDER.component"
vst3_src="$here/WORLDBUILDER.vst3"
if [[ ! -d "$component_src" ]]; then
    echo "WORLDBUILDER.component was not found next to this script ($here)." >&2
    echo "Unzip WindwardAudio-WORLDBUILDER-macOS-Universal.zip first and run the script from that folder." >&2
    exit 1
fi

components="$HOME/Library/Audio/Plug-Ins/Components"
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
logs="$HOME/Library/Logs/WORLDBUILDER"
mkdir -p "$components" "$vst3" "$logs"

bold "WORLDBUILDER installer"

# Hosts keep their own view of plug-ins while running.
if [[ $ci == 0 ]]; then
    running=$(/usr/bin/pgrep -l -x GarageBand 2>/dev/null; /usr/bin/pgrep -l -f 'Logic Pro' 2>/dev/null || true)
    if [[ -n "$running" ]]; then
        warn "GarageBand or Logic is running. Quit it (Cmd-Q), then press Return to continue."
        read -r _
    fi
fi

# A second, system-wide copy would compete with this one.
for system_copy in "/Library/Audio/Plug-Ins/Components/WORLDBUILDER.component" "/Library/Audio/Plug-Ins/VST3/WORLDBUILDER.vst3"; do
    if [[ -e "$system_copy" ]]; then
        warn "An older system-wide copy exists: $system_copy"
        warn "Remove it so hosts load the new one:  sudo rm -rf \"$system_copy\""
    fi
done

# The bundle IDs WORLDBUILDER had before the company rename. A copy with one of these
# (under any file name) is removed; its Audio Unit codes are the same as the new one's,
# so leaving it would list WORLDBUILDER twice.
old_ids="audio.veil.worldbuilder"
remove_old_copies() {
    local dir bundle id
    for dir in "$components" "$vst3"; do
        for bundle in "$dir"/*.component "$dir"/*.vst3; do
            [[ -d "$bundle" ]] || continue
            id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)
            for old in $old_ids; do
                if [[ "$id" == "$old" ]]; then
                    rm -rf "$bundle"
                    ok "removed the copy under the earlier company name: $bundle"
                fi
            done
        done
    done
    for dir in /Library/Audio/Plug-Ins/Components /Library/Audio/Plug-Ins/VST3; do
        for bundle in "$dir"/*.component "$dir"/*.vst3; do
            [[ -d "$bundle" ]] || continue
            id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)
            for old in $old_ids; do
                if [[ "$id" == "$old" ]]; then
                    warn "A system-wide copy under the earlier company name exists: $bundle"
                    warn "Remove it so hosts list WORLDBUILDER once:  sudo rm -rf \"$bundle\""
                fi
            done
        done
    done
}

install_bundle() {
    local src="$1" dest="$2" binary
    rm -rf "$dest"
    /usr/bin/ditto "$src" "$dest"
    binary="$dest/Contents/MacOS/WORLDBUILDER"
    chmod +x "$binary"
    /usr/bin/xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
    if /usr/bin/xattr -lr "$dest" 2>/dev/null | /usr/bin/grep -q com.apple.quarantine; then
        fail "could not remove the quarantine flag from $dest"
        return 1
    fi
    if [[ "$dest" == *.component && $out_of_process == 1 ]]; then
        /usr/libexec/PlistBuddy -c "Delete :AudioComponents:0:sandboxSafe" "$dest/Contents/Info.plist" 2>/dev/null || true
        warn "sandboxSafe removed: GarageBand will host WORLDBUILDER out-of-process"
    fi
    # A Developer ID signed and notarised release keeps its signature; anything else
    # is re-signed ad hoc for this Mac.
    if /usr/bin/codesign -dv --verbose=2 "$dest" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application" \
        && [[ $out_of_process == 0 ]]; then
        :
    elif ! /usr/bin/codesign --force --deep --sign - "$dest" > "$logs/codesign-sign-$(basename "$dest").txt" 2>&1; then
        fail "signing failed for $dest (see $logs/codesign-sign-$(basename "$dest").txt)"
        return 1
    fi
    if ! /usr/bin/codesign --verify --strict --verbose=2 "$dest" > "$logs/codesign-$(basename "$dest").txt" 2>&1; then
        fail "signature check failed for $dest (see $logs/codesign-$(basename "$dest").txt)"
        return 1
    fi
    ok "$(basename "$dest") installed, quarantine removed, signature valid ($(/usr/bin/file -b "$binary" | /usr/bin/sed 's/Mach-O universal binary with //'))"
    # The recorded instruments travel inside the bundle (GarageBand's sandbox reads them there).
    if [[ -f "$dest/Contents/Resources/Samples/index.json" ]]; then
        ok "$(basename "$dest"): $(/usr/bin/find "$dest/Contents/Resources/Samples" -name '*.flac' | /usr/bin/wc -l | /usr/bin/tr -d ' ') recorded notes inside ($(/usr/bin/du -sh "$dest/Contents/Resources/Samples" | /usr/bin/cut -f1))"
    else
        warn "$(basename "$dest") has no recorded samples inside; its sampled instruments use the synthesised fallbacks"
    fi
}

bold "0. Removing copies under the old company name"
remove_old_copies

bold "1-3. Copying, removing quarantine, signing"
install_bundle "$component_src" "$components/WORLDBUILDER.component"
if [[ -d "$vst3_src" ]]; then
    install_bundle "$vst3_src" "$vst3/WORLDBUILDER.vst3"
else
    warn "WORLDBUILDER.vst3 not in this folder; skipped"
fi

bold "4. Resetting the Audio Unit registry"
# GarageBand and Logic keep a cache of the Audio Units they scanned (with their names);
# it is rebuilt at their next launch, so they show the new company name.
rm -f "$HOME/Library/Caches/AudioUnitCache/com.apple.audiounits.cache" 2>/dev/null || true
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
registered=0
listing=""
for _ in $(seq 1 30); do
    listing=$(/usr/bin/auval -a 2>/dev/null | /usr/bin/grep -E '^[[:space:]]*aumu[[:space:]]+Wbld[[:space:]]+Veil' || true)
    if [[ -n "$listing" ]]; then
        registered=1
        break
    fi
    sleep 1
done
printf '%s\n' "$listing" > "$logs/install-registry.txt"
entries=$(printf '%s\n' "$listing" | /usr/bin/grep -c . || true)
if [[ $registered == 0 ]]; then
    fail "WORLDBUILDER is not listed by the Audio Unit registry after 30 s"
elif [[ $entries != 1 ]]; then
    fail "the Audio Unit registry lists WORLDBUILDER $entries times (see $logs/install-registry.txt)"
    result_registry=1
elif ! printf '%s\n' "$listing" | /usr/bin/grep -q 'Windward Audio: WORLDBUILDER'; then
    fail "the Audio Unit registry lists WORLDBUILDER under another name: $listing"
    result_registry=1
else
    ok "macOS lists WORLDBUILDER once, as Windward Audio: WORLDBUILDER"
fi

bold "5. Validating with Apple's auval"
auval_log="$logs/install-auval.txt"
set +e
/usr/bin/auval -v aumu Wbld Veil > "$auval_log" 2>&1
auval_exit=$?
set -e
if [[ $auval_exit == 0 ]] && /usr/bin/grep -q "AU VALIDATION SUCCEEDED" "$auval_log"; then
    ok "auval: PASS ($(uname -m))"
    result=${result_registry:-0}
else
    fail "auval: FAIL ($(uname -m), exit $auval_exit). Full log: $auval_log"
    /usr/bin/grep -E "FATAL|ERROR|Cannot open" "$auval_log" | /usr/bin/head -5 | /usr/bin/sed 's/^/       /'
    result=1
fi
if [[ "$(uname -m)" == arm64 ]] && /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
    set +e
    /usr/bin/arch -x86_64 /usr/bin/auval -v aumu Wbld Veil > "$logs/install-auval-x86_64.txt" 2>&1
    x86_exit=$?
    set -e
    if [[ $x86_exit == 0 ]]; then ok "auval: PASS (x86_64 under Rosetta)"; else warn "auval under Rosetta exited $x86_exit (only matters for Intel-mode hosts)"; fi
fi

bold "6. Next steps"
cat <<'NEXT'
  GarageBand
    1. Open GarageBand > Settings > Audio/MIDI and make sure "Audio Units" is ticked.
    2. Quit and reopen GarageBand.
    3. Create a Software Instrument track (Track > New Track > Software Instrument).
       Open Smart Controls (B), click Plug-ins, click the Instrument slot (the top
       slot): AU Instruments > Windward Audio > WORLDBUILDER.
    4. It plays straight away while the track is selected. No MIDI is needed.
    5. If the window shows sliders instead of the WORLDBUILDER interface, use the view menu
       at the top of the plug-in window to switch from "Controls" to "Editor".
  Logic Pro
    1. Logic Pro > Settings > Plug-in Manager, find Windward Audio > WORLDBUILDER.
       If it shows as failed, select it and click "Reset & Rescan Selection".
    2. On a Software Instrument track, click the Instrument slot:
       AU Instruments > Windward Audio > WORLDBUILDER.
NEXT
if [[ $result == 0 ]]; then
    bold "WORLDBUILDER is installed and validated."
else
    bold "WORLDBUILDER is installed, but validation failed. Send $auval_log when reporting this."
fi
exit $result
