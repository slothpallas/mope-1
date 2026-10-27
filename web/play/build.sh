#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# MOPE-1 to play in the browser: the app (the host simulator, host/mope_host.c) as WebAssembly, its
# AudioWorklet and its page. Same sources and float flags as host/build_host.sh and the device.
#   [OM_VERSION=0.2.0] web/play/build.sh  ->  build/play/{index.html, voice.js, mope.wasm, MOPE-1.html}
set -e
cd "$(dirname "$0")/../.."
PY="${PYTHON:-python3}"
OUT=build/play
mkdir -p build/gen "$OUT"
[ -f build/gen/felucca_font.h ] || "$PY" tools/gen_font.py build/gen/felucca_font.h >/dev/null
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DOM_HOST -DOM_WEB "-DOM_VERSION=\"$(printf %s "${OM_VERSION:-DEV}" | tr a-z A-Z)\"" \
    -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o "$OUT/mope.wasm" web/play/bridge.c firmware/src/dsp/synth.c
cp web/play/voice.js "$OUT/"
"$PY" web/play/pack.py web/play/index.html "$OUT"
