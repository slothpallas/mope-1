#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Look for crashes: the host simulator built with the address and undefined-behaviour sanitizers (and
# float-to-int overflow), driven by random sessions (tools/fuzz_scenario.py). Any report fails it.
#   tests/fuzz.sh [RUNS] [COMMANDS] [FIRST SEED]
set -e
cd "$(dirname "$0")/.."
RUNS=${1:-8}
N=${2:-6000}
FIRST=${3:-1}
PY="${PYTHON:-python3}"
mkdir -p build/gen build/fuzz
[ -f build/gen/felucca_font.h ] || "$PY" tools/gen_font.py build/gen/felucca_font.h >/dev/null
# (macOS 26's AddressSanitizer can't start: there, FUZZ_SAN=undefined; CI on Linux runs both)
SAN=${FUZZ_SAN:-address,undefined}
${CC:-cc} -O1 -g -ffp-contract=off -std=c99 -fno-omit-frame-pointer \
    -fsanitize=$SAN,float-cast-overflow -fno-sanitize-recover=all \
    -DOM_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen -o build/fuzz/mope_host_san host/mope_host.c \
    firmware/src/dsp/synth.c -lm
fail=0
s=$FIRST
while [ $s -lt $((FIRST + RUNS)) ]; do
    "$PY" tools/fuzz_scenario.py $s "$N" > build/fuzz/s$s.mope
    if ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
        build/fuzz/mope_host_san build/fuzz/s$s.mope build/fuzz/out$s > build/fuzz/s$s.log 2>&1; then
        echo "  ok   fuzz seed $s ($N commands)"
    else
        echo "  FAIL fuzz seed $s: build/fuzz/s$s.log"
        grep -m3 "runtime error\|ERROR: AddressSanitizer\|SUMMARY" build/fuzz/s$s.log | sed 's/^/       /'
        fail=1
    fi
    s=$((s + 1))
done
exit $fail
