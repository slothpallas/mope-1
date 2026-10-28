/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1 project: one flash object (OBJ_PROJ). A project of another format, or none (or FoMni's),
 * gives the defaults. Part of the unity build, after app.h. */
project_t proj;

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = sy_param_info(i)->def;
}

/* A project of this format loads. Each value out of its range (a setting this version added, that an older
 * project has no room for, or one whose range has changed) takes its default; the rest are kept. */
static int project_valid(int bytes)
{
    int i, have = (bytes - 8) / 2;               /* the values the saved project holds */
    if (proj.magic == PROJ_MAGIC && proj.format >= 1u && proj.format < PROJ_FORMAT) {
        /* an older format: its XP sat right after its fewer values (4-aligned), 28 spare bytes after it,
         * then (format 3) the loop. Everything after the values moves up to where it is now. */
        static uint16_t loop_n;
        static sy_ev_t loop[SY_LOOP_MAX];
        const int at = (8 + 2 * PROJ_NPAR_OF(proj.format) + 3) & ~3, at_loop = at + 4 + 28;
        uint32_t xp = 0;
        loop_n = 0;
        if (bytes >= at + 4)
            memcpy(&xp, (const uint8_t *)&proj + at, 4);
        if (proj.format == 3u && bytes >= at_loop + 2) {
            memcpy(&loop_n, (const uint8_t *)&proj + at_loop, 2);
            if (loop_n > SY_LOOP_MAX || bytes < at_loop + 2 + (int)(loop_n * sizeof(sy_ev_t)))
                loop_n = 0;
            memcpy(loop, (const uint8_t *)&proj + at_loop + 2, loop_n * sizeof(sy_ev_t));
        }
        memset((uint8_t *)&proj + at, 0, sizeof proj - (uint32_t)at);
        proj.xp = xp;
        proj.loop_n = loop_n;
        memcpy(proj.loop, loop, loop_n * sizeof(sy_ev_t));
    } else if (bytes < (int)sizeof proj) {           /* (shorter than this one: no loop) */
        proj.loop_n = 0;
    }
    if (proj.loop_n > SY_LOOP_MAX)
        proj.loop_n = 0;
    if (proj.magic != PROJ_MAGIC || proj.format < 1u || proj.format > PROJ_FORMAT)
        return 0;
    if (have > PROJ_NPAR_OF(proj.format))        /* (its spare bytes, zeros, are not settings) */
        have = PROJ_NPAR_OF(proj.format);
    proj.format = PROJ_FORMAT;
    for (i = 0; i < P_NPARAMS; i++) {
        const sy_param_t *p = sy_param_info(i);
        if (i >= have || proj.par[i] < p->lo || proj.par[i] > p->hi)
            proj.par[i] = p->def;
    }
    return 1;
}

int project_load(void)
{
    int n;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n >= 8 && project_valid(n))
        return 0;
    project_defaults();
    return -1;
}

int project_save(void)
{
    proj.loop_n = (uint16_t)sy_loop_copy(proj.loop, SY_LOOP_MAX);   /* the loop as it is now */
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        sy_set(i, proj.par[i]);
    sy_loop_load(proj.loop, proj.loop_n);
}
