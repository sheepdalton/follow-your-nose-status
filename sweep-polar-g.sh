#!/usr/bin/env bash
# Sweep the polar gamma exponent for --vis-polar-single.
#
# Runs gamma = 1.00, 1.25, 1.50, ... , 10.00 (37 values) and writes one SVG
# per gamma:  data/out/<stem>-polar-g<gamma>-<n>.svg  (each labelled inside
# with its gamma), so the routes can be compared side by side.
#
# Usage:
#   ./sweep-polar-g.sh [input.svg] [origin] [dest] [network-restore.csv]
#
# Defaults to the small, fully-connected billIntelligibile plan (fast).
# For a large plan pass a saved --NETWORK csv as the 4th argument to skip
# placement/heal -- but note the visibility graph is still recomputed on
# every run, so 37 passes over a big network can take a long time.

# 2.2 - 2.5 
set -eo pipefail

SVG="${1:-data/in/billIntelligibileBbox.svg}"
ORIGIN="${2:-249}"
DEST="${3:-132}"
NET="${4:-}"

if [[ ! -x ./isovist ]]; then
    echo "isovist binary not found -- run 'make' first." >&2
    exit 1
fi

RESTORE=""
if [[ -n "$NET" ]]; then
    RESTORE="--NETWORK-RESTORE $NET"
fi

echo "Sweeping polar gamma 1..10 (step 0.25) on $SVG, ${ORIGIN} -> ${DEST}"
for g in $(seq 1 0.20 6); do
    echo ">>> gamma = $g"
    ./isovist "$SVG" --vis-polar-single --ODseed "$ORIGIN" "$DEST" \
        --polar-g "$g" $RESTORE
done

echo "Done. Output SVGs: data/out/*-polar-g*-*.svg"
