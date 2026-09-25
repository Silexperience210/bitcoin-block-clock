#!/bin/bash
# =====================================================================
#  Simulateur desktop du Bitcoin Block Clock
#  Compile le VRAI code de dessin du sketch (+ la vraie lib Arduino_GFX)
#  pour Linux/macOS. Réseau, tactile, I2S, FreeRTOS : simulés (shim/).
#  usage : ./build.sh [--asan]          -> build/sim
#  prérequis : arduino-cli + core esp32 2.0.14 + libs du README installés
# =====================================================================
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SK="${SKETCH:-$HERE/../../bitcoin-block-clock}"
LIBS="${ARDUINO_LIBS:-$HOME/Arduino/libraries}"
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=huge_app,CPUFreq=240,USBMode=hwcdc,CDCOnBoot=cdc"
B="$HERE/build"; mkdir -p "$B"
SAN=""; [ "$1" = "--asan" ] && SAN="-fsanitize=address,undefined"
# le prétraitement Arduino génère les prototypes : on compile exactement ce qu'arduino-cli compile
arduino-cli compile --fqbn "$FQBN" --preprocess "$SK" > "$B/sketch_pre.cpp"
GFX="$LIBS/GFX_Library_for_Arduino/src"; AJ="$LIBS/ArduinoJson/src"
CXXF="-std=gnu++17 -O1 -g -w -I$HERE/shim -I$GFX -I$AJ -I$SK -I$B \
 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 \
 -DARDUINOJSON_ENABLE_PROGMEM=0 -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 $SAN"
OBJS=""
for f in Arduino_G.cpp Arduino_GFX.cpp Arduino_DataBus.cpp canvas/Arduino_Canvas.cpp; do
  o="$B/gfx_$(basename "$f" .cpp).o"; g++ $CXXF -c "$GFX/$f" -o "$o"; OBJS="$OBJS $o"
done
for f in "$SK"/src/sam/*.c; do
  o="$B/sam_$(basename "$f" .c).o"; gcc -O1 -g -w $SAN -c "$f" -o "$o"; OBJS="$OBJS $o"
done
g++ $CXXF -c "$HERE/stubs.cpp" -o "$B/stubs.o"
g++ $CXXF -c "$HERE/harness_main.cpp" -o "$B/main.o"
g++ $SAN -o "$B/sim" "$B/main.o" "$B/stubs.o" $OBJS -lm
echo "OK -> $B/sim"
