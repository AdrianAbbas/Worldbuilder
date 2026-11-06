#!/bin/bash
# The level and drums pass (2026-11-06): the Drama layer's balance again (the Place untouched;
# the slot drama::levelAddDb over Drama 0, the recorded percussion drama::drumsUnderDb under
# the Place's loudest instrument). The Places' own measurements (their Classic rise, their
# instruments' weights, busier variants and doublings) are kept from the table (WB_NOVAR):
# a solve and a correction, each pass rebuilt. About 25 minutes on 4 cores.
# Usage: tools/drama-balance/calibrate-level.sh [repository] [probe directory] [passes]
repo="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
probe="${2:-$repo/build/probe}"
passes="${3:-levA levB}"
mkdir -p "$probe"
cd "$repo" || exit 1
P="castle,village,spaceport,factory,city,forest,hive temple,cavern,ruins,spaceship,biodome,seaship,suburbs mall,military,beach,desert,palace,tavern,station megacity,bridge,mining,alienruins,nebula"
B=./build/WorldbuilderTests_artefacts/Release/WorldbuilderTests
for pass in $passes; do
  i=0; for g in $P; do i=$((i+1)); WB_NOVAR=1 $B --only balancecal --place $g > "$probe/$pass-$i.txt" 2>&1 & done; wait
  python3 tools/drama-balance/mkbal.py $(ls "$probe"/$pass-*.txt)
  cmake --build build --parallel 4 --target WorldbuilderTests 2>&1 | grep -E " error"
  echo "$pass done"
done
echo LEVELCAL DONE
