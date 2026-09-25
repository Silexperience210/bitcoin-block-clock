#!/bin/bash
# Captures PNG des 9 pages + vidéo de démo MP4 + GIF nouveau bloc (ffmpeg requis)
# usage : ./make_media.sh [dossier_sortie]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; SIM="$HERE/build/sim"; OUT="${1:-$HERE/out}"
[ -x "$SIM" ] || "$HERE/build.sh"
mkdir -p "$OUT/pages"
"$SIM" "$OUT/pages" pages
for f in "$OUT"/pages/*.ppm; do ffmpeg -y -loglevel error -i "$f" -vf scale=960:640:flags=neighbor "${f%.ppm}.png"; rm "$f"; done
rm -rf "$OUT/all"; mkdir -p "$OUT/all"; n=0
for m in boot tour pagesanim cube doom; do
  "$SIM" "$OUT/rec_$m" "$m"
  for f in $(ls "$OUT"/rec_$m/*.ppm | sort); do ln -sf "../rec_$m/$(basename "$f")" "$OUT/all/$(printf 'f%05d.ppm' $n)"; n=$((n+1)); done
done
ffmpeg -y -loglevel error -framerate 20 -i "$OUT/all/f%05d.ppm" -vf "scale=960:640:flags=neighbor,format=yuv420p" \
  -c:v libx264 -crf 20 -movflags +faststart "$OUT/demo.mp4"
ffmpeg -y -loglevel error -framerate 18 -start_number 40 -i "$OUT/rec_tour/f%05d.ppm" \
  -vf "split[a][b];[a]palettegen=max_colors=96:stats_mode=diff[p];[b][p]paletteuse=dither=none" -loop 0 "$OUT/new-block.gif"
echo "OK -> $OUT (pages/*.png, demo.mp4, new-block.gif)"
