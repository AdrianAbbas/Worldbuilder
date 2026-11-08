#!/usr/bin/env bash
# Reproduce what a user does, then validate like a host.
#   1. unzip the release package and mark every file as a browser download
#      (com.apple.quarantine), which is what broke VEIL in GarageBand
#   2. record whether macOS on this runner refuses the quarantined copy
#   3. install with install-mac.sh --ci over a copy installed under the old company
#      name (bundle ID audio.veil.worldbuilder), which the installer must remove; the
#      registry must then list WORLDBUILDER once, as "Windward Audio: WORLDBUILDER"
#   4. auval and the AU host harness (--strict), arm64 and x86_64 (Rosetta), with a
#      session saved before the rename (Tests/fixtures/old-session.wbstate), which must
#      load unchanged
#   5. the user's files in the old folders are copied to ~/Library/Windward Audio/WORLDBUILDER
#      on the first launch, and the originals kept
# Usage: scripts/ci-user-install-test.sh <WindwardAudio-WORLDBUILDER-macOS-Universal.zip> <harness binary> <log dir>
set -euo pipefail
package="$1"
harness="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
old_session="$repo/Tests/fixtures/old-session.wbstate"
expected_name="Windward Audio: WORLDBUILDER"
logs="$3"
mkdir -p "$logs"
work="$(mktemp -d)"
status=0

# Run a command with a time limit (a Gatekeeper prompt would otherwise block forever).
limit() { local seconds="$1"; shift; /usr/bin/perl -e 'alarm shift @ARGV; exec @ARGV or die "exec failed: $!\n"' "$seconds" "$@"; }

/usr/bin/ditto -x -k "$package" "$work"
folder="$work/WindwardAudio-WORLDBUILDER-macOS-Universal"
flag="0081;$(printf '%x' "$(date +%s)");Chrome;$(/usr/bin/uuidgen)"
/usr/bin/find "$folder" -exec /usr/bin/xattr -w com.apple.quarantine "$flag" {} +
{
    echo "Gatekeeper: $(/usr/sbin/spctl --status 2>&1)"
    /usr/bin/xattr -l "$folder/WORLDBUILDER.component/Contents/MacOS/WORLDBUILDER"
} | tee "$logs/quarantine.txt"

# The user's files where versions before the rename kept them (before anything loads the
# plug-in: its first launch copies them).
old_docs="$HOME/Documents/WORLDBUILDER"
old_host="$HOME/Library/Audio/Presets/Veil Audio/WORLDBUILDER"
new_data="$HOME/Library/Windward Audio/WORLDBUILDER"
mkdir -p "$old_docs/Worlds" "$old_docs/Instrument Presets" "$old_host"
echo "<WORLDBUILDER worldFile=\"1\"/>" > "$old_docs/Worlds/Before the rename.world"
echo "<WorldbuilderInstrumentPreset/>" > "$old_docs/Instrument Presets/Before the rename.wbinst"
printf '<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0"><dict><key>name</key><string>Before the rename</string></dict></plist>\n' > "$old_host/Before the rename.aupreset"
rm -rf "$new_data" "$HOME/Library/Audio/Presets/Windward Audio/WORLDBUILDER"

# 2. Quarantined copy installed by hand, without the script (informational).
components="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$components"
rm -rf "$components/WORLDBUILDER.component"
/usr/bin/ditto "$folder/WORLDBUILDER.component" "$components/WORLDBUILDER.component"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
set +e
limit 120 /usr/bin/auval -v aumu Wbld Veil > "$logs/auval-quarantined-without-script.txt" 2>&1
quarantined_exit=$?
set -e
echo "auval on the quarantined copy (no install script): exit $quarantined_exit" | tee -a "$logs/quarantine.txt"
rm -rf "$components/WORLDBUILDER.component"

# A copy as versions before the rename installed it: the old bundle ID and the old name
# (under another file name, so only its bundle ID identifies it).
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$vst3"
old_component="$components/WORLDBUILDER-before-rename.component"
old_vst3="$vst3/WORLDBUILDER-before-rename.vst3"
for pair in "WORLDBUILDER.component:$old_component" "WORLDBUILDER.vst3:$old_vst3"; do
    src="$folder/${pair%%:*}" dest="${pair#*:}"
    rm -rf "$dest"
    /usr/bin/ditto "$src" "$dest"
    /usr/bin/xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier audio.veil.worldbuilder" "$dest/Contents/Info.plist"
    /usr/bin/codesign --force --deep --sign - "$dest" >/dev/null 2>&1
done
/usr/libexec/PlistBuddy -c "Set :AudioComponents:0:name Veil Audio: WORLDBUILDER" "$old_component/Contents/Info.plist"
/usr/bin/codesign --force --deep --sign - "$old_component" >/dev/null 2>&1
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
{
    echo "Registry before the install (the copy under the old name):"
    /usr/bin/auval -a 2>/dev/null | /usr/bin/grep -E 'Wbld' || echo "(not listed)"
} | tee "$logs/registry-before.txt"

# 3. The user's path: run the installer from the quarantined download.
set +e
limit 300 /bin/bash "$folder/install-mac.sh" --ci 2>&1 | tee "$logs/install-mac.txt"
install_exit=${PIPESTATUS[0]}
set -e
echo "install-mac.sh exit $install_exit"
if [[ $install_exit != 0 ]]; then echo "::error::install-mac.sh failed (exit $install_exit)"; status=1; fi
if /usr/bin/xattr -lr "$components/WORLDBUILDER.component" | /usr/bin/grep -q com.apple.quarantine; then
    echo "::error::installed WORLDBUILDER.component is still quarantined"; status=1
