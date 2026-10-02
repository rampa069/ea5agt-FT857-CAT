#!/bin/sh
# Renderiza la UI del display en PNG sin la placa: tools/screenshot/render.sh [directorio_salida]
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=${1:-$HERE/out}
TFT="$ROOT/.pio/libdeps/cyd/TFT_eSPI"
[ -d "$TFT" ] || (cd "$ROOT" && pio pkg install -e cyd)
mkdir -p "$OUT"
c++ -std=c++17 -O1 -w -I"$HERE/fake" -I"$TFT" -I"$ROOT/src" -I"$ROOT/include" -I"$ROOT/lib/Ft8x7Cat/src" \
  "$HERE/render_main.cpp" "$HERE/fake/fake_tft.cpp" "$ROOT/src/rig_display.cpp" \
  "$ROOT/lib/Ft8x7Cat/src/ft8x7_protocol.cpp" "$ROOT/lib/Ft8x7Cat/src/rig_format.cpp" \
  -o "$OUT/render"
for ppm in $("$OUT/render" "$OUT"); do
  python3 "$HERE/ppm2png.py" "$ppm" "${ppm%.ppm}.png" 2
  rm "$ppm"
done
ls "$OUT"/*.png
