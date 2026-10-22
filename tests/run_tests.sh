#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# MOPE-1 host tests: the maths, the platform pieces kept from Felucca / X0X / FoMni / FiMba-1, the
# instrument (built without libm, as on the device; demo WAVs of every table and drum style in
# build/host/synth/), and the whole firmware app in the simulator (tests/scenarios/*.mope), with
# screenshots in build/scenarios/.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
FAIL=0
OUT=build/host
mkdir -p "$OUT" "$OUT/synth" build/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run fastmath sh -c "$CC -O2 -ffp-contract=off -Wall -Wextra -Werror -o $OUT/fastmath_test tests/host/fastmath_test.c -lm && $OUT/fastmath_test"
run encoder sh -c "$CC -O2 -w -Ifirmware/hal -o $OUT/encoder_test tests/host/encoder_test.c && $OUT/encoder_test"
run uac sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/uac_test tests/host/uac_test.c -lm && $OUT/uac_test"
run trs sh -c "$CC -O2 -w -Ifirmware/src -Ifirmware/hal -o $OUT/trs_test tests/host/trs_test.c && $OUT/trs_test"
# no -lm: a libm call that crept into the engine fails to link here, as it would on the device
run synth sh -c "$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wdouble-promotion -Werror -DOM_HOST -Ifirmware/src/dsp \
    -o $OUT/synth_test tests/host/synth_test.c firmware/src/dsp/synth.c && $OUT/synth_test $OUT/synth"
run storage sh -c "$CC -O2 -o $OUT/storage_test tests/storage_test.c && $OUT/storage_test"
# the update path, against the firmware package (Felucca's tests; needs ./build.sh)
if [ -f build/mope.fwsc ]; then
    run ota-entry sh -c "$CC -o $OUT/ota_test tests/ota_test.c && $OUT/ota_test build/mope.fwsc"
    if [ -z "${AC79_SDK:-}" ]; then
        echo "  skip update-loader (needs AC79_SDK, as the build)"
    else
    run update-loader sh -c "head -c 100000 build/mope.bin > $OUT/old_app.bin && \
        python3 tools/fm1pkg_make.py $OUT/old_app.bin build/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -o $OUT/ldr_test tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc build/mope.fwsc"
    fi
    run installer python3 tests/install_test.py
    # the package taken apart and every CRC checked; against FoMni's own package when one is there
    # (build/fomni-ref: see BUILDING.md, "Before installing")
    REF=""
    [ -f build/fomni-ref/build/omni.fwsc ] && REF="--ref build/fomni-ref/build/omni.fwsc"
    run package sh -c "python3 tools/check_fwsc.py build/mope.fwsc --app build/mope.bin $REF"
else
    echo "  skip update-path tests (no build/mope.fwsc: run ./build.sh)"
fi
run host-build sh host/build_host.sh
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh web/emu/build.sh >/dev/null 2>&1 && node tests/host/emu_test.mjs build/emu/mope.wasm"
    CHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    [ -x "$CHROME" ] || CHROME="$(command -v google-chrome || command -v chromium || true)"
    # the installer as the page ships it, against a simulated FM-1
    run updater node tests/web/updater_test.mjs
    if [ -n "$CHROME" ]; then
        run standalone node tests/host/standalone_test.mjs "$CHROME"
        if [ -f build/mope.fwsc ]; then       # the installer's buttons, end to end (needs a firmware build)
            run installer-page node tests/web/installer_page_test.mjs "$CHROME"
        fi
    fi
fi
for s in tests/scenarios/*.mope; do
    n=$(basename "$s" .mope)
    mkdir -p "build/scenarios/$n"
    run "scenario-$n" build/host/mope_host "$s" "build/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