fi
for old in "$old_component" "$old_vst3"; do
    if [[ -e "$old" ]]; then echo "::error::install-mac.sh left the copy under the old name: $old"; status=1; fi
done
# The registry lists WORLDBUILDER once, under the new company name.
listing=$(/usr/bin/auval -a 2>/dev/null | /usr/bin/grep -E 'Wbld' || true)
printf 'Registry after the install:\n%s\n' "$listing" | tee "$logs/registry.txt"
entries=$(printf '%s\n' "$listing" | /usr/bin/grep -c . || true)
if [[ $entries != 1 ]]; then
    echo "::error::the registry lists WORLDBUILDER $entries times"; status=1
elif ! printf '%s\n' "$listing" | /usr/bin/grep -qE "aumu[[:space:]]+Wbld[[:space:]]+Veil[[:space:]]+-[[:space:]]+$expected_name"; then
    echo "::error::the registry does not list WORLDBUILDER as '$expected_name': $listing"; status=1
else
    echo "registry: WORLDBUILDER listed once, as $expected_name"
fi

# 4. Validators on the installed copy.
run_check() {
    local name="$1"; shift
    set +e
    limit 600 "$@" > "$logs/$name.txt" 2>&1
    local code=$?
    set -e
    echo "$name: exit $code"
    if [[ $code != 0 ]]; then echo "::error::$name failed (exit $code)"; status=1; fi
}
strict=""  # becomes -strict when this auval supports it
/usr/bin/auval -h 2>&1 | /usr/bin/grep -qE -- '(^|[[:space:]])-strict([[:space:]]|$)' && strict="-strict"
# shellcheck disable=SC2086  # $strict is empty or one flag
run_check auval-arm64 /usr/bin/arch -arm64 /usr/bin/auval $strict -v aumu Wbld Veil
# Diagnostic run: reported in the logs, never fails the build.
run_diagnostic() {
    local name="$1"; shift
    set +e
    limit 600 "$@" > "$logs/$name.txt" 2>&1
    local code=$?
    set -e
    echo "$name (diagnostic, not a gate): exit $code"
}
# One process per hosting route, so a crash in one cannot hide the others. The
# brief's routes (AUv2 API out-of-process and in-process) are gates. The AUAudioUnit
# route is diagnostic only: as driven by the harness it gives silence/-10876 for
# Apple's own AULowpass too (see docs/DEVLOG.md), so it cannot judge WORLDBUILDER yet.
suites="v2-oop v2-inproc"
for suite in $suites; do run_check "harness-arm64-$suite" /usr/bin/arch -arm64 "$harness" --strict --only "$suite" --expect-name "$expected_name" --old-session "$old_session"; done
run_diagnostic harness-arm64-auaudiounit /usr/bin/arch -arm64 "$harness" --only auaudiounit
if /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
    # shellcheck disable=SC2086
    run_check auval-x86_64 /usr/bin/arch -x86_64 /usr/bin/auval $strict -v aumu Wbld Veil
    for suite in $suites; do run_check "harness-x86_64-$suite" /usr/bin/arch -x86_64 "$harness" --strict --only "$suite" --expect-name "$expected_name" --old-session "$old_session"; done
    run_diagnostic harness-x86_64-auaudiounit /usr/bin/arch -x86_64 "$harness" --only auaudiounit
else
    echo "::error::Rosetta unavailable: the x86_64 slice was not validated"; status=1
fi
# Control (diagnostic, not a gate): Apple's own DLS synth through the AUAudioUnit route,
# to tell a harness problem on that route from a WORLDBUILDER problem.
set +e
limit 300 "$harness" --only auaudiounit --no-window --component "aumu:dls :appl" > "$logs/control-apple-dls-auaudiounit.txt" 2>&1
echo "control (Apple DLSMusicDevice, AUAudioUnit route): exit $?"
set -e
grep -h -E "^(FAIL|SUMMARY)" "$logs"/harness-*.txt || true

# 5. The first launch copied the user's files to the new folder and kept the originals.
{
    echo "Migrated files in $new_data:"
    /usr/bin/find "$new_data" "$HOME/Library/Audio/Presets/Windward Audio/WORLDBUILDER" -type f 2>/dev/null || true
} | tee "$logs/migration.txt"
for pair in "Worlds/Before the rename.world:$old_docs/Worlds/Before the rename.world" \
            "Instrument Presets/Before the rename.wbinst:$old_docs/Instrument Presets/Before the rename.wbinst" \
            "Host Presets/Before the rename.aupreset:$old_host/Before the rename.aupreset"; do
    migrated="$new_data/${pair%%:*}" original="${pair#*:}"
    if ! /usr/bin/cmp -s "$migrated" "$original"; then echo "::error::not migrated: $migrated"; status=1; fi
    if [[ ! -f "$original" ]]; then echo "::error::the original was not kept: $original"; status=1; fi
done
if ! /usr/bin/cmp -s "$HOME/Library/Audio/Presets/Windward Audio/WORLDBUILDER/Before the rename.aupreset" "$old_host/Before the rename.aupreset"; then
    echo "::error::the host preset is not in the host's folder for the new name"; status=1
fi
[[ -f "$new_data/.migrated" ]] && echo "migration: $(cat "$new_data/.migrated")"
python3 "$(dirname "${BASH_SOURCE[0]}")/ci-crash-summary.py" 60 WorldbuilderAUHarness AUHostingService auval WORLDBUILDER > "$logs/crashes.txt" 2>&1 || true
cat "$logs/crashes.txt"
exit $status
