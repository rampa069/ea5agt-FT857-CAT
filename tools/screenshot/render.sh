#!/bin/sh
# Renderiza la UI del display en PNG sin la placa: tools/screenshot/render.sh [directorio_salida]
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=${1:-$HERE/out}
TFT="$ROOT/.pio/libdeps/cyd/TFT_eSPI"
[ -d "$TFT" ] || (cd "$ROOT" && pio pkg install -e cyd)
mkdir -p "$OUT"
c++ -std=c++17 -O1 -w -I"$HERE/fake" -I"$TFT" -I"$ROOT/src" -I"$ROOT/src/ui" -I"$ROOT/include" \
  -I"$ROOT/lib/Ft8x7Cat/src" -I"$ROOT/lib/RigUi/src" \
  "$HERE/render_main.cpp" "$HERE/fake/fake_tft.cpp" "$ROOT"/src/ui/*.cpp \
  "$ROOT"/lib/Ft8x7Cat/src/*.cpp "$ROOT"/lib/RigUi/src/*.cpp \
  -o "$OUT/render"
rm -f "$OUT"/*.png
"$OUT/render" "$OUT" | tee "$OUT/render.log" | grep -v '\.ppm$' || true
for ppm in "$OUT"/*.ppm; do
  python3 "$HERE/ppm2png.py" "$ppm" "${ppm%.ppm}.png" 2
  rm "$ppm"
done
ls "$OUT"/*.png
