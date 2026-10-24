/* SPDX-License-Identifier: GPL-3.0-only */
/* The Blob. It is the sound. It floats in the middle of a small sad room, a rainy window behind it,
 * and everything you turn happens to it:
 *
 *   BODY   its outline is the wavetable (Table, Position) wrapped round it, wobbling with the live
 *          output; Detune is a ghost of itself, drifting apart; Sub makes it heavier
 *   EYES   Cutoff is how open they are (closed: stoned); Resonance shrinks the pupils until they glow,
 *          then lasers; the mod envelope lifts the eyebrows on every note
 *   MOUTH  the amp envelope: it opens with each note, stays at the sustain, closes with the release
 *          (a long one is a yawn)
 *   VIBE   the LFO: it sways, bobs, bounces, nods or twitches, at the rate, as far as the depth
 *   TEARS  the delay: one tear an echo, the next one smaller, falling as far apart as the echoes are;
 *          they pool on the floor. Chorus: more of it, out of step, around it
 *   WALLS  the reverb: waves from it hit the room's walls and come back (the walls flare where they
 *          land); a bigger room pushes the walls out and the blob gets smaller in it, then the void;
 *          Fry deep-fries it
 *   DECK   the drums: a cassette in the corner, the style on its label, reels turning at the tempo.
 *          Kicks squash it, snares make it blink, hats are rain on the window
 *   VOICES each extra note held is a small blob next to it; a stolen voice gets yeeted
 *   CLIPS  it catches fire and says nothing (this is fine)
 *   IDLE   it falls asleep; much later it wakes up and stares at you
 *
 * Under it, while you work on a part, that part's schematic: the classic drawing of it (the envelope's
 * shape, the LFO's wave, the filter's curve, the wavetable's frames, the echoes, the room's tail, the
 * drum grid), live, with each knob's name over its meter. Warm colours mean something is happening.
 *
 * Part of the unity build (ui.c includes it after px.c). Reads proj, ui, sy_tel; writes only pixels
 * and its own animation state. */

static struct {
    uint32_t last_ms;
    float t, dt;
    float kick, snare, bass, hat;                       /* pulses, 1 on a hit, falling */
    uint32_t kick_n, snare_n, hat_n, bass_n, notes, steals, clips;
    uint8_t gates;
    float fire, blink, blink_due, shrug, tape_anim, pancake, sleep, stare, reel;
    int8_t tape_shown, tape_dir;
    float puddle, panel, wall_flash, echo_flash;
    struct { float x, y, vy, vx, s; uint8_t on; } tear[24];
    struct { float due, s; uint8_t on, side; } echo[10];
    struct { float r, a, dir; uint8_t on; } wave[6];
    struct { float x, y, vx, vy, life, spin; uint8_t kind, on; } p[48];
    float rain_x[30], rain_y[30];
    float zoom, focus;
    float lang, lang_to;
    float lean, wob;                                    /* the LFO's: the body's tilt, the lumps' wobble */
    float drip_x[24], drip_y[24], drip_v[24], drip_g[24];   /* the text dripping down the side walls */
    float vamt;                                         /* how big the LFO's dance is now (notes: big; idle: small) */                                /* the light's angle round it (turns) and where it goes */
    uint32_t lnotes;                                  /* the camera: how close, on what (in radii from the centre) */
    float sigh_due;
    uint32_t rng;
} bs;

enum { PK_SPLASH, PK_FIRE, PK_ZZZ, PK_STEAM, PK_YEET, PK_PUFF, PK_SWEAT, PK_SPARK };

static inline uint32_t b_rnd(void)
{
    uint32_t x = bs.rng ? bs.rng : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return bs.rng = x;
}
static inline float b_rf(void) { return (float)(b_rnd() >> 8) * (1.0f / 16777216.0f); }
static inline float b_rs(void) { return b_rf() * 2.0f - 1.0f; }
static inline float pn(int p) { return (float)(proj.par[p] - sy_param_info(p)->lo) /
                                       (float)(sy_param_info(p)->hi - sy_param_info(p)->lo); }
