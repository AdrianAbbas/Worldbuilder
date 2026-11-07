#!/usr/bin/env bash
# Runs the whole Linux test suite as four concurrent shards (about a quarter of the
# wall time of one run), then the CPU-measuring groups alone so their timings are not
# disturbed. Together the shards cover every test group and every Place.
# Usage: scripts/run-tests-parallel.sh path/to/WorldbuilderTests [log directory]
set -u
tests="${1:?the WorldbuilderTests binary}"
logs="${2:-build/test-logs}"
mkdir -p "$logs"
# The listening renders (Milestone 17: the piano, the voice, each changed part before and
# after) go beside the logs.
export WORLDBUILDER_RENDERS="$logs/renders"
first="castle,village,spaceport,factory,city,forest,hive,temple,cavern,ruins,station,megacity,bridge"
second="spaceship,biodome,seaship,suburbs,mall,military,beach,desert,palace,tavern,mining,alienruins,nebula"
rest="instrument,frameregions,smoke,samples,recordings,balance,slide,arrangement,clips,arrpersist,keys,overlap,transport,single,transitions,gameaudio,outputs,locations,parameters,rates,world,layers,climate,music,slots,patterns,playmode,determinism,power,tonal,state,models,distinct,editor,overlays,intensity,capture,automation,planet,pixel"

"$tests" --only instruments --place "$first" > "$logs/1-instruments-a.txt" 2>&1 &
"$tests" --only instruments --place "$second" > "$logs/2-instruments-b.txt" 2>&1 &
"$tests" --only places,placeworld,scenes,envbalance,envkept,hurdy,drama,dramarework > "$logs/3-places.txt" 2>&1 &
"$tests" --only "$rest" > "$logs/4-rest.txt" 2>&1 &
# The Drama loudness pass: every Place and style at each tier, the dramatic scenes played
# through, Inspire's moods (about half an hour of audio measured).
"$tests" --only loudness > "$logs/7-loudness.txt" 2>&1 &
# The orchestration pass: every Place at Drama 0 and 100 %, the developer's three Places
# through their arrangements, the listening renders.
"$tests" --only orchestra > "$logs/8-orchestra.txt" 2>&1 &
# The level and drums pass: every Place against its level before the Drama tier, the Drama
# dial moved, the recorded percussion against the main parts, the listening renders.
"$tests" --only level > "$logs/9-level.txt" 2>&1 &
# The Drama design pass: the orchestra scoring live, the risers and hits by rules, the
# percussion per world in the Place's space, the before / after renders; then the kits' level
# match measured with the recordings CI has (the table for Source/Places/FamilyTrim.inc).
( "$tests" --only design > "$logs/11-design.txt" 2>&1; WB_TRIM_RELAX=2.5 "$tests" --only familytrim > "$logs/familytrim.txt" 2>&1 ) &
wait
"$tests" --only effects,realtime > "$logs/5-cpu.txt" 2>&1
# The woodwinds pass: every woodwind set in tune and click-free, the parts level-matched, the
# Places kept, the CPU (alone: it times four busy Places), the listening renders.
"$tests" --only woodwinds > "$logs/10-woodwinds.txt" 2>&1
# The 60-second editor stress test (the GarageBand freeze), alone so it is not starved.
"$tests" --only stress > "$logs/6-stress.txt" 2>&1

passed=0
failed=0
for log in "$logs"/[0-9]-*.txt "$logs"/[0-9][0-9]-*.txt; do
    [[ -f "$log" ]] || continue
    line=$(grep -E '^[0-9]+ passed, [0-9]+ failed' "$log" | tail -1)
    if [[ -z "$line" ]]; then
        echo "$log: did not finish"
        failed=$((failed + 1))
        continue
    fi
    passed=$((passed + $(echo "$line" | cut -d' ' -f1)))
    failed=$((failed + $(echo "$line" | cut -d' ' -f3)))
    grep '^FAIL' "$log"
done
# The kits' level match as CI measures it (with the recordings), for Source/Places/FamilyTrim.inc.
if [[ -f "$logs/familytrim.txt" ]]; then sed -n '/familytrim/,$p' "$logs/familytrim.txt" | head -60; fi
echo "$passed passed, $failed failed"
[[ "$failed" -eq 0 ]]
