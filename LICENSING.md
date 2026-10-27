# MOPE-1 licensing

MOPE-1 is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full
text in `LICENSE`). Its instrument, screen, effects, maths and browser player are its own; it runs on
the open FM-1 platform of Felucca, which reached it through X0X, FoMni and FiMba-1 (the firmware
MOPE-1 started from), and it keeps that platform's licence. If you distribute MOPE-1, or firmware
derived from it, give your recipients its complete corresponding source under the same licence.

## MOPE-1's own

The wavetable synth and its backing band, with the room (reverb) and the tears (delay)
(`firmware/src/dsp/synth.{c,h}`); its maths (`firmware/src/dsp/mopemath.h`); the blob, its room and its
pixels (`firmware/src/app/{blob,px}.c`) and the controls (`firmware/src/app/ui.c`, apart from the frame
below); the browser player (`web/play/`); the host simulator's scenario runner (`host/mope_host.c`);
the tests of all of these (`tests/host/{synth,mopemath,play,page}_test.*`, `tests/scenarios/`) and the
documentation. Copyright (C) 2026 slothpallas.

## The platform underneath

| What | Origin | Licence |
| --- | --- | --- |
| Drivers and start-up: `firmware/hal/`, `firmware/loader/` (the update loader, unchanged), `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `firmware/crt0.S`, `firmware/app.ld`; the tools in `tools/` (build, package, install, rescue, font); the installer's protocol `web/fm1{ota,pkg}.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x), [FoMni](https://github.com/charlesvestal/fm1-fomni) and [FiMba-1](https://github.com/jadamsowers/fm1-fimba) (USB names changed here) | GPL-3.0-only |
| The app's frame: `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, `firmware/src/main.c`, `app.h`, `project.c`, and in `ui.c` the page bands, knob acceleration and autosave | FoMni and FiMba-1, from X0X and Felucca | GPL-3.0-only |
| The simulator's shape (a scripted FM-1 on the host) and the scenario commands; the CI workflow (`.github/workflows/web.yml`) | FiMba-1 / FoMni | GPL-3.0-only |
| Platform tests: `tests/host/{encoder,uac,trs}_test.c`, `tests/{storage,ota,ldr}_test.c`, `tests/install_test.py`, `tests/web/` (a simulated FM-1 for the installer) | FoMni / Felucca | GPL-3.0-only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors), the UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), an alternative font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and "FM-1" are
trademarks of their respective owners. MOPE-1 is independent firmware, not affiliated with, endorsed
by or supported by any of them.

## Radio

MOPE-1 never enables the Bluetooth / Wi-Fi radio of the hardware.
