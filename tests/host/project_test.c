/* SPDX-License-Identifier: GPL-3.0-only */
/* Saved sounds across versions: a format-3 save (0.1-0.2: 43 settings, the XP, 28 spare bytes, the loop)
 * loads into today's layout with its settings, its XP and its loop, the new settings at their defaults;
 * today's own save round-trips. */
#include <stdio.h>
#include <string.h>
#include "../../firmware/src/app/app.h"

static uint8_t flash[4096];
static int flash_n = -1;
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    (void)obj;
    if (flash_n < 0)
        return -1;
    memcpy(dst, flash, (uint32_t)flash_n < max ? (uint32_t)flash_n : max);
    return flash_n;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    (void)obj;
    memcpy(flash, src, len);
    flash_n = (int)len;
    return 0;
}
#include "../../firmware/src/app/project.c"

static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok   "); else { printf("  FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    const int n3 = P_LOOPBARS + 1, at = (8 + 2 * n3 + 3) & ~3, at_loop = at + 4 + 28;
    const uint32_t magic = PROJ_MAGIC, fmt = 3u, xp = 12345u;
    const uint16_t loop_n = 3;
    const sy_ev_t ev[3] = {{0, 60, 100, 11, 0}, {48, 63, 90, 22, 0}, {96, 67, 80, 33, 0}};
    int16_t par[P_NPARAMS];
    int i, ok = 1;
    sy_init();
    printf("project\n");
    /* a format-3 save, laid out as 0.1 and 0.2 wrote it */
    memset(flash, 0, sizeof flash);
    memcpy(flash, &magic, 4);
    memcpy(flash + 4, &fmt, 4);
    for (i = 0; i < n3; i++)
        par[i] = sy_param_info(i)->def;
    par[P_TABLE] = 11;
    par[P_CUTOFF] = 33;
    par[P_LOOPBARS] = 2;
    memcpy(flash + 8, par, (size_t)(2 * n3));
    memcpy(flash + at, &xp, 4);
    memcpy(flash + at_loop, &loop_n, 2);
    memcpy(flash + at_loop + 2, ev, sizeof ev);
    flash_n = at_loop + 2 + 192 * (int)sizeof(sy_ev_t);
    CHECK(project_load() == 0, "a format-3 save loads");
    CHECK(proj.par[P_TABLE] == 11 && proj.par[P_CUTOFF] == 33 && proj.par[P_LOOPBARS] == 2, "its settings are kept");
    CHECK(proj.xp == xp, "its XP is kept (%u)", (unsigned)proj.xp);
    for (i = 0; i < 3; i++)
        ok &= proj.pat_ev[0][i].t == ev[i].t && proj.pat_ev[0][i].note == ev[i].note && proj.pat_ev[0][i].len == ev[i].len;
    CHECK(proj.pat_n[0] == 3 && ok, "its loop is kept (%d notes)", proj.pat_n[0]);
    for (ok = 1, i = P_BTABLE; i < P_NPARAMS; i++)
        ok &= proj.par[i] == sy_param_info(i)->def;
    CHECK(ok && proj.seen == 0, "the new settings start at their defaults, nothing seen");
    CHECK(proj.format == PROJ_FORMAT, "it is today's format now");
    {   /* a format-4 save (0.3: 56 settings, the XP, the pages seen, the loop) keeps all of it */
        const int n4 = P_LEVEL + 1, at4 = (8 + 2 * n4 + 3) & ~3, at4_loop = at4 + 4 + 28;
        const uint32_t fmt4 = 4u;
        const uint8_t seen4 = 0x1D;
        memset(flash, 0, sizeof flash);
        memcpy(flash, &magic, 4);
        memcpy(flash + 4, &fmt4, 4);
        for (i = 0; i < n4; i++)
            par[i] = sy_param_info(i)->def;
        par[P_TABLE] = 9;
        par[P_LEVEL] = -17;
        memcpy(flash + 8, par, (size_t)(2 * n4));
        memcpy(flash + at4, &xp, 4);
        flash[at4 + 4] = seen4;
        memcpy(flash + at4_loop, &loop_n, 2);
        memcpy(flash + at4_loop + 2, ev, sizeof ev);
        flash_n = at4_loop + 2 + 192 * (int)sizeof(sy_ev_t);
        CHECK(project_load() == 0, "a format-4 save loads");
        CHECK(proj.par[P_TABLE] == 9 && proj.par[P_LEVEL] == -17 && proj.xp == xp && proj.seen == seen4,
              "its settings, XP and pages seen are kept");
        for (ok = 1, i = 0; i < 3; i++)
            ok &= proj.pat_ev[0][i].t == ev[i].t && proj.pat_ev[0][i].note == ev[i].note;
        CHECK(proj.pat_n[0] == 3 && ok, "its loop is kept (%d notes)", proj.pat_n[0]);
        for (ok = 1, i = P_RTONE; i < P_NPARAMS; i++)
            ok &= proj.par[i] == sy_param_info(i)->def;
        CHECK(ok, "the effects' new knobs start at their defaults");
    }
    {   /* a format-6 save (0.4 betas: up to the bass's way, the punches' knobs after the pages seen) keeps
         * its knobs and loop; the clock and aftertouch start at their defaults */
        const int n6 = P_BSTYLE + 1, at6 = (8 + 2 * n6 + 3) & ~3, at6_loop = at6 + 4 + 28;
        const uint32_t fmt6 = 6u;
        memset(flash, 0, sizeof flash);
        memcpy(flash, &magic, 4);
        memcpy(flash + 4, &fmt6, 4);
        for (i = 0; i < n6; i++)
            par[i] = sy_param_info(i)->def;
        par[P_MIDIOUT] = 1;                              /* (0.3's "on": now KEYS) */
        par[P_BSTYLE] = BS_PULSE;
        memcpy(flash + 8, par, (size_t)(2 * n6));
        memcpy(flash + at6, &xp, 4);
        flash[at6 + 4] = 0x21;
        flash[at6 + 5] = 0x3A;                           /* (HALF's knobs: 3 and 10 from their defaults) */
        flash[at6 + 5 + sizeof proj.pk - 1] = 0x07;
        memcpy(flash + at6_loop, &loop_n, 2);
        memcpy(flash + at6_loop + 2, ev, sizeof ev);
        flash_n = at6_loop + 2 + 192 * (int)sizeof(sy_ev_t);
        CHECK(project_load() == 0, "a format-6 save loads");
        CHECK(proj.par[P_MIDIOUT] == MO_KEYS && proj.par[P_BSTYLE] == BS_PULSE && proj.seen == 0x21 && proj.xp == xp,
              "its settings are kept (MIDI out on: the keys)");
        CHECK(proj.pk[0] == 0x3A && proj.pk[sizeof proj.pk - 1] == 0x07, "the punches' knobs are kept");
        CHECK(proj.pat_n[0] == 3 && proj.pat_ev[0][2].note == ev[2].note, "its loop is kept");
        CHECK(proj.par[P_CLOCK] == CK_OWN && proj.par[P_PRESS] == AT_VIBRATO, "the clock and aftertouch at their defaults");
    }
    {   /* a format-7 save with a full loop (192 notes): pattern 1 keeps its first SY_LOOP_MAX */
        const int n7 = P_PRESS + 1, at7 = (8 + 2 * n7 + 3) & ~3, at7_loop = at7 + 4 + 28;
        const uint32_t fmt7 = 7u;
        const uint16_t full = 192;
        static uint8_t big[4096];
        sy_ev_t e;
        memset(big, 0, sizeof big);
        memcpy(big, &magic, 4);
        memcpy(big + 4, &fmt7, 4);
        for (i = 0; i < n7; i++)
            par[i] = sy_param_info(i)->def;
        memcpy(big + 8, par, (size_t)(2 * n7));
        memcpy(big + at7, &xp, 4);
        memcpy(big + at7_loop, &full, 2);
        for (i = 0; i < 192; i++) {
            e.t = (uint16_t)(i * 4);
            e.note = (uint8_t)(40 + i % 50);
            e.vel = 100;
            e.len = 6;
            e.flags = 0;
            memcpy(big + at7_loop + 2 + i * (int)sizeof e, &e, sizeof e);
        }
        memcpy(flash, big, sizeof flash);
        flash_n = at7_loop + 2 + 192 * (int)sizeof(sy_ev_t);
        CHECK(project_load() == 0 && proj.pat == 0, "a format-7 save loads, on pattern 1");
        CHECK(proj.pat_n[0] == SY_LOOP_MAX && proj.pat_ev[0][SY_LOOP_MAX - 1].t == (SY_LOOP_MAX - 1) * 4 &&
              proj.pat_n[1] == 0 && proj.pat_n[3] == 0, "its loop is pattern 1 (its first %d notes), the rest empty", SY_LOOP_MAX);
    }
    {   /* a format-8 save (0.4.0, 0.4.1: 70 settings, then all the rest as now): everything after the settings
         * moves up; the macros start in the middle, the new effects off */
        const int n8 = P_PRESS + 1, at8 = (8 + 2 * n8 + 3) & ~3;
        const uint32_t fmt8 = 8u;
        const uint16_t pn[SY_PATS] = {2, 0, 0, 1};
        const int at_pn = at8 + 4 + 1 + (int)sizeof proj.pk + 1 + SY_PATS + (int)sizeof proj.rsv, at_ev = at_pn + 2 * SY_PATS;
        static uint8_t big[4096];
        memset(big, 0, sizeof big);
        memcpy(big, &magic, 4);
        memcpy(big + 4, &fmt8, 4);
        for (i = 0; i < n8; i++)
            par[i] = sy_param_info(i)->def;
        par[P_TABLE] = 7;
        par[P_PRESS] = AT_WAVE;
        memcpy(big + 8, par, (size_t)(2 * n8));
        memcpy(big + at8, &xp, 4);
        big[at8 + 4] = 0x81;                             /* seen */
        big[at8 + 5] = 0x3A;                             /* pk[0] */
        big[at8 + 5 + sizeof proj.pk] = 3;               /* pat */
        big[at8 + 5 + sizeof proj.pk + 1 + 2] = 2;       /* pat_bars[2] */
        memcpy(big + at_pn, pn, sizeof pn);
        memcpy(big + at_ev, ev, 2 * sizeof ev[0]);
        memcpy(big + at_ev + 3 * SY_LOOP_MAX * (int)sizeof(sy_ev_t), &ev[2], sizeof ev[0]);
        flash_n = (int)sizeof(project_t) - 2 * (P_NPARAMS - n8);
        memcpy(flash, big, (size_t)flash_n);
        CHECK(project_load() == 0, "a format-8 save (0.4.1) loads");
        CHECK(proj.par[P_TABLE] == 7 && proj.par[P_PRESS] == AT_WAVE && proj.xp == xp && proj.seen == 0x81 && proj.pk[0] == 0x3A,
              "its settings, XP, pages seen and punch knobs are kept");
        CHECK(proj.pat == 3 && proj.pat_bars[2] == 2 && proj.pat_n[0] == 2 && proj.pat_n[3] == 1 &&
              proj.pat_ev[0][1].note == ev[1].note && proj.pat_ev[3][0].note == ev[2].note, "its four patterns are kept");
        CHECK(proj.par[P_MEYES] == 50 && proj.par[P_MROOM] == 50 && proj.par[P_SWIRL] == 0 && proj.par[P_BURN] == 0 &&
              proj.par[P_MUMBLE] == 0 && proj.par[P_MBRESO] == 50, "the macros in the middle, the new effects off");
    }
    {   /* your presets from 0.4.1 (format 1: 80 values a slot): each slot's name and sound kept */
        const uint32_t pm = PRESETS_MAGIC;
        const uint16_t pf1 = 1, pnp = P_PRESS + 1, used = 0x8001;
        const int was = PRESET_NAME + 2 * PRESET_NPAR_1;
        int16_t v;
        memset(flash, 0, sizeof flash);
        memcpy(flash, &pm, 4);
        memcpy(flash + 4, &pf1, 2);
        memcpy(flash + 6, &pnp, 2);
        memcpy(flash + 8, &used, 2);
        memcpy(flash + 12, "DAMP MONDAY", 11);
        memcpy(flash + 12 + 15 * was, "LAST ONE", 8);
        for (i = 0; i < P_PRESS + 1; i++) {
            v = sy_param_info(i)->def;
            memcpy(flash + 12 + PRESET_NAME + 2 * i, &v, 2);
            memcpy(flash + 12 + 15 * was + PRESET_NAME + 2 * i, &v, 2);
        }
        v = 66;
        memcpy(flash + 12 + PRESET_NAME + 2 * P_BLEVEL, &v, 2);
        v = 9;
        memcpy(flash + 12 + 15 * was + PRESET_NAME + 2 * P_TABLE, &v, 2);
        flash_n = 12 + PRESET_SLOTS * was;
        CHECK(presets_load() == 0 && presets.used == used, "0.4.1's presets load");
        CHECK(!memcmp(presets.slot[0].name, "DAMP MONDAY", 11) && presets.slot[0].par[P_BLEVEL] == 66 &&
              !memcmp(presets.slot[15].name, "LAST ONE", 8) && presets.slot[15].par[P_TABLE] == 9, "the first and the last slot kept");
        CHECK(presets.slot[15].par[P_MBITE] == 50 && presets.slot[0].par[P_SWIRL] == 0, "their macros in the middle");
        presets_save();
        memset(&presets, 0, sizeof presets);
        CHECK(presets_load() == 0 && presets.slot[15].par[P_TABLE] == 9 && presets.format == 2, "and round-trip in today's format");
        flash_n = -1;
    }
    proj.xp = xp;
    proj.pat = 2;
    proj.pat_bars[2] = 3;
    proj.pat_n[3] = 1;
    proj.pat_ev[3][0].note = 77;
    proj.par[P_BLEVEL] = 66;
    proj.seen = 5;
    project_save();
    memset(&proj, 0, sizeof proj);
    CHECK(project_load() == 0 && proj.par[P_BLEVEL] == 66 && proj.seen == 5 && proj.xp == xp, "today's save round-trips");
    CHECK(proj.pat == 2 && proj.pat_bars[2] == 3 && proj.pat_n[3] == 1 && proj.pat_ev[3][0].note == 77, "with its patterns");
    printf(fails ? "PROJECT TESTS FAILED (%d)\n" : "project: all passed\n", fails);
    return fails != 0;
}
