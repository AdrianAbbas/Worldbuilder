#!/bin/bash
# The calibration chain. The orchestration pass (2026-11-05): the tier balance only (the
# Place untouched, the Drama layer and its percussion under it): a solve, then two
# corrections (each pass rebuilds; the instruments' busier-variant levels are measured in the
# first pass only). The loudness pass's arrangement calibration (arrcal, its section balance)
# is no longer used: its table stays in DramaBalanceV1.inc for the "before" renders.
# About an hour and a half on 4 cores.
# Usage: tools/drama-balance/calibrate.sh [repository] [probe directory]
repo="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
probe="${2:-$repo/build/probe}"
mkdir -p "$probe"
cd "$repo" || exit 1
P="castle,village,spaceport,factory,city,forest,hive temple,cavern,ruins,spaceship,biodome,seaship,suburbs mall,military,beach,desert,palace,tavern,station megacity,bridge,mining,alienruins,nebula"
B=./build/WorldbuilderTests_artefacts/Release/WorldbuilderTests
done_passes=""
for pass in calA calB calC; do
  novar=""; [[ "$pass" != calA ]] && novar=1
  i=0; for g in $P; do i=$((i+1)); env ${novar:+WB_NOVAR=1} $B --only balancecal --place $g > "$probe/$pass-$i.txt" 2>&1 & done; wait
  done_passes="$done_passes $pass"
  files=""; for p in $done_passes; do files="$files $(ls "$probe"/$p-*.txt)"; done
  python3 tools/drama-balance/mkbal.py $files
  cmake --build build --parallel 4 --target WorldbuilderTests 2>&1 | grep -E " error"
  echo "$pass done"
done
echo FULLCAL DONE
