/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1 app: the project (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. (FiMba-1's app.h, reshaped.) */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/synth.h"
#include "../dsp/mopemath.h"

#ifndef OM_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define OM_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x45504F4Du           /* "MOPE": never a FiMba-1 or FoMni project */
/* Add a setting at the end of P_*, bump the format, and add its count here. */
#define PROJ_FORMAT 8u                    /* 8: four patterns; 7: the clock, aftertouch; 6: the settings page (voices, tempo, room, bass); 5: the effects' new knobs; 4: the shadow, the filter kinds, the sigh, the tremble; 3: the loop; ... */
#define PROJ_NPAR_OF(f) ((f) == 1u ? P_VOLUME + 1 : (f) == 2u ? P_ARPGATE + 1 : (f) == 3u ? P_LOOPBARS + 1 : \
                         (f) == 4u ? P_LEVEL + 1 : (f) == 5u ? P_DUST + 1 : (f) == 6u ? P_BSTYLE + 1 : P_NPARAMS)
#define PROJ_OLD_LOOP 192                 /* a loop's notes up to format 7 (one loop) */

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* synth.h P_* */
    uint32_t xp;                         /* notes played, ever (the blob's levels) */
    uint8_t seen;                        /* other pages found (SHADOW 1, SIGH 2, TREMBLE 4, SETUP 8, DECK 16, TEARS 32,
                                            COPIES 64, DUST 128): no more nudges */
    uint8_t pk[PU_N * PU_KN / 2];        /* the punches' knobs, a nibble each: how far from SY_PUNCH_KDEF (0: as
                                            it comes, so older projects get the knobs as they were) */
    uint8_t pat;                         /* the pattern playing (format 8 on) */
    uint8_t pat_bars[SY_PATS];           /* each one's bars: P_LOOPBARS + 1 (0: as the one playing) */
    uint8_t rsv[22 - PU_N * PU_KN / 2];  /* room to grow */
    uint16_t pat_n[SY_PATS];             /* the patterns: their notes (up to format 7: one loop, here) */
    sy_ev_t pat_ev[SY_PATS][SY_LOOP_MAX];
} project_t;

extern project_t proj;
_Static_assert(PU_KMAX == 15 && PU_N * PU_KN / 2 <= 22, "the punches' knobs: a nibble each, in the spare bytes");
_Static_assert(sizeof(project_t) <= PLAT_STORE_MAX, "a project fits its flash object");

/* Your presets: 16 slots, each a name and a whole sound (every setting kept; a preset loads only the
 * sound's, not the mixer, the tempo or the MIDI). Their own flash object (OBJ_PRESETS). */
#define PRESETS_MAGIC 0x53504F4Du        /* "MOPS" */
#define PRESET_SLOTS 16
#define PRESET_NAME 16                   /* characters, no terminator needed */
#define PRESET_NPAR 80                   /* room for settings to come (P_NPARAMS now) */
typedef struct {
    char name[PRESET_NAME];
    int16_t par[PRESET_NPAR];
} preset_slot_t;
typedef struct {
    uint32_t magic;
    uint16_t format, npar;               /* npar: how many of par[] the saved slots hold */
    uint16_t used, rsv;                  /* used: a bit a filled slot */
    preset_slot_t slot[PRESET_SLOTS];
} presets_t;
_Static_assert(P_NPARAMS <= PRESET_NPAR, "a preset slot holds every setting");
_Static_assert(sizeof(presets_t) <= PLAT_STORE_MAX - 512u, "presets: the object's last 512 bytes stay unwritten (the loader)");
extern presets_t presets;
int presets_load(void);                  /* 0 = loaded; else none */
int presets_save(void);                  /* 0 = saved */

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */
int project_pk(int fx, int k);           /* a punch's knob, 0..PU_KMAX */
void project_pk_set(int fx, int k, int v);
void project_send_knobs(void);           /* the punches' knobs to the engine */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
