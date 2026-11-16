#!/bin/sh
# Builds the simulator and writes screenshots of the radar and settings
# screens to sim/out/ (PNG on macOS, BMP elsewhere).
#   sim/run.sh [lat lon range_nm [adsb.json]]
# Defaults: Paris, 25 nm, the response captured in tests/fixtures/.
# For live traffic, save an API answer first, for example:
#   curl -o /tmp/ac.json https://api.adsb.lol/v2/point/51.47/-0.45/50
#   sim/run.sh 51.47 -0.45 50 /tmp/ac.json
set -e
cd "$(dirname "$0")"

LAT=${1:-48.85} LON=${2:-2.35} RANGE=${3:-25}
JSON=${4:-../tests/fixtures/adsb_point.json}

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build
mkdir -p out
build/touchsky_sim ../firmware/main/map.bin "$JSON" "$LAT" "$LON" "$RANGE" out >/dev/null

for bmp in out/*.bmp; do
    if command -v sips >/dev/null; then
        sips -s format png "$bmp" --out "${bmp%.bmp}.png" >/dev/null && rm "$bmp"
        echo "sim/${bmp%.bmp}.png"
    else
        echo "sim/$bmp"
    fi
done
