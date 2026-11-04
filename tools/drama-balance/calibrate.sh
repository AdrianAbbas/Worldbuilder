#!/bin/bash
# The whole calibration chain: the tier balance (two correction passes), then the
# arrangement (two passes alone, two as a second Place). Each pass rebuilds.
cd /home/user/worldbuilder
P="castle,village,spaceport,factory,city,forest,hive temple,cavern,ruins,spaceship,biodome,seaship,suburbs mall,military,beach,desert,palace,tavern,station megacity,bridge,mining,alienruins,nebula"
B=./build/WorldbuilderTests_artefacts/Release/WorldbuilderTests
latest() { ls -d /home/user/deps/probe/$1-*.txt; }
CAL=cal6; ARR=arr6; SEC=sec6
for pass in calA calB; do
  i=0; for g in $P; do i=$((i+1)); $B --only balancecal --place $g > /home/user/deps/probe/$pass-$i.txt 2>&1 & done; wait
  CAL=$pass
  python3 /home/user/deps/mkbal.py $(latest $CAL) $(latest $ARR) $(latest $SEC)
  cmake --build build --parallel 4 2>&1 | grep -E "error"
  echo "$pass errors: $(grep -h 'error ' /home/user/deps/probe/$pass-*.txt | awk '{print $7}' | sort -g | sed -n '1p;$p' | tr '\n' ' ')"
done
for pass in arrA arrB; do
  i=0; for g in $P; do i=$((i+1)); $B --only arrcal --place $g > /home/user/deps/probe/$pass-$i.txt 2>&1 & done; wait
  ARR=$pass
  python3 /home/user/deps/mkbal.py $(latest $CAL) $(latest $ARR) $(latest $SEC)
  cmake --build build --parallel 4 2>&1 | grep -E "error"
  echo "$pass: $(grep -h ARRM /home/user/deps/probe/$pass-*.txt | awk '{print $4, $6, $8}' | tr ' ' '\n' | sort -g | sed -n '1p;$p' | tr '\n' ' ')"
done
for pass in secA secB; do
  i=0; for g in $P; do i=$((i+1)); WB_SECOND=1 $B --only arrcal --place $g > /home/user/deps/probe/$pass-$i.txt 2>&1 & done; wait
  SEC=$pass
  python3 /home/user/deps/mkbal.py $(latest $CAL) $(latest $ARR) $(latest $SEC)
  cmake --build build --parallel 4 2>&1 | grep -E "error"
  echo "$pass: $(grep -h ARRM /home/user/deps/probe/$pass-*.txt | awk '{print $4, $6, $8}' | tr ' ' '\n' | sort -g | awk '{a[NR]=$1} END{print a[1], a[int(NR/2)], a[NR]}')"
done
echo FULLCAL DONE
