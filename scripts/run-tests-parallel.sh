#!/usr/bin/env bash
# Runs the whole Linux test suite as four concurrent shards (about a quarter of the
# wall time of one run), then the CPU-measuring groups alone so their timings are not
# disturbed. Together the shards cover every test group and every Place.
# Usage: scripts/run-tests-parallel.sh path/to/WorldbuilderTests [log directory]
set -u
tests="${1:?the WorldbuilderTests binary}"
logs="${2:-build/test-logs}"
mkdir -p "$logs"
first="castle,village,spaceport,factory,city,forest,hive,temple,cavern,ruins,station,megacity,bridge"
second="spaceship,biodome,seaship,suburbs,mall,military,beach,desert,palace,tavern,mining,alienruins,nebula"
rest="smoke,arrangement,locations,parameters,rates,world,layers,climate,music,slots,patterns,playmode,determinism,power,tonal,state,models,distinct,editor,overlays,intensity,capture,automation,planet"

"$tests" --only instruments --place "$first" > "$logs/1-instruments-a.txt" 2>&1 &
"$tests" --only instruments --place "$second" > "$logs/2-instruments-b.txt" 2>&1 &
"$tests" --only places,placeworld > "$logs/3-places.txt" 2>&1 &
"$tests" --only "$rest" > "$logs/4-rest.txt" 2>&1 &
wait
"$tests" --only effects,realtime > "$logs/5-cpu.txt" 2>&1

passed=0
failed=0
for log in "$logs"/[1-5]-*.txt; do
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
echo "$passed passed, $failed failed"
[[ "$failed" -eq 0 ]]
