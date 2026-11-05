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
        ok &= proj.loop[i].t == ev[i].t && proj.loop[i].note == ev[i].note && proj.loop[i].len == ev[i].len;
    CHECK(proj.loop_n == 3 && ok, "its loop is kept (%d notes)", proj.loop_n);
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
            ok &= proj.loop[i].t == ev[i].t && proj.loop[i].note == ev[i].note;
        CHECK(proj.loop_n == 3 && ok, "its loop is kept (%d notes)", proj.loop_n);
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
        CHECK(proj.loop_n == 3 && proj.loop[2].note == ev[2].note, "its loop is kept");
        CHECK(proj.par[P_CLOCK] == CK_OWN && proj.par[P_PRESS] == AT_VIBRATO, "the clock and aftertouch at their defaults");
    }
    proj.par[P_BLEVEL] = 66;
    proj.seen = 5;
    project_save();
    memset(&proj, 0, sizeof proj);
    CHECK(project_load() == 0 && proj.par[P_BLEVEL] == 66 && proj.seen == 5 && proj.xp == xp, "today's save round-trips");
    printf(fails ? "PROJECT TESTS FAILED (%d)\n" : "project: all passed\n", fails);
    return fails != 0;
}
