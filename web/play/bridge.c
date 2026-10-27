/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1 to play in a browser: the whole app as the host simulator runs it (host/mope_host.c: the
 * firmware's UI and instrument against a simulated FM-1), compiled to WebAssembly for voice.js.
 *
 * The browser pulls sound; making it moves the simulated FM-1's clock on, a millisecond at a time,
 * so its engine and its screen keep the same time they keep on the device. Everything the page needs
 * back after a pull sits in one status block it reads in place.
 *
 *   web/play/build.sh  ->  build/play/mope.wasm */
#include <stdint.h>

static void play_audio(const int32_t *blk, uint32_t n);
#include "../../host/mope_host.c"

#undef __attribute__
#define EXPORT __attribute__((used, visibility("default")))

/* sound waiting for the browser: interleaved stereo, wrapped */
#define WAIT 8192u
static float waiting[WAIT * 2];
static uint32_t wait_in, wait_out;
static void play_audio(const int32_t *blk, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++, wait_in++) {
        float *at = &waiting[(wait_in % WAIT) * 2u];
        at[0] = (float)blk[2u * i] * (1.0f / 8388608.0f);
        at[1] = (float)blk[2u * i + 1u] * (1.0f / 8388608.0f);
    }
}

/* what the page reads after each pull */
typedef struct {
    uint32_t screen_ver;      /* goes up when the screen changed */
    uint32_t lit_buttons;     /* bit per button, as the panel's lights */
    uint32_t lit_keys, dim_keys;
    uint32_t cpu;             /* the share of the FM-1's time the app takes, % */
    uint32_t save_ver;        /* goes up when something was saved */
    uint32_t ms;              /* the simulated clock */
} play_status_t;
static play_status_t status;

static float pulled[1024 * 2];

EXPORT void play_start(void) { boot(); }

/* n frames (up to 1024) of sound, interleaved stereo; the FM-1 runs until they exist */
EXPORT float *play_pull(uint32_t n)
{
    uint32_t i;
    if (n > 1024u)
        n = 1024u;
    while (wait_in - wait_out < n)
        run_ms(1);
    for (i = 0; i < n; i++, wait_out++) {
        const float *at = &waiting[(wait_out % WAIT) * 2u];
        pulled[2u * i] = at[0];
        pulled[2u * i + 1u] = at[1];
    }
    status.screen_ver = blits;
    status.lit_buttons = lit_btn;
    status.lit_keys = lit_keys;
    status.dim_keys = dim_keys;
    status.cpu = cpu_pct;
    status.save_ver = store_writes;
    status.ms = now_ms;
    return pulled;
}
EXPORT play_status_t *play_status(void) { return &status; }
EXPORT uint16_t *play_screen(void) { return fb; }

/* the panel: what is held down, a knob's detents, the master volume (0..4096) */
EXPORT void play_hold(uint32_t buttons, uint32_t keys) { held_btn = buttons, held_keys = keys; }
EXPORT void play_turn(int knob, int32_t detents)
{
    if (knob >= 0 && knob < NE)
        enc_acc[knob] += detents;
}
EXPORT void play_volume(uint32_t v) { master = v > 4096u ? 4096u : v; }

/* a MIDI channel message, into the queue the FM-1's USB and its jack fill (as a USB-MIDI packet) */
EXPORT void play_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (mi_w - mi_r < MQ)
        min_q[mi_w++ % MQ] = (st >> 4) | (st & 0xFFu) << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24;
}

/* saved objects: the page keeps them between visits and hands them back before play_start */
EXPORT uint32_t play_saves(void) { return OBJ_NOBJ; }
EXPORT uint32_t play_save_room(void) { return PLAT_STORE_MAX; }
EXPORT uint8_t *play_save(uint32_t obj) { return obj < OBJ_NOBJ ? store[obj] : 0; }
EXPORT int play_save_size(uint32_t obj) { return obj < OBJ_NOBJ ? store_len[obj] : -1; }
EXPORT void play_save_sized(uint32_t obj, int n)
{
    if (obj < OBJ_NOBJ)
        store_len[obj] = n < 0 ? -1 : n > (int)PLAT_STORE_MAX ? (int)PLAT_STORE_MAX : n;
}