static inline float approach(float v, float to, float k) { return v + (to - v) * (k > 1.0f ? 1.0f : k); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

static void spawn(uint8_t kind, float x, float y, float vx, float vy, float life)
{
    int i;
    for (i = 0; i < 48; i++)
        if (!bs.p[i].on) {
            bs.p[i].on = 1;
            bs.p[i].kind = kind;
            bs.p[i].x = x;
            bs.p[i].y = y;
            bs.p[i].vx = vx;
            bs.p[i].vy = vy;
            bs.p[i].life = life;
            bs.p[i].spin = b_rf();
            return;
        }
}

/* behind the drawer's glass: each colour a darker one */
static const uint8_t GLASS[PC_N] = {
    [PC_BG0] = PC_BG0, [PC_BG1] = PC_BG0, [PC_BG2] = PC_BG1, [PC_RAIN] = PC_BG2, [PC_RAIN2] = PC_RAIN,
    [PC_WALL] = PC_BG1, [PC_WALL2] = PC_BG2, [PC_BLOB0] = PC_BLOBD, [PC_BLOB1] = PC_BLOB0, [PC_BLOB2] = PC_BLOBM,
    [PC_BLOB3] = PC_BLOB0, [PC_EYE] = PC_DIM, [PC_PUPIL] = PC_BG0, [PC_TEAR] = PC_RAIN, [PC_TEAR2] = PC_RAIN,
    [PC_MOUTH] = PC_BG0, [PC_TONGUE] = PC_EMBER, [PC_LAMP] = PC_LAMP2, [PC_LAMP2] = PC_EMBER, [PC_FIRE] = PC_EMBER,
    [PC_FIRE2] = PC_LAMP2, [PC_LASER] = PC_EMBER, [PC_FRY] = PC_EMBER, [PC_CRUST] = PC_FRY, [PC_WHITE] = PC_DIM,
    [PC_DIM] = PC_BG2, [PC_TAPE] = PC_TAPE2, [PC_TAPE2] = PC_BG1, [PC_XP] = PC_BG2, [PC_GHOST] = PC_BG2,
    [PC_PINK] = PC_EMBER, [PC_BLACK] = PC_BLACK, [PC_ORANGE] = PC_EMBER, [PC_RED] = PC_EMBER, [PC_AMBER] = PC_LAMP2,
    [PC_CORAL] = PC_EMBER, [PC_EMBER] = PC_BG1, [PC_PANEL] = PC_PANEL, [PC_GRID] = PC_BG1, [PC_BLOBD] = PC_BG0,
    [PC_BLOBM] = PC_BLOBD, [PC_SPEC] = PC_BLOB0, [PC_EYE2] = PC_BG2, [PC_SHEEN] = PC_BG1,
    [PC_SMOKE] = PC_BLACK, [PC_SMOKE2] = PC_SMOKE, [PC_SPOOL] = PC_BG0,
    [PC_COOLW] = PC_BG0, [PC_WARMW] = PC_BG0, [PC_WARMW2] = PC_BG1,
};

static int tape_up(void) { return sy_tel.drums_on || sy_tel.rec || ui.page == PG_DECK || ui.page == PG_LOOP || bs.tape_anim > 0.0f; }

static void blob_init(void)
{
    int i;
    memset(&bs, 0, sizeof bs);
    bs.rng = 0x1234567u;
    for (i = 0; i < 30; i++) {
        bs.rain_x[i] = b_rf();
        bs.rain_y[i] = b_rf();
    }
    bs.tape_shown = (int8_t)proj.par[P_STYLE];
    bs.tape_dir = 1;
    bs.kick_n = sy_tel.kick_n;
    bs.snare_n = sy_tel.snare_n;
    bs.hat_n = sy_tel.hat_n;
    bs.bass_n = sy_tel.bass_n;
    bs.notes = sy_tel.notes;
    bs.steals = sy_tel.steals;
    bs.clips = sy_tel.clips;
    bs.blink_due = 3.0f;
    bs.panel = ui.panel ? 1.0f : 0.0f;
    bs.lang = bs.lang_to = 0.13f;                        /* low and to the right, to begin */
    bs.lnotes = sy_tel.notes;
}

/* ------------------------------------------------------------ geometry --- */
typedef struct {
    float cx, cy, r;                                    /* the blob: centre, radius */
    float face_dy;                                      /* the nod */
    float ab;                                           /* the room's area ends here (the whole screen: the drawer is see-through) */
    float drawer;                                       /* the drawer's top (PX_H: closed) */
    float rcx, rcy;                                     /* where the blob floats */
    float vy;                                           /* the back wall's middle: above it (we look down into the room) */
    float bw, bh;                                       /* the back wall's half width, half height */
    float floor;                                        /* the floor under the blob */
} geo_t;

/* ------------------------------------------------------------ the room --- */
/* Text, dripping down the side walls: tiny broken pieces of the font, one step off the wall's colour,
 * sliding down. Texture more than writing; cold on the left, warm on the right. */
static float side_lx[PX_H], side_rx[PX_H];
#define NDRIP 24
static void drips_move(void)
{
    int i;
    for (i = 0; i < NDRIP; i++) {
        bs.drip_y[i] += bs.dt * bs.drip_v[i];
        bs.drip_g[i] += bs.dt * 2.5f;                    /* (the pieces flicker to other pieces) */
        if (bs.drip_y[i] > (float)PX_H + 24.0f || bs.drip_v[i] <= 0.0f) {
            const int left = i & 1;
            bs.drip_x[i] = left ? 1.0f + b_rf() * 16.0f : (float)PX_W - 5.0f - b_rf() * 16.0f;
            bs.drip_y[i] = bs.drip_v[i] <= 0.0f ? b_rf() * (float)PX_H : -b_rf() * 40.0f;   /* (at first: all over) */
            bs.drip_v[i] = 5.0f + b_rf() * 9.0f;
            bs.drip_g[i] = b_rf() * 40.0f;
        }
    }
}
static void draw_drips(void)
{
    int i, k;
    for (i = 0; i < NDRIP; i++) {
        const int left = i & 1;
        const int32_t x = (int32_t)bs.drip_x[i];
        for (k = 0; k < 8; k++) {                        /* a head and its trail, each a piece of a glyph */
            const int32_t y = (int32_t)bs.drip_y[i] - k * 4;
            const uint32_t gl = fontd((char)('A' + ((int)bs.drip_g[i] + k * 7 + i * 3) % 26));
            const int row = (int)(((uint32_t)i * 7u + (uint32_t)k * 3u) % 4u);   /* which 2 x 3 piece of it */
            const uint8_t c = k < 2 ? (left ? PC_RAIN : PC_WARMW2) : (left ? PC_WALL : PC_WARMW);
            int r, q;
            if (y < 0 || y >= PX_H)
                continue;
            for (r = 0; r < 3; r++)
                for (q = 0; q < 3; q++) {
                    const int32_t px = x + q, py = y + r;
                    if (py < 0 || py >= PX_H)
                        continue;
                    if (left ? (float)px >= side_lx[py] - 1.0f : (float)px < side_rx[py] + 1.0f)
                        continue;                        /* only on the walls */
                    if (k > 3 && px_dith(px, py, 2))     /* (the trail breaks up) */
                        continue;
                    if (gl >> ((5 - (row + r)) * 4 + (3 - q)) & 1u)
                        px_pset(px, py, c);
                }
        }
    }
}

static void draw_room(const geo_t *g)
{
    const float size = pn(P_SIZE), voidness = fm_clampf((size - 0.82f) / 0.18f, 0.0f, 1.0f);
    const int32_t ab = (int32_t)g->ab;
    const int32_t x0 = (int32_t)(g->rcx - g->bw), x1 = (int32_t)(g->rcx + g->bw);
    const int32_t y0 = (int32_t)(g->vy - g->bh), y1 = (int32_t)(g->vy + g->bh);
    const int32_t wx0 = x0 + 4, wx1 = x1 - 4, wy0 = y0 + 4, wy1 = y1 - 6;
    const int win = wx1 - wx0 > 8 && wy1 - wy0 > 6;
    int32_t x, y;
    /* the room in tones alone, no lines: the back wall lit grey-blue, the side walls darker toward
     * you, the ceiling dark, the floor taking the window's light */
    for (y = 0; y < ab; y++) {
        const float t_top = y < y0 ? (float)(y0 - y) / (float)(y0 > 0 ? y0 : 1) : 0.0f;
        const float t_bot = y > y1 ? (float)(y - y1) / (float)(ab - y1 > 0 ? ab - y1 : 1) : 0.0f;
        const float t = t_top > t_bot ? t_top : t_bot;
        const float lx = lerpf((float)x0, 0.0f, t), rx = lerpf((float)x1, (float)PX_W, t);
        for (x = 0; x < PX_W; x++) {
            uint8_t c;
            if ((float)x < lx || (float)x >= rx) {       /* a side wall: darker the nearer it is */
                const int left = (float)x < lx;
                const float near = left ? 1.0f - (float)x / (lx + 1.0f) : ((float)x - rx) / ((float)PX_W - rx + 1.0f);
                const float dv = px_dith4(x, y) + 0.5f;
                uint32_t hh = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
                float nz;
                hh = (hh ^ (hh >> 13)) * 1274126177u;
                hh ^= hh >> 16;
                nz = (float)(hh & 255u) / 255.0f;        /* (noise, not a grid: plaster) */
                c = dv > near * 1.1f ? PC_BG1 : PC_BG0;
                if (nz < (1.0f - near) * 0.55f)          /* left: cold from the window; right: the lamp's warmth */
                    c = left ? PC_COOLW : PC_WARMW;
            } else if (y < y0) {
                c = px_dith4(x, y) + 0.5f > t * 1.4f ? PC_BG1 : PC_BG0;   /* the ceiling */
            } else if (y > y1) {                         /* the floor: the window's light falls on it */
                const float wc = ((float)wx0 + (float)wx1) * 0.5f, ww = ((float)wx1 - (float)wx0) * 0.5f * (1.0f + t * 1.6f);
                const float lit = win ? fm_clampf(1.0f - fm_fabsf((float)x - wc) / (ww + 1.0f), 0.0f, 1.0f) * (1.0f - t * 0.8f) : 0.0f;
                c = px_dith4(x, y) + 0.5f < lit * 0.9f ? PC_SHEEN : px_dith4(x, y) + 0.5f > t ? PC_BG2 : PC_BG1;
            } else {                                     /* the back wall: lit in the middle, its edges fading */
                const float ex = fm_clampf(fm_minf((float)(x - x0), (float)(x1 - x)) / (g->bw * 0.45f + 1.0f), 0.0f, 1.0f);
                const float ey = fm_clampf(fm_minf((float)(y - y0), (float)(y1 - y)) / (g->bh * 0.4f + 1.0f), 0.0f, 1.0f);
                c = px_dith4(x, y) + 0.5f < ex * ey * (1.0f - 0.4f * (float)(y - y0) / (float)(y1 - y0 + 1)) ? PC_BG2 : PC_BG1;
            }
            px_fb[y * PX_W + x] = c;
        }
        side_lx[y] = lx;                                 /* (for the drips: where the side walls are) */
        side_rx[y] = rx;
    }
    if (win) {                                           /* the window: rain outside, the lamp's glow */
        const float lx = (float)wx1 - 6.0f, ly = (float)wy0 + 4.0f;
        for (y = wy0; y < wy1; y++)
            for (x = wx0; x < wx1; x++) {
                const float dx = (float)x - lx, dy = (float)y - ly, d = dx * dx + dy * dy;
                /* no frame: the glass fades in over its last few pixels, and it is only a little darker */
                const int32_t de = fm_minf(fm_minf((float)(x - wx0), (float)(wx1 - 1 - x)), fm_minf((float)(y - wy0), (float)(wy1 - 1 - y)));
                const float alpha = fm_clampf(((float)de + 0.5f) / 4.0f, 0.0f, 1.0f);
                if (px_dith4(x, y) + 0.5f >= alpha)
                    continue;
                px_fb[y * PX_W + x] = px_dith4(x + 1, y) + 0.5f < 0.3f ? PC_BG0 : PC_BG1;
                if (d < 30.0f && px_dith4(x, y) + 0.5f < 0.7f - d / 40.0f)
                    px_fb[y * PX_W + x] = PC_LAMP2;
            }
        px_pset((int32_t)lx, (int32_t)ly, PC_LAMP);
        px_pset((int32_t)lx + 1, (int32_t)ly, PC_LAMP);
        for (x = 0; x < 30; x++) {
            const int32_t rx = wx0 + (int32_t)(bs.rain_x[x] * (float)(wx1 - wx0));
            const int32_t ry = wy0 + (int32_t)(bs.rain_y[x] * (float)(wy1 - wy0));
            if ((float)x < 30.0f * (0.15f + 0.6f * pn(P_GUNK) + 0.25f * bs.hat) && rx > wx0 && rx < wx1 && ry > wy0 && ry + 1 < wy1) {
                px_pset(rx, ry, PC_RAIN2);
                px_pset(rx - 1, ry + 1, PC_RAIN);
            }
        }
    }
    draw_drips();
    if (voidness > 0.0f) {                               /* past a certain size the room isn't there */
        const float lvl = voidness;
        int i;
        for (y = 0; y < ab; y++)
            for (x = 0; x < PX_W; x++)
                if (px_dith4(x, y) + 0.5f < lvl)
                    px_fb[y * PX_W + x] = PC_BG0;
        for (i = 0; i < 28; i++) {
            const int32_t sx = (int32_t)((i * 37 + 11) % PX_W), sy = (int32_t)((i * 53 + 7) % (ab > 4 ? ab - 2 : 2));
            if (((int)(bs.t * 2.0f) + i) % 7)
                px_pset(sx, sy, i % 5 == 0 ? PC_AMBER : i % 3 ? PC_DIM : PC_WHITE);
        }
    }
    if (voidness < 0.5f && PX_W - x1 > 24) {             /* the neon sign, right wall; its N keeps going out */
        const int n_on = ((int)(bs.t * 7.0f) % 23) > 3 && ((int)(bs.t * 3.0f) % 11) != 0;
        const int32_t sx = PX_W - 22, sy = y0 > 10 ? y0 : 10;
        px_text(sx, sy, "OPE", PC_CORAL);
        px_text(sx + 15, sy, "N", n_on ? PC_CORAL : PC_BG1);
    }
}

/* the reverb's waves: ripples on the floor, out to the walls and back (warm on the way back) */
static void draw_waves(const geo_t *g)
{
    int i;
    for (i = 0; i < 6; i++) {
        const float rx = bs.wave[i].r, ry = bs.wave[i].r * 0.2f;
        float a;
        int k, n;
        if (!bs.wave[i].on)
            continue;
        a = bs.wave[i].a;
        n = (int)(rx * 5.0f) + 12;
        for (k = 0; k < n; k++) {
            const float t = (float)k / (float)n;
            const int32_t x = (int32_t)(g->cx + rx * fm_cosf(t * FM_TWO_PI)), y = (int32_t)(g->floor + ry * fm_sin_turns(t));
            if (y < (int32_t)g->ab && px_dith4(x, y) + 0.5f < a)
                px_pset(x, y, bs.wave[i].dir < 0.0f ? PC_CORAL : PC_RAIN2);
        }
    }
}

/* ------------------------------------------------------------ the blob --- */
#define NV 64
static float bvx[NV], bvy[NV];

static void body_shape(const geo_t *g, float squash_x, float squash_y, float r_scale, float lag)
{
    const int tbl = proj.par[P_TABLE];
    const float pos = pn(P_POS), sub = pn(P_SUB), live = 0.14f + 0.1f * sy_tel.amp_env;
    int k;
    for (k = 0; k < NV; k++) {
        float th = (float)k / (float)NV, sh, sc, r, x, y;
        sh = sy_shape(tbl, pos, th + lag);
        sc = sy_tel.scope[k & (SY_SCOPE - 1)];
        sc = sc > 1.0f ? 1.0f : sc < -1.0f ? -1.0f : sc;
        /* lumps that drift (it is never a clean ball) */
        const float lump = 0.07f * fm_sin_turns(2.0f * th + bs.t * 0.021f) + 0.05f * fm_sin_turns(3.0f * th + 0.37f - bs.t * 0.013f) +
                           0.1f * bs.wob * fm_sin_turns(4.0f * th + 0.2f);   /* (the LFO wobbles them) */
        r = g->r * r_scale * (0.84f + 0.3f * sh + live * sc + lump);
        x = fm_cosf(th * FM_TWO_PI) * r * squash_x;
        y = fm_sin_turns(th) * r * squash_y;
        if (y > 0.0f) {                                 /* it sags a little: a softer, rounder bottom (Sub: heavier) */
            const float k2 = y / (g->r + 1.0f);
            x *= 1.0f + (0.04f + 0.22f * sub) * k2 - 0.1f * k2 * k2;   /* (and narrower at the very bottom: no jaw) */
            y *= 1.0f - 0.08f * k2;
        }
        x += y * (0.12f * fm_sin_turns(bs.t * 0.017f) + bs.lean);   /* it leans, slowly, whichever way (and with the LFO) */
        bvx[k] = g->cx + x;
        bvy[k] = g->cy + y;
    }
}

/* the angle of (x, y), in turns (0..1) */
static float atan_turns(float y, float x)
{
    const float ax = fm_fabsf(x), ay = fm_fabsf(y), mx = fm_maxf(ax, ay), mn = fm_minf(ax, ay);
    float t, s, a;
    if (mx < 1e-6f)
        return 0.0f;
    t = mn / mx;
    s = t * t;
    a = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * t + t;
    if (ay > ax)
        a = 1.57079637f - a;
    if (x < 0.0f)
        a = 3.14159274f - a;
    if (y < 0.0f)
        a = -a;
    a *= 0.159154943f;
    return a < 0.0f ? a + 1.0f : a;
}

/* Light, for every blob-like thing: a soft volume lit from the window (up, left, toward you), the
 * floor bouncing a little blue back underneath, the lamp outside catching its right edge warm. Each
 * pixel's level picked through a 4 x 4 dither from a ramp: pixel art, but round. */
/* The light is wrong on purpose: low, from below, like a torch under a chin, and it moves. Each note
 * swings it round the blob (by pitch) and the envelope brightens it; a kick makes it flinch. Set per
 * frame by light_frame(). */
static float LX, LY, LZ, HX, HY, HZ, L_GAIN = 1.0f;
static void light_frame(void)
{
    const float a = bs.lang, el = 0.38f;                 /* its angle round the blob (turns); low */
    float hl;
    LX = fm_cosf(a * FM_TWO_PI) * (1.0f - el * el);
    LY = fm_sin_turns(a) * (1.0f - el * el);
    LZ = el;
    hl = fm_rsqrtf(LX * LX + LY * LY + (LZ + 1.0f) * (LZ + 1.0f));
    HX = LX * hl;
    HY = LY * hl;
    HZ = (LZ + 1.0f) * hl;
    L_GAIN = 0.75f + 0.35f * sy_tel.amp_env - 0.3f * bs.kick;
}
static const uint8_t RAMP_BLOB[6] = {PC_BLOBD, PC_BLOB0, PC_BLOBM, PC_BLOB1, PC_BLOB2, PC_BLOB3};
static const uint8_t RAMP_FRY[6] = {PC_BLACK, PC_EMBER, PC_EMBER, PC_FRY, PC_CRUST, PC_LAMP2};   /* burnt, not golden */
static const uint8_t RAMP_FOG[6] = {PC_BG1, PC_BLOBD, PC_BLOBD, PC_BLOB0, PC_BLOBM, PC_BLOB1};

typedef struct {
    float cx, cy;
    float rr[NV + 1];                                    /* the outline's radius by angle (wrapped) */
    float rmax;
    const uint8_t *ramp;
    float rim, fry, spec;                                /* the lamp's rim (0..1), fry mix, gleam */
    uint8_t outline;                                     /* the edge pixel's colour */
    float floor, refl;                                   /* the floor's line, and how much it reflects */
    float verb;                                          /* the walls' light, thrown back (the reverb) */
    int32_t ab;
} shade_t;

static void shade_px(const shade_t *sh, int32_t x, int32_t y)
{
    const float dx = (float)x + 0.5f - sh->cx, dy = (float)y + 0.5f - sh->cy;
    const float dist = fm_sqrtf(dx * dx + dy * dy), a = atan_turns(dy, dx) * (float)NV;
    const int k = (int)a;
    const float rr = sh->rr[k] + (sh->rr[k + 1] - sh->rr[k]) * (a - (float)k);
    float nx, ny, nz, d, lam, lv, sp;
    const float dith = px_dith4(x, y);
    uint8_t c;
    int idx;
    if (dist >= rr)
        return;
    if (dist > rr - 1.0f) {                              /* a dark edge, one pixel: pixel art */
        px_pset(x, y, sh->outline);
        return;
    }
    d = dist / rr;
    nz = fm_sqrtf(1.0f - d * d);
    nx = dx / rr;
    ny = dy / rr;
    lam = nx * LX + ny * LY + nz * LZ;
    lam = lam > 0.0f ? lam : 0.0f;
    lv = 0.08f + 0.8f * lam * L_GAIN + 0.12f * nz;       /* (a little from the room, head on) */
    if (sh->verb > 0.0f) {                              /* the reverb: cold light thrown back by the walls */
        const float side = (1.0f - nz) * (1.0f - nz) * sh->verb;
        lv += 0.5f * side;
    }
    idx = (int)(lv * 5.0f + dith + 0.5f);
    idx = idx < 0 ? 0 : idx > 5 ? 5 : idx;
    if (sh->fry > 0.0f) {                               /* fried: in burnt patches, not all over */
        const uint32_t hh = ((uint32_t)(x / 3) * 73856093u) ^ ((uint32_t)(y / 3) * 19349663u);
        const float patch = (float)((hh >> 9) & 255u) / 255.0f;
        c = patch < sh->fry * 1.1f + dith * 0.15f ? RAMP_FRY[idx] : sh->ramp[idx];
    } else {
        c = sh->ramp[idx];
    }
    sp = nx * HX + ny * HY + nz * HZ;                     /* the sheen: wide, dull, soft */
    sp = sp * sp;
    sp = sp * sp;
    sp = sp * sp;
    if (sp * sh->spec > 0.75f + dith * 0.5f)
        c = idx >= 4 ? PC_BLOB3 : sh->ramp[idx + 1 < 6 ? idx + 1 : 5];
    if (sh->rim > 0.0f && nx > 0.25f) {                  /* the lamp's rim, from the right */
        const float rim = (1.0f - nz) * (1.0f - nz) * nx * sh->rim * 3.0f;
        if (rim > 0.55f + dith * 0.5f)
            c = rim > 0.9f ? PC_AMBER : PC_ORANGE;
    }
    px_pset(x, y, c);
    /* its reflection in the wet floor: mirrored, darker, broken up */
    if (sh->refl > 0.0f) {
        const int32_t ry = (int32_t)(2.0f * sh->floor - (float)y);
        if (ry > (int32_t)sh->floor && ry < sh->ab && dith + 0.5f < sh->refl * (1.0f - (float)(ry - (int32_t)sh->floor) / 14.0f))
            px_pset(x, ry, idx >= 3 ? PC_BLOB0 : PC_BLOBD);
    }
}

static void shade_body(const shade_t *sh)
{
    int32_t x, y;
    const int32_t x0 = (int32_t)(sh->cx - sh->rmax - 1.0f), x1 = (int32_t)(sh->cx + sh->rmax + 1.0f);
    const int32_t y0 = (int32_t)(sh->cy - sh->rmax - 1.0f), y1 = (int32_t)(sh->cy + sh->rmax + 1.0f);
    for (y = y0 < 0 ? 0 : y0; y <= y1 && y < sh->ab; y++)
        for (x = x0 < 0 ? 0 : x0; x <= x1 && x < PX_W; x++)
            shade_px(sh, x, y);
}

/* a round thing (a copy, a buddy): the same light; fog: how far back it is (0 near .. 1 lost) */
static void shade_ball(const geo_t *g, float cx, float cy, float rx, float ry, float fog)
{
    shade_t sh;
    int k;
    memset(&sh, 0, sizeof sh);
    sh.cx = cx;
    sh.cy = cy;
    for (k = 0; k <= NV; k++) {
        const float th = (float)k / (float)NV, c = fm_cosf(th * FM_TWO_PI), s = fm_sin_turns(th);
        sh.rr[k] = rx * ry / fm_sqrtf(ry * ry * c * c + rx * rx * s * s + 1e-6f);
    }
    sh.rmax = rx > ry ? rx : ry;
    sh.ramp = fog > 0.4f ? RAMP_FOG : RAMP_BLOB;
    sh.spec = 1.0f - fog;
    sh.outline = fog > 0.4f ? PC_BG0 : PC_BLOBD;
    sh.ab = (int32_t)g->ab;
    shade_body(&sh);
}

static void draw_shadow(const geo_t *g)
{
    const float lift = g->floor - (g->cy + g->r), w = g->r * (0.9f - fm_clampf(lift, 0.0f, 20.0f) * 0.02f);
    int32_t x, y;
    for (y = (int32_t)(g->floor - 5.0f); y <= (int32_t)(g->floor + 5.0f); y++)   /* (seen from above: rounder) */
        for (x = (int32_t)(g->cx - w); x <= (int32_t)(g->cx + w); x++) {
            const float dx = ((float)x - g->cx) / w, dy = ((float)y - g->floor) / 5.0f, d = dx * dx + dy * dy;
            if (d < 1.0f && px_dith4(x, y) + 0.5f < (1.0f - d) * 1.4f)
                px_pset(x, y, PC_BG0);
        }
}

/* Chorus: copies of itself, see-through, peeking out from behind it, a beat out of step. One a side,
 * then more, further out and fainter. */
static void draw_copies(const geo_t *g, float sqx, float sqy)
{
    const int n = (int)(pn(P_CHORUS) * 6.0f + 0.4f);
    int i, k;
    for (i = n - 1; i >= 0; i--) {                      /* the far ones first */
        const int side = i & 1 ? 1 : -1, layer = i / 2;
        geo_t gg = *g;
        const float sway = 2.0f * fm_sin_turns(bs.t * (0.31f + 0.07f * (float)i) + (float)i * 0.37f);
        gg.cx = g->cx + (float)side * g->r * (0.42f + 0.3f * (float)layer) + sway;
        gg.cy = g->cy - (float)layer * 1.5f + 1.2f * fm_sin_turns(bs.t * 0.4f + (float)i * 0.3f);
        gg.r = g->r * (0.92f - 0.08f * (float)layer);
        body_shape(&gg, sqx, sqy, 1.0f, 0.05f * (float)(i + 1));   /* (a beat late: its own lag) */
        px_poly(bvx, bvy, NV, layer == 0 ? PC_BLOB0 : PC_BLOBD, layer == 0 ? 2 : 1);   /* see-through: half there */
        for (k = 0; k < NV; k += 2 + layer)
            px_pset((int32_t)bvx[k], (int32_t)bvy[k], layer == 0 ? PC_BLOBM : PC_BLOB0);
    }
}

/* the voices: one light each over its head; lit while a note is held, glowing while it rings out */
static void draw_voices(const geo_t *g)
{
    const int32_t w = SY_NVOICE * 5 - 2, x0 = (int32_t)g->cx - w / 2;
    int32_t y = (int32_t)(g->cy - g->r - 9.0f);
    int i;
    if (y < 3)
        y = 3;
    if (ui.arp) {                                        /* the arpeggiator: a staircase, its step lit */
        const int st = (int)(sy_tel.arp_seq % 4u);
        for (i = 0; i < 4; i++)
            px_rect(x0 + w + 4 + i * 3, y + 3 - i, 2, 1 + i, i == st ? PC_SPEC : PC_BLOB0);
    }
    for (i = 0; i < SY_NVOICE; i++) {
        const uint8_t c = i < sy_tel.gates ? PC_AMBER : i < sy_tel.voices ? PC_EMBER : PC_GRID;
        const int32_t x = x0 + i * 5;
        px_rect(x, y, 3, 3, c);
        if (i < sy_tel.gates)
            px_pset(x + 1, y + 1, PC_WHITE);
    }
}

static void draw_body(const geo_t *g, float sqx, float sqy)
{
    const float det = pn(P_DETUNE), fry = pn(P_FRY);
    const float vol = pn(P_VOLUME) * (float)plat_master() / 4096.0f;
    shade_t sh;
    int k;
    if (det > 0.02f) {                                   /* Detune: a ghost of it, drifting apart, beating */
        const float off = det * 6.0f * fm_sin_turns(bs.t * (0.2f + det * 1.5f));
        geo_t gg = *g;
        gg.cx += off;
        body_shape(&gg, sqx, sqy, 1.0f, 0.03f * det);
        for (k = 0; k < NV; k += 2)
            px_pset((int32_t)bvx[k], (int32_t)bvy[k], PC_GHOST);
        gg.cx = g->cx - off;
        body_shape(&gg, sqx, sqy, 1.0f, -0.03f * det);
        for (k = 1; k < NV; k += 2)
            px_pset((int32_t)bvx[k], (int32_t)bvy[k], PC_GHOST);
    }
    body_shape(g, sqx, sqy, 1.0f, 0.0f);
    memset(&sh, 0, sizeof sh);
    sh.cx = g->cx;
    sh.cy = g->cy;
    sh.rmax = 0.0f;
    for (k = 0; k < NV; k++) {
        const float dx = bvx[k] - g->cx, dy = bvy[k] - g->cy;
        sh.rr[k] = fm_sqrtf(dx * dx + dy * dy);
        sh.rmax = sh.rr[k] > sh.rmax ? sh.rr[k] : sh.rmax;
    }
    sh.rr[NV] = sh.rr[0];
    sh.ramp = RAMP_BLOB;
    sh.fry = fry < 0.2f ? 0.0f : (fry - 0.2f) * 1.6f;
    sh.spec = (fry > 0.6f ? 0.4f : 0.8f) * fm_clampf(2.4f - bs.zoom, 0.0f, 1.0f);   /* (none in a close-up) */
    sh.verb = pn(P_REVERB) * sy_tel.verb * 1.5f;
    sh.rim = 0.15f + 0.35f * vol + 0.6f * sy_tel.amp_env * vol;   /* louder: the lamp catches more of it */
    sh.outline = PC_BLOBD;
    sh.floor = g->floor;
    sh.refl = 0.45f;
    sh.ab = (int32_t)g->ab;
    shade_body(&sh);
    if (fry > 0.55f) {                                   /* deep-fried: crust, blown-out pixels */
        const int n = (int)((fry - 0.55f) * 160.0f);
        int i;
        for (i = 0; i < n; i++) {
            const int32_t x = (int32_t)(g->cx + b_rs() * g->r * 0.8f), y = (int32_t)(g->cy + b_rs() * g->r * 0.8f);
            const uint8_t c = px_get(x, y);
            if (c != PC_BG0 && c != PC_BG1 && c != PC_BG2 && c != PC_BLOBD)
                px_pset(x, y, b_rnd() & 1u ? PC_AMBER : b_rnd() & 1u ? PC_RED : PC_WHITE);
        }
    }
}

static void draw_face(const geo_t *g)
{
    const float s = g->r / 26.0f, cut = pn(P_CUTOFF), reso = pn(P_RESO);
    const float cx = g->cx, ey = g->cy - 6.0f * s + g->face_dy;   /* (from above: the face sits a little lower) */
    const int fine = bs.fire > 0.3f;
    /* the filter opens and shuts them all the way (and a little more than linearly, so the middle reads) */
    float open = cut < 0.04f ? 0.0f : 0.08f + 0.92f * fm_sqrtf(cut), brow = sy_tel.mod_env * pn(P_ENVAMT) * 7.0f * s + bs.shrug * 4.0f;
    float lookx = 0.0f, looky = 1.0f;                    /* sad: looking down */
    int e;
    if (sy_tel.last_note >= 0 && sy_tel.voices)
        lookx = fm_clampf((float)(sy_tel.last_note - 60) / 18.0f, -1.0f, 1.0f);
    if (bs.stare > 0.5f) {                               /* it looks at you */
        lookx = 0.0f;
        looky = 0.0f;
    }
    open *= 1.0f - bs.blink;
    open *= 1.0f - bs.sleep;
    if (fine)
        open = 0.35f;
    for (e = -1; e <= 1; e += 2) {
        const float ex = cx + (float)e * 10.5f * s, rw = 5.8f * s, rh = 6.4f * s * open;
        if (rh < 0.9f) {                                 /* shut: a droopy line */
            px_line((int32_t)(ex - rw), (int32_t)ey, (int32_t)(ex + rw), (int32_t)(ey + (float)e * 1.5f * s), PC_PUPIL);
            px_line((int32_t)(ex - rw), (int32_t)ey + 1, (int32_t)(ex + rw), (int32_t)(ey + (float)e * 1.5f * s) + 1, PC_BLOBD);
        } else {
            float pr = (3.4f - 2.8f * reso) * s;          /* resonance: the pupils shrink to pinpoints */
            uint8_t pc = reso > 0.6f ? PC_RED : PC_PUPIL;
            px_ellipse(ex, ey, rw + 0.8f, rh + 0.8f, PC_BLOBD);   /* the socket */
            px_ellipse(ex, ey, rw, rh, PC_EYE);
            px_ellipse(ex, ey - rh * 0.55f, rw * 0.9f, rh * 0.45f, PC_EYE2);   /* the lid's shadow on it */
            if (pr < 0.6f)
                pr = 0.6f;
            px_ellipse(ex + lookx * 2.0f * s, ey + looky * (rh - pr) * 0.6f, pr, pr, pc);
            if (pr > 1.2f && reso <= 0.6f)                /* a catchlight: the window, in its eye */
                px_pset((int32_t)(ex + lookx * 2.0f * s + LX * pr * 0.5f), (int32_t)(ey + looky * (rh - pr) * 0.6f + LY * pr * 0.5f), PC_SPEC);
            if (reso > 0.6f && ((int)(bs.t * 8.0f) & 1))
                px_pset((int32_t)(ex + lookx * 2.0f * s), (int32_t)(ey + looky * (rh - pr) * 0.6f), PC_AMBER);
            {   /* the lids: heavy, sloping down at the outer corners (stoned, sad) */
                const float lid = ey - rh + (1.0f - open) * rh * 0.9f;
                int32_t x;
                for (x = (int32_t)(ex - rw - 1.0f); x <= (int32_t)(ex + rw + 1.0f); x++) {
                    float slope = ((float)x - ex) * (float)e * 0.35f, ly = lid + slope;
                    int32_t y;
                    for (y = (int32_t)(ey - rh - 1.0f); y < (int32_t)ly; y++) {
                        const uint8_t c = px_get(x, y);
                        if (c == PC_EYE || c == PC_EYE2 || c == PC_PUPIL || c == PC_RED || c == PC_AMBER || c == PC_SPEC)
                            px_pset(x, y, PC_BLOB0);
                    }
                    if (px_get(x, (int32_t)ly) == PC_EYE || px_get(x, (int32_t)ly) == PC_EYE2)
                        px_pset(x, (int32_t)ly, PC_BLOBD);   /* the lid's edge */
                }
            }
        }
        {   /* the brows: inner ends up (sad); the mod envelope lifts them */
            const float by = ey - 8.0f * s - brow;
            const uint8_t bc = fine ? PC_BLOB0 : PC_PUPIL;
            px_line((int32_t)(ex - (float)e * 5.5f * s), (int32_t)(by - 3.0f * s), (int32_t)(ex + (float)e * 4.5f * s),
                    (int32_t)(by + 0.5f * s), bc);
            px_line((int32_t)(ex - (float)e * 5.5f * s), (int32_t)(by - 3.0f * s) + 1, (int32_t)(ex + (float)e * 4.5f * s),
                    (int32_t)(by + 0.5f * s) + 1, bc);   /* (thick: it reads from across the room) */
        }
        if (reso > 0.85f && open > 0.2f && !fine) {     /* lasers */
            const int32_t x0 = (int32_t)(ex + lookx * 1.5f * s), y0 = (int32_t)ey;
            const int32_t x1 = (int32_t)(ex + (float)e * 70.0f), y1 = (int32_t)(ey + 60.0f);
            px_line(x0, y0, x1, y1, PC_RED);
            px_line(x0, y0 + 1, x1, y1 + 1, PC_RED);
            if ((int)(bs.t * 20.0f) & 1)
                px_line(x0, y0, x1, y1, PC_AMBER);
        }
    }
    {   /* the mouth: the amp envelope */
        const float a = sy_tel.amp_env, my = g->cy + 10.0f * s + g->face_dy * 0.5f;
        const int yawn = !sy_tel.gates && a > 0.05f && pn(P_RELEASE) > 0.6f;
        if (fine) {                                       /* a small calm smile */
            px_line((int32_t)(cx - 4.5f * s), (int32_t)my, (int32_t)(cx + 4.5f * s), (int32_t)my, PC_PUPIL);
            px_pset((int32_t)(cx - 4.5f * s) - 1, (int32_t)my - 1, PC_PUPIL);
            px_pset((int32_t)(cx + 4.5f * s) + 1, (int32_t)my - 1, PC_PUPIL);
        } else if (a < 0.06f) {                          /* shut: meh. Flat, a little crooked */
            const float w = 5.5f * s;
            int t;
            for (t = 0; t < 2; t++) {                    /* (two pixels thick) */
                px_line((int32_t)(cx - w), (int32_t)(my + 0.8f * s) + t, (int32_t)(cx + w * 0.5f), (int32_t)my + t, PC_PUPIL);
                px_line((int32_t)(cx + w * 0.5f), (int32_t)my + t, (int32_t)(cx + w), (int32_t)(my + 1.2f * s) + t, PC_PUPIL);
            }
        } else {
            /* open: wide with the attack, held at the sustain; a long release is a tall yawn */
            const float rx = yawn ? (3.0f + 2.0f * a) * s : (4.0f + 5.0f * a) * s, ry = (1.5f + (yawn ? 10.0f : 8.0f) * a) * s;
            px_ellipse(cx, my + ry * 0.4f, rx + 0.8f, ry + 0.8f, PC_BLOBD);   /* the lips */
            px_ellipse(cx, my + ry * 0.4f, rx, ry, PC_MOUTH);
            if (a > 0.3f)
                px_ellipse(cx, my + ry * 1.0f, rx * 0.65f, ry * 0.38f, PC_TONGUE);
        }
    }
    /* the LFO, deep: little motion marks either side (it's vibing, sadly) */
    if (pn(P_LFODEPTH) * bs.vamt > 0.3f && !bs.sleep) {
        const float ph = sy_tel.lfo_phase, side = sy_tel.lfo > 0.0f ? 1.0f : -1.0f;
        const int32_t mx = (int32_t)(cx - side * (g->r + 4.0f)), my = (int32_t)(g->cy - 2.0f);
        if (fm_fabsf(sy_tel.lfo) > 0.5f) {
            px_line(mx, my - 2, mx - (int32_t)(side * 3.0f), my - 3, PC_ORANGE);
            px_line(mx, my + 1, mx - (int32_t)(side * 4.0f), my + 1, PC_ORANGE);
            px_line(mx, my + 4, mx - (int32_t)(side * 3.0f), my + 5, PC_ORANGE);
        }
        (void)ph;
    }
}

/* what the levels give it: nothing useful */
static void draw_unlocks(const geo_t *g)
{
    const uint32_t lv = xp_level(proj.xp);
    if (lv >= 10u) {                                    /* a balloon, mostly air gone */
        const float bx = g->cx + g->r * 1.05f, by = g->cy - g->r * 0.2f + 1.5f * fm_sin_turns(bs.t * 0.3f);
        px_line((int32_t)(g->cx + g->r * 0.6f), (int32_t)(g->cy + 2.0f), (int32_t)bx, (int32_t)(by + 4.0f), PC_DIM);
        px_ellipse(bx, by, 3.0f, 3.8f, PC_RED);
        px_pset((int32_t)bx - 1, (int32_t)by - 2, PC_CORAL);
    }
}

/* The drums: a cassette, smoked see-through plastic, up in the corner. Its label has the style on it,
 * rolling past; through its window, the tape winding from one spool to the other at the tempo. */
static void draw_cassette(const geo_t *g)
{
    const int on = sy_tel.drums_on;
    const float slide = bs.tape_anim;
    const int32_t x = 2, y = 2 - (int32_t)(slide * 24.0f);   /* a new one drops in from the top */
    const char *name = sy_style_name(bs.tape_shown >= 0 ? bs.tape_shown : proj.par[P_STYLE]);
    int32_t i, j;
    (void)g;
    if (!tape_up())
        return;
    /* the shell: dark smoked plastic, mostly there (what's behind shows through, darkened) */
    for (j = 0; j < 19; j++)
        for (i = 0; i < 34; i++) {
            const int32_t px = x + i, py = y + j;
            const int edge = i == 0 || j == 0 || i == 33 || j == 18;
            if (py < 0)
                continue;
            if (edge)
                px_pset(px, py, (i == 0 || j == 0) ? PC_SMOKE2 : PC_BLACK);   /* lit from the top left */
            else if (px_dith(px, py, 3))
                px_pset(px, py, PC_SMOKE);
            else
                px_pset(px, py, GLASS[px_get(px, py) < PC_N ? px_get(px, py) : 0]);
        }
    /* the label: off-white paper, a red stripe, the name rolling past */
    px_rect(x + 3, y + 2, 28, 7, PC_EYE);
    px_rect(x + 3, y + 8, 28, 1, sy_tel.rec ? PC_RED : on ? PC_TEAR : PC_DIM);
    {
        const int32_t len = px_text_w(name), win = 26;
        const int32_t off = len > win ? ((int32_t)(bs.t * 9.0f) % (len + 14)) - 7 : -(win - len) / 2;
        const int32_t cx = x + 4 - off;
        int k;
        for (k = 0; name[k]; k++) {                       /* clipped to the label: glyph by glyph */
            const int32_t gx = cx + k * 5;
            char one[2] = {name[k], 0};
            if (gx >= x + 3 && gx + 4 <= x + 31)
                px_text(gx, y + 2, one, PC_BLACK);
        }
    }
    /* the window: the tape on its spools (one fills as the other empties), the hubs turning */
    {
        const float prog = fm_clampf(bs.reel * 0.02f - fm_floorf(bs.reel * 0.02f), 0.0f, 1.0f);
        const float rl = 2.0f + 2.5f * (1.0f - prog), rr = 2.0f + 2.5f * prog;
        int k;
        px_rect(x + 6, y + 10, 22, 7, PC_BLACK);
        px_ellipse((float)(x + 11), (float)(y + 13) + 0.5f, rl, fm_minf(rl, 3.4f), PC_SPOOL);
        px_ellipse((float)(x + 23), (float)(y + 13) + 0.5f, rr, fm_minf(rr, 3.4f), PC_SPOOL);
        px_line(x + 11, y + 16, x + 23, y + 16, PC_SPOOL);    /* the tape running between them, below */
        for (k = 0; k < 2; k++) {                           /* the hubs: white teeth, turning */
            const float hx = (float)(x + 11 + k * 12), hy = (float)(y + 13), a = bs.reel + (float)k * 0.13f;
            px_pset((int32_t)hx, (int32_t)hy, PC_SMOKE);
            px_pset((int32_t)(hx + 1.6f * fm_cosf(a * FM_TWO_PI) + 0.5f), (int32_t)(hy + 1.6f * fm_sin_turns(a) + 0.5f), PC_EYE);
            px_pset((int32_t)(hx - 1.6f * fm_cosf(a * FM_TWO_PI) + 0.5f), (int32_t)(hy - 1.6f * fm_sin_turns(a) + 0.5f), PC_EYE);
        }
        px_rect(x + 6, y + 10, 22, 1, PC_SMOKE2);           /* the window's top edge, a glint */
    }
    if (sy_tel.rec == 2 && ((int)(bs.t * 3.0f) & 1))       /* recording: a red light on the shell */
        px_rect(x + 29, y + 11, 3, 3, PC_RED);
    if (sy_tel.rec == 1) {                                   /* counting in: four lights, one a beat */
        int k;
        for (k = 0; k < 4; k++)
            px_rect(x + 8 + k * 5, y + 4, 3, 3, k <= sy_tel.count ? PC_RED : PC_DIM);
    }
    px_pset(x + 2, y + 16, PC_SMOKE2);                      /* screws */
    px_pset(x + 31, y + 16, PC_SMOKE2);
    if (on && proj.par[P_BASS] > 0) {                       /* the auto-bass: a speaker, its cone thumping */
        const float c = 2.0f + 1.5f * bs.bass;
        px_rect(x + 36, y + 3, 11, 14, PC_SMOKE);
        px_rect(x + 36, y + 3, 11, 1, PC_SMOKE2);
        px_ellipse((float)(x + 41) + 0.5f, (float)(y + 10), c, c, bs.bass > 0.5f ? PC_ORANGE : PC_BLACK);
        px_pset(x + 41, y + 10, PC_SMOKE2);
    }
}

static void draw_particles(void)
{
    int i;
    for (i = 0; i < 48; i++) {
        int32_t x, y;
        if (!bs.p[i].on)
            continue;
        x = (int32_t)bs.p[i].x;
        y = (int32_t)bs.p[i].y;
        switch (bs.p[i].kind) {
        case PK_SPLASH: px_pset(x, y, PC_TEAR2); break;
        case PK_FIRE:
            px_pset(x, y, bs.p[i].life > 0.4f ? PC_AMBER : PC_RED);
            px_pset(x, y - 1, bs.p[i].life > 0.6f ? PC_WHITE : PC_ORANGE);
            px_pset(x + 1, y, PC_RED);
            break;
        case PK_ZZZ: px_text(x, y, "Z", bs.p[i].life > 0.5f ? PC_DIM : PC_RAIN); break;
        case PK_STEAM:
            if (px_dith(x, y, (int)(bs.p[i].life * 4.0f)))
                px_pset(x, y, PC_DIM);
            px_pset(x + 1, y - 1, PC_DIM);
            break;
        case PK_YEET:                                    /* a voice's light, stolen, flying off */
            px_rect(x - 1, y - 1, 3, 3, PC_RED);
            px_pset(x, y, PC_AMBER);
            px_pset(x - (int32_t)(bs.p[i].vx * 0.06f), y - (int32_t)(bs.p[i].vy * 0.06f), PC_EMBER);   /* its trail */
            px_pset(x - (int32_t)(bs.p[i].vx * 0.1f), y - (int32_t)(bs.p[i].vy * 0.1f), PC_EMBER);
            break;
        case PK_PUFF:
            px_ellipse(bs.p[i].x, bs.p[i].y, 2.0f + (1.0f - bs.p[i].life) * 3.0f, 1.5f + (1.0f - bs.p[i].life) * 2.0f,
                       bs.p[i].life > 0.5f ? PC_DIM : PC_RAIN);
            break;
        case PK_SWEAT:
            px_pset(x, y, PC_TEAR2);
            px_pset(x, y + 1, PC_TEAR);
            break;
        default: px_pset(x, y, PC_AMBER); break;
        }
    }
}

static void draw_tears(const geo_t *g)
{
    int i;
    for (i = 0; i < 24; i++) {
        if (!bs.tear[i].on)
            continue;
        px_ellipse(bs.tear[i].x, bs.tear[i].y, bs.tear[i].s * 0.7f, bs.tear[i].s, PC_TEAR);
        if (bs.tear[i].s > 1.2f)
            px_pset((int32_t)(bs.tear[i].x - 0.5f), (int32_t)(bs.tear[i].y - bs.tear[i].s * 0.4f), PC_TEAR2);
    }
    if (bs.puddle > 0.02f) {                             /* where the tears end up */
        const float w = 6.0f + bs.puddle * 30.0f;
        int32_t x, y;
        for (y = (int32_t)g->floor - 1; y < (int32_t)g->floor + 3; y++)
            for (x = (int32_t)(g->cx - w); x < (int32_t)(g->cx + w); x++) {
                float dx = ((float)x - g->cx) / w, dy = ((float)y - g->floor - 0.5f) / 2.0f;
                if (dx * dx + dy * dy < 1.0f && px_dith(x, y, 2 + (int)(bs.puddle * 2.0f)))
                    px_pset(x, y, ((x + (int32_t)(bs.t * 4.0f)) % 9) ? PC_TEAR : PC_TEAR2);
            }
    }
}

/* ------------------------------------------------------------ the schematic --- */
/* The panel under the room: the page's name, its drawing, and the four knobs' names over their meters.
 * All in the room's cold colours, like the drum grid: blob-blue lines, tear-blue and violet for the
 * second things, a pale glint for where it is now. */
#define PANEL_H 44
typedef struct {
    int32_t y0;                                         /* the panel's top */
    int32_t gx0, gx1, gy0, gy1;                         /* the drawing's box */
} drawer_t;

static int32_t gy_of(const drawer_t *p, float v)          /* 0..1 up the box */
{
    return p->gy1 - (int32_t)(fm_clampf(v, 0.0f, 1.0f) * (float)(p->gy1 - p->gy0) + 0.5f);
}

static void grid(const drawer_t *p)
{
    px_line(p->gx0, p->gy1, p->gx1, p->gy1, PC_GRID);
}

/* a curve through n points (0..1 across, 0..1 up), its area dithered warm underneath */
static void curve(const drawer_t *p, const float *v, int n, uint8_t line, uint8_t fill, int fill_lvl)
{
    int i;
    int32_t px0 = p->gx0, py0 = gy_of(p, v[0]);
    for (i = 1; i < n; i++) {
        const int32_t x = p->gx0 + (int32_t)((float)i / (float)(n - 1) * (float)(p->gx1 - p->gx0)), y = gy_of(p, v[i]);
        if (fill_lvl) {
            int32_t xx, yy;
            for (xx = px0; xx <= x; xx++)
                for (yy = y + 1; yy < p->gy1; yy++)
                    if (px_dith(xx, yy, fill_lvl))
                        px_pset(xx, yy, fill);
        }
        px_line(px0, py0, x, y, line);
        px0 = x;
        py0 = y;
    }
}

static float tri_wave(float ph) { return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; }
static float lfo_shape_at(int shape, float ph)
{
    ph -= fm_floorf(ph);
    switch (shape) {
    case LFO_TRI: return tri_wave(ph + 0.25f - fm_floorf(ph + 0.25f));
    case LFO_SQUARE: return ph < 0.5f ? 1.0f : -1.0f;
    case LFO_SAW: return 1.0f - 2.0f * ph;
    case LFO_RANDOM: {
        const uint32_t k = (uint32_t)(ph * 4.0f) * 2654435761u;
        return (float)((k >> 8) & 255u) / 127.5f - 1.0f;
    }
    default: return fm_sin_turns(ph);
    }
}

static void schem_body(const drawer_t *p)                /* the wavetable: its frames, stacked */
{
    const int t = proj.par[P_TABLE];
    const float pos = pn(P_POS), det = pn(P_DETUNE);
    float v[56];
    int f, i;
    for (f = 4; f >= 1; f--) {                           /* the frames behind, up and right */
        const float fp = fm_clampf(pos + (float)f * 0.18f, 0.0f, 1.0f);
        const int32_t dx = f * 3, dy = -f * 2;
        int32_t lx = 0, ly = 0;
        for (i = 0; i < 40; i++) {
            const int32_t x = p->gx0 + dx + i * 2, y = (p->gy0 + p->gy1) / 2 + dy - (int32_t)(sy_shape(t, fp, (float)i / 40.0f) * 7.0f);
            if (i)
                px_line(lx, ly, x, y, f == 1 ? PC_WALL2 : PC_WALL);
            lx = x;
            ly = y;
        }
    }
    for (i = 0; i < 56; i++)                             /* the frame playing, in front */
        v[i] = 0.5f + 0.45f * sy_shape(t, pos, (float)i / 55.0f);
    {
        drawer_t q = *p;
        q.gx1 = p->gx0 + 78;
        curve(&q, v, 56, PC_BLOB2, PC_BLOBD, 1);
        if (det > 0.05f) {                               /* the detuned one, sliding */
            for (i = 0; i < 56; i++)
                v[i] = 0.5f + 0.45f * sy_shape(t, pos, (float)i / 55.0f + det * 0.1f * fm_sin_turns(bs.t * 0.5f));
            curve(&q, v, 56, PC_GHOST, 0, 0);
        }
    }
    /* the position, a slider of frames on the right */
    for (f = 0; f < SY_NFRAME; f++) {
        const int32_t y = p->gy1 - f * (p->gy1 - p->gy0) / (SY_NFRAME - 1);
        const int on = (int)(pos * (float)(SY_NFRAME - 1) + 0.5f) == f;
        px_rect(p->gx1 - 12, y, on ? 10 : 6, 1, on ? PC_SPEC : PC_WALL2);
    }
}

static void schem_eyes(const drawer_t *p)                /* the filter: its curve, and where the envelope takes it */
{
    const float cut = pn(P_CUTOFF), k = 2.0f - 1.94f * pn(P_RESO), amt = pn(P_ENVAMT);
    float v[56], v2[56];
    int i, live;
    for (live = 0; live < 2; live++) {
        const float oct = cut * 9.3f + (live ? amt * 6.0f * sy_tel.mod_env : 0.0f);
        for (i = 0; i < 56; i++) {
            /* across: 20 Hz .. 20 kHz on octaves; up: -36 .. +18 dB */
            const float fo = (float)i / 55.0f * 10.0f - (oct + 0.6f), w = fm_exp2f(fo), w2 = w * w;
            const float mag2 = 1.0f / ((1.0f - w2) * (1.0f - w2) + w2 * k * k);
            const float db = 10.0f * fm_log2f(mag2) * 0.30103f;
            (live ? v2 : v)[i] = (db + 36.0f) / 54.0f;
        }
    }
    curve(p, v, 56, PC_BLOB2, PC_BLOBD, 1);
    if (amt > 0.02f && sy_tel.mod_env > 0.02f)
        curve(p, v2, 56, PC_SPEC, 0, 0);                  /* where it is now, opened by the note */
    {   /* the cutoff, a marker */
        const int32_t x = p->gx0 + (int32_t)(fm_clampf((cut * 9.3f + 0.6f) / 10.0f, 0.0f, 1.0f) * (float)(p->gx1 - p->gx0));
        int32_t y;
        for (y = p->gy0; y < p->gy1; y += 2)
            px_pset(x, y, PC_TEAR);
    }
}

static void schem_mouth(const drawer_t *p)               /* the envelope: attack, decay, sustain, release */
{
    const float a = 0.05f + 0.25f * pn(P_ATTACK), d = 0.05f + 0.25f * pn(P_DECAY), r = 0.05f + 0.3f * pn(P_RELEASE);
    const float sus = pn(P_SUSTAIN), hold = 1.0f - a - d - r, w = (float)(p->gx1 - p->gx0);
    const int32_t xa = p->gx0 + (int32_t)(a * w), xd = xa + (int32_t)(d * w), xs = xd + (int32_t)(hold * w), xr = p->gx1;
    const int32_t yb = p->gy1, yt = p->gy0, ys = gy_of(p, sus);
    int32_t x, y;
    /* the shape, filled warm */
    for (x = p->gx0; x <= xr; x++) {
        float lv;
        if (x < xa)
            lv = (float)(x - p->gx0) / (float)(xa - p->gx0 + 1);
        else if (x < xd)
            lv = 1.0f - (1.0f - sus) * (float)(x - xa) / (float)(xd - xa + 1);
        else if (x < xs)
            lv = sus;
        else
            lv = sus * (1.0f - (float)(x - xs) / (float)(xr - xs + 1));
        for (y = gy_of(p, lv) + 1; y < yb; y++)
            if (px_dith(x, y, 1))
                px_pset(x, y, PC_BLOBD);
    }
    px_line(p->gx0, yb, xa, yt, PC_BLOB2);
    px_line(xa, yt, xd, ys, PC_BLOB2);
    px_line(xd, ys, xs, ys, PC_BLOB2);
    px_line(xs, ys, xr, yb, PC_BLOB2);
    for (y = yt; y < yb; y += 3) {                       /* the stages' borders */
        px_pset(xa, y, PC_WALL2);
        px_pset(xd, y, PC_WALL2);
        px_pset(xs, y, PC_WALL2);
    }
    {   /* where the loudest note is in it now: a red dot */
        const float e = sy_tel.amp_env;
        int32_t dx = -1;
        switch (sy_tel.amp_stage) {
        case 1: dx = p->gx0 + (int32_t)(e * (float)(xa - p->gx0)); break;
        case 2: dx = xa + (int32_t)(fm_clampf((1.0f - e) / (1.0f - sus + 1e-3f), 0.0f, 1.0f) * (float)(xd - xa)); break;
        case 3: dx = (xd + xs) / 2; break;
        case 4: dx = xs + (int32_t)(fm_clampf(1.0f - e / (sus + 1e-3f), 0.0f, 1.0f) * (float)(xr - xs)); break;
        default: break;
        }
        if (dx >= 0) {
            const int32_t dy = gy_of(p, e);
            for (y = dy; y < yb; y += 2)
                px_pset(dx, y, PC_SPEC);
            px_rect(dx - 1, dy - 1, 3, 3, PC_SPEC);
            px_pset(dx, dy, PC_BLOB2);
        }
    }
}

static void schem_vibe(const drawer_t *p)                /* the LFO: its wave, its speed, where it is */
{
    static const char *const DEST[LD_N] = {"SHAPE", "EYES", "PITCH", "VOL"};
    const int shape = proj.par[P_LFOSHAPE];
    const float cycles = 1.0f + 3.0f * pn(P_LFORATE), depth = 0.15f + 0.85f * pn(P_LFODEPTH);
    float v[64];
    int i;
    for (i = 0; i < 64; i++)
        v[i] = 0.5f + 0.48f * depth * lfo_shape_at(shape, (float)i / 63.0f * cycles);
    {
        drawer_t q = *p;
        q.gx1 = p->gx1 - 26;
        px_line(q.gx0, (q.gy0 + q.gy1) / 2, q.gx1, (q.gy0 + q.gy1) / 2, PC_WALL);
        curve(&q, v, 64, PC_BLOB2, PC_BLOBD, 0);
        {   /* the dot riding the first cycle */
            const float ph = sy_tel.lfo_phase;
            const int32_t x = q.gx0 + (int32_t)(ph / cycles * (float)(q.gx1 - q.gx0));
            const int32_t y = gy_of(&q, 0.5f + 0.48f * depth * lfo_shape_at(shape, ph));
            px_rect(x - 1, y - 1, 3, 3, PC_SPEC);
        }
    }
    /* where it goes */
    px_text(p->gx1 - 22, p->gy0 + 2, "TO", PC_DIM);
    px_text(p->gx1 - 22, p->gy0 + 9, DEST[proj.par[P_LFODEST]], PC_TEAR);
}

static void schem_tears(const drawer_t *p)               /* the delay: the taps, left, right, left, fading */
{
    const float fb = pn(P_FEEDBACK) * 0.95f, dly = pn(P_DELAY), time = 0.15f + 0.85f * pn(P_TIME);
    const float step = time * 22.0f, h = (float)(p->gy1 - p->gy0);
    float lv = 1.0f, x = (float)p->gx0 + 2.0f;
    int k;
    px_rect(p->gx0 + 1, p->gy1 - (int32_t)h, 2, (int32_t)h, PC_WHITE);   /* the note itself */
    for (k = 0; k < 12 && x + step < (float)p->gx1; k++) {
        const int32_t bh = (int32_t)(h * lv * (0.3f + 0.7f * dly) + 0.5f);
        x += step;
        lv *= k ? fb : 1.0f;
        if (bh < 1 || dly <= 0.0f)
            break;
        px_rect((int32_t)x, p->gy1 - bh, 2, bh, k & 1 ? PC_GHOST : PC_TEAR);   /* ping, pong */
        if (k == 0 && bs.echo_flash > 0.3f)
            px_rect((int32_t)x - 1, p->gy1 - bh - 1, 4, 1, PC_BLOB2);
    }
    {   /* the chorus: a wobbling ribbon over the top, wider and busier with more */
        const float c = pn(P_CHORUS);
        int32_t xx;
        if (c > 0.02f)
            for (xx = p->gx0; xx < p->gx1; xx++)
                px_pset(xx, p->gy0 + 2 + (int32_t)(c * 2.5f * fm_sin_turns((float)xx * 0.08f + bs.t * 0.7f) +
                                                    c * 1.5f * fm_sin_turns((float)xx * 0.13f - bs.t * 0.5f)), PC_GHOST);
    }
}

static void schem_walls(const drawer_t *p)               /* the room: its tail; the fry: its curve */
{
    const float rv = pn(P_REVERB), size = pn(P_SIZE), fry = pn(P_FRY), tape = pn(P_TAPE);
    const int32_t split = p->gx1 - 26;
    const float len = 0.15f + 0.85f * size;
    int32_t x, y;
    for (x = p->gx0; x < split; x++) {                   /* the impulse, then the tail dying away */
        const float t = (float)(x - p->gx0) / ((float)(split - p->gx0) * len);
        const float wob = tape * 1.2f * fm_sin_turns((float)x * 0.05f + bs.t * 0.4f);
        const float lv = rv * fm_expf(-t * 4.0f) + wob * 0.03f;
        const int32_t top = gy_of(p, lv);
        if (t > 1.2f)
            break;
        for (y = top; y < p->gy1; y++)
            if (px_dith(x, y, 1 + (y == top ? 3 : 0)))
                px_pset(x, y, y == top ? PC_BLOB2 : (x + y) % 5 ? PC_RAIN : PC_BLOBD);
    }
    px_rect(p->gx0, p->gy0, 2, p->gy1 - p->gy0, PC_WHITE);
    /* the fry: what goes in, what comes out */
    {
        const float g = 1.0f + fry * fry * 24.0f;
        int32_t lx = 0, ly = 0, i;
        px_rect(split + 2, p->gy0, 1, p->gy1 - p->gy0, PC_WALL);
        for (i = 0; i <= 20; i++) {
            const float in = (float)i / 10.0f - 1.0f, out = fm_tanhf(in * g) / fm_tanhf(g);
            const int32_t cx = split + 5 + i, cy = (p->gy0 + p->gy1) / 2 - (int32_t)(out * (float)(p->gy1 - p->gy0) * 0.45f);
            if (i)
                px_line(lx, ly, cx, cy, fry > 0.6f ? PC_SPEC : PC_TEAR);
            lx = cx;
            ly = cy;
        }
    }
}

/* a round step: d 3 (a plus) or 4/5 (corners cut); ring: only its edge */
static void step_dot(int32_t x, int32_t y, int32_t d, uint8_t c, int ring)
{
    int32_t i, j;
    for (j = 0; j < d; j++)
        for (i = 0; i < d; i++) {
            const int corner = (i == 0 || i == d - 1) && (j == 0 || j == d - 1);
            const int edge = i == 0 || j == 0 || i == d - 1 || j == d - 1;
            if (corner || (ring && !edge))
                continue;
            px_pset(x + i, y + j, c);
        }
}

static void schem_deck(const drawer_t *p)                /* the drums: round steps, cold colours, the playhead */
{
    /* kick: the blob's pale blue; snare and clap: a tear's; hats: rain; bass: the ghost's violet */
    static const uint8_t HIT[4] = {PC_BLOB2, PC_TEAR, PC_RAIN2, PC_GHOST};
    static const uint8_t SOFT[4] = {PC_BLOB0, PC_RAIN, PC_RAIN, PC_BLOBD};
    const int s = sy_tel.drums_on ? sy_tel.style : proj.par[P_STYLE], steps = sy_style_steps(s);
    const int fill = sy_tel.drums_on && (sy_tel.bar & 3u) == 3u;
    const float cw = (float)(p->gx1 - p->gx0) / (float)steps;
    const int32_t d = cw >= 6.0f ? 5 : 4;                  /* (24 steps: 4, still round, nearly touching) */
    int r, k;
    for (r = 0; r < 4; r++)
        for (k = 0; k < steps; k++) {
            const float sw = (k & 1) ? fm_minf(pn(P_SWING) * 0.4f + 0.0f, 0.6f) * cw : 0.0f;   /* swing: late */
            const uint32_t hh = (uint32_t)(k * 2654435761u + (uint32_t)r * 40503u);
            const float hu = pn(P_HUMAN) * ((float)((hh >> 12) & 7u) - 3.5f) * 0.3f;   /* human: unsteady */
            const int32_t x = p->gx0 + (int32_t)((float)k * cw + (cw - (float)d) * 0.5f + sw + hu), y = p->gy0 + r * 5;
            const int here = sy_tel.drums_on && k == sy_tel.step;
            char c;
            if (r == 0)
                c = sy_style_step(s, SY_TR_KICK, fill, k);
            else if (r == 1)
                c = (c = sy_style_step(s, SY_TR_SNARE, fill, k)) != '.' ? c : sy_style_step(s, SY_TR_CLAP, fill, k);
            else if (r == 2)
                c = (c = sy_style_step(s, SY_TR_HAT, fill, k)) != '.' ? c : sy_style_step(s, SY_TR_OHAT, fill, k);
            else
                c = sy_style_step(s, SY_TR_BASS, fill, k);
            if (c == '.' || (r == 3 && c == '_'))
                step_dot(x, y, d, here ? PC_DIM : (r == 3 && c == '_') ? PC_BLOBD : PC_GRID, 1);   /* empty: a ring */
            else
                step_dot(x, y, d, here ? PC_SPEC : (c == 'x' || r == 3) ? HIT[r] : SOFT[r], 0);
        }
    if (sy_tel.drums_on) {                               /* the playhead: a pale glint under the step */
        const int32_t x = p->gx0 + (int32_t)((float)sy_tel.step * cw + (cw - (float)d) * 0.5f);
        px_rect(x + 1, p->gy0 + 20, d - 2 > 1 ? d - 2 : 1, 1, PC_SPEC);
    }
}

/* the mixer: four channel strips, each a fader (its level) and a meter (what comes through it now) */
static void schem_mix(const drawer_t *p)
{
    static const uint8_t CH[4] = {P_SYNTH, P_DRUMS, P_BASS, P_GUNK};
    static float hold[4];
    int i;
    for (i = 0; i < 4; i++) {
        const int32_t x = 3 + i * 29 + 8, h = p->gy1 - p->gy0;
        const float lv = fm_clampf(sy_tel.lvl[i] * (i == 3 ? 6.0f : 1.6f), 0.0f, 1.0f);
        const int32_t fy = gy_of(p, pn(CH[i]));
        int32_t y, mh;
        hold[i] = lv > hold[i] ? lv : hold[i] * 0.94f;
        px_rect(x, p->gy0, 1, h, PC_GRID);                  /* the fader's slot */
        px_rect(x - 3, fy - 1, 7, 3, PC_BLOB2);              /* its cap */
        px_rect(x - 2, fy, 5, 1, PC_BLOB3);
        mh = (int32_t)(hold[i] * (float)h + 0.5f);          /* the meter beside it */
        for (y = 0; y < h; y += 2)
            px_rect(x + 6, p->gy1 - y - 1, 4, 1, y < mh ? (y > h * 3 / 4 ? PC_SPEC : i == 3 ? PC_GHOST : PC_TEAR) : PC_GRID);
    }
}

/* the arpeggiator: the next steps as a roll, low notes down, high up; the one playing lit; gate as length */
static void schem_arp(const drawer_t *p)
{
    uint8_t held[16];
    const int nh = sy_arp_held(held, 16), steps = 16;
    const float cw = (float)(p->gx1 - p->gx0) / (float)steps, gate = pn(P_ARPGATE);
    int k, lo = 127, hi = 0;
    uint32_t base;
    if (!ui.arp || nh == 0) {
        px_text(p->gx0 + 2, p->gy0 + 6, ui.arp ? "HOLD SOME KEYS" : "ARP IS OFF", PC_DIM);
        return;
    }
    base = sy_tel.arp_seq > 0u ? sy_tel.arp_seq - 1u : 0u;
    base -= base % (uint32_t)steps;                        /* the page of 16 the playing step is on */
    for (k = 0; k < steps; k++) {
        const int n = sy_arp_note(nh, (int)(base + (uint32_t)k));
        lo = n >= 0 && n < lo ? n : lo;
        hi = n > hi ? n : hi;
    }
    if (hi == lo)
        hi = lo + 1;
    for (k = 0; k < steps; k++) {
        const int n = sy_arp_note(nh, (int)(base + (uint32_t)k));
        const int now = sy_tel.arp_seq > 0u && base + (uint32_t)k == sy_tel.arp_seq - 1u;
        const int32_t x = p->gx0 + (int32_t)((float)k * cw), y = gy_of(p, 0.08f + 0.84f * (float)(n - lo) / (float)(hi - lo));
        const int32_t w = (int32_t)(cw * gate) > 2 ? (int32_t)(cw * gate) : 2;
        if (n < 0)
            continue;
        px_rect(x, y - 1, w, 3, now ? PC_SPEC : PC_BLOB2);
        px_pset(x, y - 1, PC_BLOBD);                        /* (rounded) */
        px_pset(x, y + 1, PC_BLOBD);
        if (proj.par[P_ARPMODE] == ARP_STAB) {              /* a stab: the whole chord, stacked */
            int c;
            for (c = 1; c < nh && c < 4; c++)
                px_rect(x, y - 1 - c * 3, w, 2, now ? PC_SPEC : PC_TEAR);
        }
    }
}

/* the loop: every note as a bar along the loop, low notes down, high up; the playhead (red while it
 * records); with its steps out (SEQ held, or its page staying), the bar on show and its 16 steps */
static void schem_loop(const drawer_t *p)
{
    int n, i, lo = 127, hi = 0;
    const sy_ev_t *ev = sy_loop_events(&n);
    const int lt = sy_tel.loop_ticks ? sy_tel.loop_ticks : SY_TPB, w = p->gx1 - p->gx0;
    const int steps = step_mode(), bar = ui.loop_bar, bars = lt / SY_TPB;
    for (i = 0; i < n; i++) {
        lo = ev[i].note < lo ? ev[i].note : lo;
        hi = ev[i].note > hi ? ev[i].note : hi;
    }
    if (hi - lo < 12) {
        lo = (lo + hi) / 2 - 6;
        hi = lo + 12;
    }
    for (i = 1; i < bars; i++)                           /* the bars' seams */
        px_rect(p->gx0 + i * w / bars, p->gy0, 1, p->gy1 - p->gy0, PC_GRID);
    if (steps) {                                         /* the bar the white keys are, and its steps */
        const int32_t bx = p->gx0 + bar * w / bars, bw = w / bars;
        int k;
        for (k = 0; k < 16; k++) {
            const int32_t sx = bx + k * bw / 16;
            const int sel = sy_tel.loop_sel == bar * 16 + k;
            px_rect(sx, p->gy1 + 1, bw / 16 > 1 ? bw / 16 - 1 : 1, 1, sel ? PC_SPEC : k % 4 ? PC_WALL : PC_BLOB0);
        }
        px_rect(bx, p->gy0, 1, p->gy1 - p->gy0, PC_TEAR);
        px_rect(bx + bw - 1, p->gy0, 1, p->gy1 - p->gy0, PC_TEAR);
    }
    for (i = 0; i < n; i++) {
        const sy_ev_t *e = &ev[i];
        const int32_t x = p->gx0 + (int32_t)e->t * w / lt;
        const int32_t len = e->len ? (int32_t)e->len * w / lt : 2;
        const int32_t y = gy_of(p, 0.08f + 0.84f * (float)(e->note - lo) / (float)(hi - lo));
        const int sel = sy_tel.loop_sel >= 0 && e->t / 12 == (uint16_t)sy_tel.loop_sel;
        if ((int)e->t >= lt)
            continue;
        px_rect(x, y - 1, len > 1 ? len : 2, 2, sel ? PC_SPEC : (e->flags & 1u) ? PC_BLOB3 : PC_BLOB2);
    }
    if (sy_tel.drums_on) {                               /* the playhead */
        const int32_t x = p->gx0 + (int32_t)sy_tel.loop_pos * w / lt;
        int32_t y;
        for (y = p->gy0; y < p->gy1; y += sy_tel.rec == 2 ? 1 : 2)
            px_pset(x, y, sy_tel.rec == 2 ? PC_RED : PC_SPEC);
    }
    if (sy_tel.rec == 1) {                               /* counting in: four lights, one a beat */
        int k;
        for (k = 0; k < 4; k++)
            px_rect((p->gx0 + p->gx1) / 2 - 10 + k * 6, (p->gy0 + p->gy1) / 2 - 1, 4, 4, k <= sy_tel.count ? PC_RED : PC_DIM);
    } else if (!n) {
        px_text(p->gx0 + 2, p->gy0 + 6, "REC: PLAY IT IN", PC_DIM);
    }
}

static void schem_setup(const drawer_t *p)               /* the glide: a note sliding to the next */
{
    const float gl = pn(P_GLIDE);
    float v[48];
    int i;
    for (i = 0; i < 48; i++) {
        const float t = (float)i / 47.0f;
        v[i] = t < 0.3f ? 0.2f : 0.2f + 0.6f * (1.0f - fm_expf(-(t - 0.3f) / (0.01f + gl * 0.3f)));
    }
    curve(p, v, 48, PC_BLOB2, PC_BLOBD, 0);
    px_text(p->gx1 - 30, p->gy1 - 6, proj.par[P_MIDIOUT] ? "OUT ON" : "OUT OFF", proj.par[P_MIDIOUT] ? PC_TEAR : PC_DIM);
    px_text(p->gx1 - 30, p->gy0 + 1, proj.par[P_MIDICH] ? "CH ONE" : "CH ALL", PC_DIM);
}

static int preset_card(void) { return ui.preset_t && plat_ms() - ui.preset_t < 2600u; }

/* PRESETS: a whole sound at once. Its card: its name, which of the eight, and what it brought (its
 * wave, its filter, its envelope), so it is plain that everything changed, not just the wave. */
static void draw_preset_card(int32_t y0)
{
    drawer_t q;
    float v[33];
    int i;
    px_text(3, y0 + 3, "PRESET", PC_BLOB2);
    {
        const char *nm = MOODS[ui.mood].name;
        px_text(PX_W - 3 - px_text_w(nm) > 36 ? PX_W - 3 - px_text_w(nm) : 36, y0 + 3, nm, PC_WHITE);
    }
    {   /* its kind, and where it sits among that kind's presets */
        const int cat = MOODS[ui.mood].cat;
        int k = 0;
        px_text(3, y0 + 10, CAT_NAME[cat], PC_TEAR);
        for (i = 0; i < NMOOD; i++)
            if (MOODS[i].cat == cat) {
                px_rect(4 + px_text_w(CAT_NAME[cat]) + 3 + k * 5, y0 + 12, 3, 2, i == ui.mood ? PC_SPEC : PC_GRID);
                k++;
            }
    }
    q.y0 = y0;
    q.gy0 = y0 + 19;
    q.gy1 = y0 + 34;
    for (i = 0; i < 3; i++) {
        static const char *const NAME[3] = {"WAVE", "FILTER", "ENV"};
        int k;
        q.gx0 = 3 + i * 39;
        q.gx1 = q.gx0 + 34;
        px_rect(q.gx0 - 1, q.gy0 - 1, 37, q.gy1 - q.gy0 + 3, PC_BG0);
        for (k = 0; k < 33; k++) {
            const float t = (float)k / 32.0f;
            if (i == 0) {
                v[k] = 0.5f + 0.45f * sy_shape(proj.par[P_TABLE], pn(P_POS), t);
            } else if (i == 1) {
                const float kk = 2.0f - 1.94f * pn(P_RESO), w = fm_exp2f(t * 10.0f - (pn(P_CUTOFF) * 9.3f + 0.6f)), w2 = w * w;
                v[k] = (10.0f * fm_log2f(1.0f / ((1.0f - w2) * (1.0f - w2) + w2 * kk * kk)) * 0.30103f + 36.0f) / 54.0f;
            } else {
                const float a = 0.05f + 0.25f * pn(P_ATTACK), d = 0.05f + 0.25f * pn(P_DECAY), r = 0.05f + 0.3f * pn(P_RELEASE);
                const float su = pn(P_SUSTAIN), hs = 1.0f - r;
                v[k] = t < a ? t / a : t < a + d ? 1.0f - (1.0f - su) * (t - a) / d : t < hs ? su : su * (1.0f - (t - hs) / r);
            }
        }
        curve(&q, v, 33, PC_BLOB2, PC_BLOBD, 1);
        px_text(q.gx0, y0 + 37, NAME[i], PC_DIM);
    }
}

static void draw_panel(const geo_t *g)
{
    drawer_t p;
    const uint32_t now = plat_ms(), since = now - ui.focus_t;
    int i;
    p.y0 = (int32_t)g->drawer;
    if (p.y0 >= PX_H)
        return;
    {   /* dark glass: three pixels in four go dark, the fourth keeps a darker shade of what is behind */
        int32_t x, y;
        for (y = p.y0 < 0 ? 0 : p.y0; y < PX_H; y++)
            for (x = 0; x < PX_W; x++) {
                uint8_t *q = &px_fb[y * PX_W + x];
                *q = ((x ^ y) & 1) || (y & 2) ? PC_PANEL : GLASS[*q < PC_N ? *q : 0];
            }
    }
    px_rect(0, p.y0, PX_W, 1, PC_GRID);
    if (preset_card()) {
        draw_preset_card(p.y0);
        return;
    }
    /* the tab: the page's name, and what it is set to */
    px_text(3, p.y0 + 3, PAGE_NAME[ui.page], PC_BLOB2);
    {
        const char *what = 0;
        switch (ui.page) {
        case PG_BODY: what = SY_TABLE_NAME[proj.par[P_TABLE]]; break;
        case PG_VIBE: what = LFO_SHAPE_NAME[proj.par[P_LFOSHAPE]]; break;
        case PG_DECK: what = sy_style_name(proj.par[P_STYLE]); break;
        case PG_ARP: what = ARP_RATE_NAME[proj.par[P_ARPRATE]]; break;
        case PG_LOOP: what = sy_tel.rec == 2 ? "REC" : sy_tel.rec == 1 ? "READY" : sy_tel.loop_sel >= 0 ? "STEP" : "ALL"; break;
        default: break;
        }
        if (what) {
            char b[24];
            const int32_t room = PX_W - 3 - (3 + px_text_w(PAGE_NAME[ui.page]) + 6);
            int n = 0;
            while (what[n] && n < 23 && (n + 1) * 5 - 1 <= room)
                n++;
            for (i = 0; i < n; i++)
                b[i] = what[i];
            b[n] = 0;
            px_text(PX_W - 3 - px_text_w(b), p.y0 + 3, b, PC_DIM);
        }
    }
    p.gx0 = 4;
    p.gx1 = PX_W - 5;
    p.gy0 = p.y0 + 12;
    p.gy1 = p.y0 + 30;
    grid(&p);
    switch (ui.page) {
    case PG_BODY: schem_body(&p); break;
    case PG_EYES: schem_eyes(&p); break;
    case PG_MOUTH: schem_mouth(&p); break;
    case PG_VIBE: schem_vibe(&p); break;
    case PG_TEARS: schem_tears(&p); break;
    case PG_WALLS: schem_walls(&p); break;
    case PG_DECK: schem_deck(&p); break;
    case PG_MIX: schem_mix(&p); break;
    case PG_ARP: schem_arp(&p); break;
    case PG_LOOP: schem_loop(&p); break;
    default: schem_setup(&p); break;
    }
    /* the knobs: a name over a meter, the one being turned lit up */
    for (i = 0; i < 4; i++) {
        const int k = PAGE_KNOB[ui.page][i];
        const int32_t x = 3 + i * 29, y = p.y0 + 32;   /* (the name 6 high, the meter under it) */
        const int hot = ui.touched == i && since < 1500u;
        const sy_param_t *sp;
        float v;
        int32_t w;
        if (k == K_NONE && ui.page == PG_LOOP) {         /* the stepped step: its note, length, velocity */
            int n, j;
            const sy_ev_t *ev = sy_loop_events(&n);
            float lv = 0.0f;
            for (j = 0; j < n; j++)                      /* (no step chosen: the whole loop, its first note shows) */
                if (sy_tel.loop_sel < 0 ? j == 0 : ev[j].t / 12 == (uint16_t)sy_tel.loop_sel)
                    lv = i == 0 ? (float)ev[j].note / 127.0f : i == 1 ? (float)ev[j].len / 96.0f : (float)ev[j].vel / 127.0f;
            px_text(x, y, PAGE_LABEL[ui.page][i], hot ? PC_WHITE : n ? PC_DIM : PC_GRID);
            px_rect(x, y + 8, 26, 2, PC_GRID);
            w = (int32_t)(fm_clampf(lv, 0.0f, 1.0f) * 26.0f + 0.5f);
            px_rect(x, y + 8, w, 2, hot ? PC_SPEC : PC_TEAR);
            continue;
        }
        if (k == K_NONE)
            continue;
        sp = sy_param_info(k);
        v = (float)(proj.par[k] - sp->lo) / (float)(sp->hi - sp->lo);
        px_text(x, y, PAGE_LABEL[ui.page][i], hot ? PC_WHITE : PC_DIM);
        px_rect(x, y + 8, 26, 2, PC_GRID);
        w = (int32_t)(v * 26.0f + 0.5f);
        if (sp->hi - sp->lo <= 16) {                     /* a few choices: one notch lit */
            const int32_t nw = 26 / (sp->hi - sp->lo + 1) > 1 ? 26 / (sp->hi - sp->lo + 1) : 1;
            px_rect(x + (int32_t)(v * (float)(26 - nw)), y + 8, nw, 2, hot ? PC_SPEC : PC_TEAR);
        } else {
            px_rect(x, y + 8, w, 2, hot ? PC_SPEC : PC_TEAR);
            if (w > 0)
                px_rect(x + w - 1, y + 8, 1, 2, PC_BLOB2);
        }
    }
}

static void draw_xp(void)
{
    const uint32_t lv = xp_level(proj.xp), a = xp_for(lv), b = xp_for(lv + 1u);
    const int32_t w = (int32_t)((float)(proj.xp - a) / (float)(b - a) * 118.0f);
    px_rect(1, 0, w, 1, PC_XP);
}


static void draw_caption(void)
{
    const uint32_t now = plat_ms();
    const int32_t cx = tape_up() ? 84 : 60, maxw = tape_up() ? 70 : PX_W - 4;
    char l1[32], l2[32];
    int i, n, cut = -1;
    if (!ui.cap[0] || (int32_t)(ui.cap_until - now) <= 0)
        return;
    if (px_text_w(ui.cap) <= maxw) {
        px_text_c(cx, 3, ui.cap, PC_WHITE, PC_BG0);
        return;
    }
    for (n = 0; ui.cap[n]; n++)                          /* too wide: two lines, split at the space nearest the middle */
        ;
    for (i = 0; ui.cap[i]; i++)
        if (ui.cap[i] == ' ' && (cut < 0 || (i > n / 2 ? i - n / 2 : n / 2 - i) < (cut > n / 2 ? cut - n / 2 : n / 2 - cut)))
            cut = i;
    if (cut < 0)
        cut = n / 2;
    for (i = 0; i < cut && i < 31; i++)
        l1[i] = ui.cap[i];
    l1[i] = 0;
    for (i = 0; ui.cap[cut + 1 + i] && i < 31; i++)
        l2[i] = ui.cap[cut + 1 + i];
    l2[i] = 0;
    px_text_c(cx, 3, l1, PC_WHITE, PC_BG0);
    px_text_c(cx, 11, l2, PC_WHITE, PC_BG0);
}

/* ------------------------------------------------------------ animate --- */
static void wave_out(const geo_t *g, float a)
{
    int i;
    for (i = 0; i < 6; i++)
        if (!bs.wave[i].on) {
            bs.wave[i].on = 1;
            bs.wave[i].r = g->r;
            bs.wave[i].a = a;
            bs.wave[i].dir = 1.0f;
            return;
        }
}

static void animate(const geo_t *g)
{
    const float dt = bs.dt;
    int i;
    if (sy_tel.kick_n != bs.kick_n) {
        bs.kick_n = sy_tel.kick_n;
        bs.kick = 1.0f;
        if (pn(P_REVERB) > 0.05f)
            wave_out(g, pn(P_REVERB) * 0.5f);
    }
    if (sy_tel.snare_n != bs.snare_n) {
        bs.snare_n = sy_tel.snare_n;
        bs.snare = 1.0f;
        bs.blink = 1.0f;
    }
    if (sy_tel.hat_n != bs.hat_n) {                      /* rain on the window, and a drop on it */
        bs.hat_n = sy_tel.hat_n;
        bs.hat = 1.0f;
        spawn(PK_SPLASH, g->cx + b_rs() * g->r * 0.6f, g->cy - g->r * 0.9f, b_rs() * 20.0f, -15.0f - b_rf() * 10.0f, 0.3f);
    }
    if (sy_tel.bass_n != bs.bass_n) {
        bs.bass_n = sy_tel.bass_n;
        bs.bass = 1.0f;
    }
    bs.kick -= bs.kick * dt * 9.0f;
    bs.snare -= bs.snare * dt * 8.0f;
    bs.hat -= bs.hat * dt * 3.0f;
    bs.bass -= bs.bass * dt * 6.0f;
    bs.wall_flash -= bs.wall_flash * dt * 4.0f;
    bs.echo_flash -= bs.echo_flash * dt * 6.0f;
    if (sy_tel.drums_on)
        bs.reel += dt * (float)proj.par[P_TEMPO] / 60.0f * 0.5f;
    /* a new tape: the old one slides out, the blob shrugs, the new one slides in */
    if (bs.tape_shown != proj.par[P_STYLE] && bs.tape_anim <= 0.0f) {
        bs.tape_anim = 0.01f;
        bs.tape_dir = 1;
        bs.shrug = 1.0f;
    }
    if (bs.tape_anim > 0.0f) {
        bs.tape_anim += (float)bs.tape_dir * dt * 5.0f;
        if (bs.tape_anim >= 1.0f) {
            bs.tape_shown = (int8_t)proj.par[P_STYLE];
            bs.tape_dir = -1;
        }
        if (bs.tape_anim <= 0.0f)
            bs.tape_anim = 0.0f;
    }
    bs.shrug -= bs.shrug * dt * 3.0f;
    /* notes: tears (the delay), waves (the reverb) */
    if (sy_tel.notes != bs.notes) {
        const float dly = pn(P_DELAY), fb = pn(P_FEEDBACK) * 0.95f, per = (0.06f + 0.64f * pn(P_TIME));
        int n = dly > 0.02f ? 1 + (int)(fb * fb * 7.0f) : 0, k;
        bs.notes = sy_tel.notes;
        for (k = 0, i = 0; k < n && i < 10; i++)
            if (!bs.echo[i].on) {
                bs.echo[i].on = 1;
                bs.echo[i].due = per * (float)(k + 1);
                bs.echo[i].s = (0.9f + dly * 1.6f) * fm_powf(fb * fb > 0.05f ? fb * fb : 0.05f, (float)k * 0.5f);
                bs.echo[i].side = (uint8_t)(k & 1);
                k++;
            }
        if (pn(P_REVERB) > 0.05f)
            wave_out(g, pn(P_REVERB));
        bs.sigh_due = 0.0f;
    }
    for (i = 0; i < 10; i++)
        if (bs.echo[i].on && (bs.echo[i].due -= dt) <= 0.0f) {
            int j;
            bs.echo[i].on = 0;
            bs.echo_flash = 1.0f;
            for (j = 0; j < 24; j++)
                if (!bs.tear[j].on) {
                    const float s = g->r / 26.0f, side = bs.echo[i].side ? 1.0f : -1.0f;
                    bs.tear[j].on = 1;
                    bs.tear[j].x = g->cx + side * 11.0f * s;
                    bs.tear[j].y = g->cy - 3.0f * s + g->face_dy;
                    bs.tear[j].vy = 5.0f;
                    bs.tear[j].vx = side * 3.0f;
                    bs.tear[j].s = bs.echo[i].s;
                    break;
                }
        }
    for (i = 0; i < 24; i++)
        if (bs.tear[i].on) {
            bs.tear[i].vy += 120.0f * dt;
            bs.tear[i].y += bs.tear[i].vy * dt;
            bs.tear[i].x += bs.tear[i].vx * dt;
            if (bs.tear[i].y >= g->floor) {
                bs.tear[i].on = 0;
                bs.puddle += bs.tear[i].s * 0.03f;
                spawn(PK_SPLASH, bs.tear[i].x, g->floor - 1.0f, -12.0f, -18.0f, 0.25f);
                spawn(PK_SPLASH, bs.tear[i].x, g->floor - 1.0f, 12.0f, -18.0f, 0.25f);
            }
        }
    bs.puddle = fm_clampf(bs.puddle - dt * 0.012f, 0.0f, 1.0f);
    /* waves: out to the walls (they flare), back, weaker each time (Size: how much comes back) */
    for (i = 0; i < 6; i++)
        if (bs.wave[i].on) {
            const float wall = g->bw > g->r + 3.0f ? g->bw : g->r + 3.0f, keep = 0.35f + 0.6f * pn(P_SIZE);
            bs.wave[i].r += bs.wave[i].dir * dt * 55.0f;
            if (bs.wave[i].dir > 0.0f && bs.wave[i].r >= wall) {
                bs.wave[i].dir = -1.0f;
                bs.wave[i].a *= keep;
                bs.wall_flash = bs.wave[i].a + 0.3f;
            } else if (bs.wave[i].dir < 0.0f && bs.wave[i].r <= g->r) {
                bs.wave[i].dir = 1.0f;
                bs.wave[i].a *= keep;
            }
            if (bs.wave[i].a < 0.12f || bs.wave[i].r > 90.0f)
                bs.wave[i].on = 0;
        }
    if (sy_tel.steals != bs.steals) {                    /* a voice stolen: a buddy yeeted */
        bs.steals = sy_tel.steals;
        spawn(PK_YEET, g->cx + (float)(SY_NVOICE * 5) * 0.5f, g->cy - g->r - 8.0f,
              (b_rnd() & 1u ? 1.0f : -1.0f) * (60.0f + b_rf() * 40.0f), -70.0f, 1.6f);
    }
    if (sy_tel.clips != bs.clips) {                      /* clipping: on fire */
        bs.clips = sy_tel.clips;
        bs.fire = 1.0f;
    }
    if (bs.fire > 0.05f && (b_rnd() & 1u))
        spawn(PK_FIRE, g->cx + b_rs() * g->r, g->cy + g->r * 0.6f - b_rf() * g->r * 0.5f, b_rs() * 4.0f,
              -25.0f - b_rf() * 25.0f, 0.5f + 0.4f * b_rf());
    bs.fire -= bs.fire * dt * 0.7f;
    if (pn(P_FRY) > 0.4f && b_rf() < pn(P_FRY) * dt * 4.0f)
        spawn(PK_SWEAT, g->cx + (b_rnd() & 1u ? -1.0f : 1.0f) * g->r * 0.75f, g->cy - g->r * 0.3f, 0.0f, 12.0f, 0.8f);
    if (pn(P_FRY) > 0.85f && b_rf() < dt * 10.0f)
        spawn(PK_STEAM, g->cx + b_rs() * g->r * 0.5f, g->cy - g->r, b_rs() * 5.0f, -18.0f, 1.0f);
    if (!sy_tel.voices && bs.gates && bs.sigh_due <= 0.0f) {   /* a sigh after the last note fades */
        bs.sigh_due = 1.0f;
        spawn(PK_PUFF, g->cx + 5.0f, g->cy + 6.0f, 9.0f, -7.0f, 1.2f);
    }
    bs.gates = sy_tel.voices;
    if ((bs.blink_due -= dt) <= 0.0f) {
        bs.blink = 1.0f;
        bs.blink_due = 2.5f + b_rf() * 4.0f + bs.stare * 8.0f;
    }
    bs.blink -= bs.blink * dt * 12.0f;
    {
        const float idle = (float)(plat_ms() - ui.act_t) * 0.001f;
        const int quiet = !sy_tel.voices && !sy_tel.drums_on;
        bs.sleep = approach(bs.sleep, quiet && idle > 20.0f && idle < 90.0f ? 1.0f : 0.0f, dt * 0.8f);
        bs.stare = approach(bs.stare, quiet && idle >= 90.0f ? 1.0f : 0.0f, dt * 2.0f);
        if (bs.sleep > 0.8f && b_rf() < dt * 0.8f)
            spawn(PK_ZZZ, g->cx + g->r * 0.6f, g->cy - g->r * 0.6f, 6.0f, -8.0f, 2.0f);
        if (bs.stare > 0.9f && ui.cap_until < plat_ms() && b_rf() < dt * 0.05f)
            ui_caption("...", 2500u);
    }
    bs.pancake = approach(bs.pancake, ui.pancake ? 1.0f : 0.0f, dt * 6.0f);
    for (i = 0; i < 48; i++)
        if (bs.p[i].on) {
            const uint8_t k = bs.p[i].kind;
            bs.p[i].x += bs.p[i].vx * dt;
            bs.p[i].y += bs.p[i].vy * dt;
            if (k == PK_SPLASH || k == PK_YEET || k == PK_SWEAT)
                bs.p[i].vy += (k == PK_YEET ? 140.0f : 90.0f) * dt;
            if (k == PK_YEET)
                bs.p[i].spin += dt * 3.0f;
            if ((bs.p[i].life -= dt) <= 0.0f || bs.p[i].y > 125.0f || bs.p[i].x < -10.0f || bs.p[i].x > 130.0f)
                bs.p[i].on = 0;
        }
    for (i = 0; i < 30; i++) {                           /* the rain outside (0..1 across the window) */
        bs.rain_y[i] += dt * (0.9f + (float)(i % 5) * 0.12f);
        bs.rain_x[i] -= dt * 0.25f;
        if (bs.rain_y[i] > 1.0f || bs.rain_x[i] < 0.0f) {
            bs.rain_y[i] = -b_rf() * 0.3f;
            bs.rain_x[i] = b_rf() * 1.2f;
        }
    }
}

/* ------------------------------------------------------------ the frame --- */
static void blob_draw(void)
{
    const uint32_t now = plat_ms();
    geo_t g;
    float sqx = 1.0f, sqy = 1.0f, bobx = 0.0f, boby = 0.0f, pa;
    const float size = pn(P_SIZE), lfo = sy_tel.lfo;
    float depth = pn(P_LFODEPTH);
    bs.dt = bs.last_ms ? (float)(now - bs.last_ms) * 0.001f : 0.033f;
    if (bs.dt > 0.1f)
        bs.dt = 0.1f;
    bs.last_ms = now;
    bs.t += bs.dt;
    /* the panel slides in and out; the room takes what is left */
    bs.panel = approach(bs.panel, ui.panel || preset_card() ? 1.0f : 0.0f, bs.dt * 8.0f);
    pa = bs.panel;
    g.ab = (float)PX_H;                                 /* the room keeps the screen; the drawer is a dark glass over it */
    g.drawer = (float)PX_H - (float)PANEL_H * pa;
    /* the room: Size pushes its walls out (then away); the blob shrinks in it */
    g.rcx = 60.0f;
    g.rcy = 55.0f - 12.0f * pa;                          /* (the room lifts a little when the drawer opens) */
    g.bw = 12.0f + 46.0f * size;
    /* the camera looks down from above (the synth is on a table below you): the back wall sits high, the
     * ceiling is a sliver, the floor fills the lower half */
    g.vy = g.rcy - 17.0f - 6.0f * size;
    g.bh = fm_minf(g.bw * 0.6f, g.vy - 3.0f);
    g.r = lerpf(38.0f, 34.0f, pa) * (1.0f - 0.35f * size);
    g.face_dy = 0.0f;
    /* the LFO's dance: big enough to see at any depth. It moves, and its body goes with it: it leans
     * into a sway, stretches up a bob, squashes landing a bounce, nods, twitches; its lumps wobble */
    /* how big: full while notes sound (riding the envelope), a preview while you set it, a murmur idle */
    {
        const int setting = ui.page == PG_VIBE && ui.panel && plat_ms() - ui.focus_t < 2000u && ui.touched >= 0;
        const float want = sy_tel.voices ? 0.45f + 0.55f * fm_clampf(sy_tel.amp_env * 1.3f, 0.0f, 1.0f)
                                         : setting ? 0.7f : 0.12f;
        bs.vamt = approach(bs.vamt, want, bs.dt * (want > bs.vamt ? 6.0f : 1.5f));
    }
    depth *= bs.vamt;
    bs.lean = 0.0f;
    bs.wob = lfo * depth;
    switch (proj.par[P_LFOSHAPE]) {
    case LFO_SINE:                                       /* a sway: side to side, leaning into it */
        bobx = lfo * depth * 16.0f;
        bs.lean = lfo * depth * 0.4f;
        break;
    case LFO_TRI:                                        /* a bob: up and down, stretching as it rises */
        boby = lfo * depth * 12.0f;
        sqy *= 1.0f - lfo * depth * 0.18f;
        sqx *= 1.0f + lfo * depth * 0.12f;
        break;
    case LFO_SQUARE:                                     /* a bounce: up in the air, squashed when it lands */
        boby = lfo > 0.0f ? -depth * 14.0f : 0.0f;
        sqy *= lfo > 0.0f ? 1.0f + depth * 0.15f : 1.0f - depth * 0.2f;
        sqx *= lfo > 0.0f ? 1.0f - depth * 0.08f : 1.0f + depth * 0.15f;
        break;
    case LFO_SAW:                                        /* a nod: the face drops, the body tips forward */
        g.face_dy = lfo * depth * 8.0f;
        boby = lfo * depth * 4.0f;
        bs.lean = lfo * depth * 0.15f;
        break;
    default:                                             /* a twitch: sudden, all over */
        bobx = lfo * depth * 8.0f;
        boby = -fm_fabsf(lfo) * depth * 6.0f;
        bs.lean = -lfo * depth * 0.25f;
        break;
    }
    sqy *= 1.0f + 0.03f * fm_sin_turns(bs.t * (0.18f + 0.1f * (1.0f - bs.sleep)));   /* breathing */
    sqy *= 1.0f - 0.2f * bs.kick;
    sqx *= 1.0f + 0.12f * bs.kick;
    sqy *= 1.0f - 0.7f * bs.pancake;
    sqx *= 1.0f + 0.5f * bs.pancake;
    if (proj.par[P_STYLE] == ST_GABBER && sy_tel.drums_on && bs.kick > 0.2f) {   /* gabber: despair */
        bobx += b_rs() * 1.5f;
        boby += b_rs() * 1.0f;
    }
    /* the camera: on the Eyes page it moves in on the eyes, on the Mouth page on the mouth (the part you
     * are shaping fills the view); elsewhere it shows all of it */
    {
        const int on = ui.panel || preset_card();
        const float zt = on && ui.page == PG_EYES ? 2.1f : on && ui.page == PG_MOUTH ? 2.2f : 1.0f;
        const float ft = on && ui.page == PG_EYES ? -0.27f : on && ui.page == PG_MOUTH ? 0.36f : 0.0f;
        bs.zoom = approach(bs.zoom < 0.5f ? 1.0f : bs.zoom, zt, bs.dt * 5.0f);
        bs.focus = approach(bs.focus, ft, bs.dt * 5.0f);
        g.r *= bs.zoom;
        g.face_dy *= bs.zoom;
        boby *= bs.zoom;
        bobx *= bs.zoom;
    }
    g.cx = g.rcx + bobx;
    g.cy = g.rcy + boby - bs.shrug * 2.0f + 1.2f * fm_sin_turns(bs.t * 0.23f) + bs.pancake * g.r * 0.6f;   /* it hovers */
    g.cy -= bs.focus * g.r;                              /* the part in focus, to the middle */
    g.floor = fm_minf(g.cy + g.r * 0.8f + 6.0f + 3.0f * size, g.ab - 3.0f);   /* (from above: right under it) */
    if (g.floor < g.cy + g.r * 0.9f)                     /* (close up: the floor is out of the picture) */
        g.floor = (float)PX_H + 40.0f;

    animate(&g);
    drips_move();
    if (sy_tel.notes != bs.lnotes) {                     /* a note: the light swings, by its pitch */
        bs.lnotes = sy_tel.notes;
        if (sy_tel.last_note >= 0)
            bs.lang_to = 0.08f + 0.34f * (float)((sy_tel.last_note * 7) % 24) / 23.0f;   /* below: right .. left */
    }
    bs.lang = approach(bs.lang, bs.lang_to, bs.dt * 4.0f);
    light_frame();
    draw_room(&g);
    draw_shadow(&g);
    draw_copies(&g, sqx, sqy);
    draw_body(&g, sqx, sqy);
    draw_face(&g);
    draw_tears(&g);
    draw_unlocks(&g);
    draw_voices(&g);
    draw_particles();
    draw_cassette(&g);
    draw_panel(&g);
    draw_xp();
    draw_caption();
    px_present();
}
