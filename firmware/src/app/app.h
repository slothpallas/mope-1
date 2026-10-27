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
#define PROJ_FORMAT 3u                    /* 3: the loop; 2: the mixer, the gunk, swing, human, the arpeggiator */
#define PROJ_NPAR_OF(f) ((f) == 1u ? P_VOLUME + 1 : (f) == 2u ? P_ARPGATE + 1 : P_NPARAMS)

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* synth.h P_* */
    uint32_t xp;                         /* notes played, ever (the blob's levels) */
    uint8_t rsv[28];                     /* room to grow */
    uint16_t loop_n;                     /* the loop's notes (format 3 on) */
    sy_ev_t loop[SY_LOOP_MAX];
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
