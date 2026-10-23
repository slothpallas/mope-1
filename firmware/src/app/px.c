/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1's pixels: a 120 x 120 picture of palette indices, shown twice the size (each pixel 2 x 2 on the
 * 240 x 240 panel), in strips through gfx.c's canvas. Chunky on purpose, and small: 14 KiB.
 * Part of the unity build (ui.c includes it, after gfx.c). */
#define PX_W 120
#define PX_H 120
static uint8_t px_fb[PX_W * PX_H] OM_POOL;

/* the palette: rain at 3 a.m., a sad blob, a sodium lamp; warm reds and oranges only for what is happening */
enum {
    PC_BG0, PC_BG1, PC_BG2, PC_RAIN, PC_RAIN2, PC_WALL, PC_WALL2, PC_BLOB0, PC_BLOB1, PC_BLOB2, PC_BLOB3,
    PC_EYE, PC_PUPIL, PC_TEAR, PC_TEAR2, PC_MOUTH, PC_TONGUE, PC_LAMP, PC_LAMP2, PC_FIRE, PC_FIRE2, PC_LASER,
    PC_FRY, PC_CRUST, PC_WHITE, PC_DIM, PC_TAPE, PC_TAPE2, PC_XP, PC_GHOST, PC_PINK, PC_BLACK,
    /* the warm ones: what is happening, what you are touching (readouts, effects, playheads) */
    PC_ORANGE, PC_RED, PC_AMBER, PC_CORAL, PC_EMBER, PC_PANEL, PC_GRID,
    /* the blob's light: its deepest shade, a midtone, the gleam; the eye's shadow; the floor's sheen */
    PC_BLOBD, PC_BLOBM, PC_SPEC, PC_EYE2, PC_SHEEN,
    /* a cassette: smoked plastic, its edge catching light, the brown tape wound on the spools */
    PC_SMOKE, PC_SMOKE2, PC_SPOOL,
    /* the side walls: barely cold (left), barely warm (right), and the text dripping down them */
    PC_COOLW, PC_WARMW, PC_WARMW2, PC_N
};
static const uint16_t PX_PAL[PC_N] = {
    RGB(13, 16, 24), RGB(22, 27, 38), RGB(31, 38, 51), RGB(58, 74, 92), RGB(93, 113, 134), RGB(42, 49, 64),
    RGB(70, 82, 102), RGB(52, 76, 88), RGB(84, 118, 130), RGB(126, 160, 168), RGB(178, 204, 204),
    RGB(217, 214, 199), RGB(20, 22, 28), RGB(111, 183, 224), RGB(168, 220, 245), RGB(43, 29, 37),
    RGB(140, 74, 90), RGB(224, 145, 58), RGB(122, 79, 36), RGB(224, 69, 42), RGB(245, 197, 66),
    RGB(255, 42, 58), RGB(138, 90, 42), RGB(201, 140, 58), RGB(240, 236, 224), RGB(125, 135, 148),
    RGB(196, 184, 154), RGB(94, 84, 67), RGB(127, 179, 106), RGB(107, 90, 140), RGB(199, 125, 155), RGB(0, 0, 0),
    RGB(238, 132, 52), RGB(222, 64, 52), RGB(250, 186, 84), RGB(240, 110, 94), RGB(110, 40, 34), RGB(10, 13, 20),
    RGB(30, 38, 54),
    RGB(28, 40, 54), RGB(68, 98, 110), RGB(226, 240, 236), RGB(150, 150, 142), RGB(44, 56, 74),
    RGB(34, 33, 38), RGB(78, 76, 86), RGB(84, 50, 32),
    RGB(27, 35, 50), RGB(36, 29, 34), RGB(64, 45, 46),
};
static uint16_t px_pal_sw[PC_N];                    /* byte-swapped for the panel */

static void px_init(void)
{
    uint32_t i;
    for (i = 0; i < PC_N; i++)
        px_pal_sw[i] = swap16(PX_PAL[i]);
}

static inline void px_pset(int32_t x, int32_t y, uint8_t c)
{
    if ((uint32_t)x < PX_W && (uint32_t)y < PX_H)
        px_fb[y * PX_W + x] = c;
}
static inline uint8_t px_get(int32_t x, int32_t y)
{
    return (uint32_t)x < PX_W && (uint32_t)y < PX_H ? px_fb[y * PX_W + x] : PC_BG0;
}

static void px_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t c)
{
    int32_t i, j, x1 = x + w, y1 = y + h;
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x1 > PX_W)
        x1 = PX_W;
    if (y1 > PX_H)
        y1 = PX_H;
    for (j = y; j < y1; j++)
        for (i = x; i < x1; i++)
            px_fb[j * PX_W + i] = c;
}

/* a 2 x 2 ordered dither: c over what is there, `amt` 0..4 of the 4 pixels */
static const uint8_t BAYER[4] = {0, 2, 3, 1};
static inline int px_dith(int32_t x, int32_t y, int amt) { return amt > BAYER[(x & 1) | ((y & 1) << 1)]; }
/* a 4 x 4 one, for shading: -0.5 .. +0.5 */
static const int8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
static inline float px_dith4(int32_t x, int32_t y) { return ((float)BAYER4[(x & 3) | ((y & 3) << 2)] + 0.5f) / 16.0f - 0.5f; }

static void px_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1, err = dx + dy, guard = 400;
    while (guard--) {
        int32_t e2 = 2 * err;
        px_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* a filled ellipse, centre (cx, cy), radii rx ry (floats: it can shrink smoothly) */
static void px_ellipse(float cx, float cy, float rx, float ry, uint8_t c)
{
    int32_t y, x, y0 = (int32_t)(cy - ry - 1.0f), y1 = (int32_t)(cy + ry + 1.0f);
    if (rx <= 0.0f || ry <= 0.0f)
        return;
    for (y = y0; y <= y1; y++) {
        float dy = ((float)y + 0.5f - cy) / ry, w;
        if (dy * dy > 1.0f)
            continue;
        w = rx * fm_sqrtf(1.0f - dy * dy);
        for (x = (int32_t)(cx - w + 0.5f); x < (int32_t)(cx + w + 0.5f); x++)
            px_pset(x, y, c);
    }
}

static void px_ring(float cx, float cy, float r, uint8_t c)
{
    int k, n = (int)(r * 6.3f) + 8;
    for (k = 0; k < n; k++) {
        float a = (float)k / (float)n;
        px_pset((int32_t)(cx + r * fm_cosf(a * FM_TWO_PI)), (int32_t)(cy + r * fm_sin_turns(a)), c);
    }
}

/* a filled polygon (even-odd, any shape): the blob is one */
#define PX_MAXV 96
static void px_poly(const float *vx, const float *vy, int n, uint8_t c, int dither)
{
    int32_t y, ymin = PX_H, ymax = -1, i;
    for (i = 0; i < n; i++) {
        int32_t a = (int32_t)vy[i];
        ymin = a < ymin ? a : ymin;
        ymax = a > ymax ? a : ymax;
    }
    if (ymin < 0)
        ymin = 0;
    if (ymax >= PX_H)
        ymax = PX_H - 1;
    for (y = ymin; y <= ymax; y++) {
        float xs[PX_MAXV], fy = (float)y + 0.5f;
        int m = 0, a, b;
        for (i = 0; i < n; i++) {
            int j = i + 1 == n ? 0 : i + 1;
            float ya = vy[i], yb = vy[j];
            if ((ya <= fy && yb > fy) || (yb <= fy && ya > fy))
                if (m < PX_MAXV)
                    xs[m++] = vx[i] + (fy - ya) / (yb - ya) * (vx[j] - vx[i]);
        }
        for (a = 1; a < m; a++) {                       /* (few crossings: insertion sort) */
            float t = xs[a];
            for (b = a; b > 0 && xs[b - 1] > t; b--)
                xs[b] = xs[b - 1];
            xs[b] = t;
        }
        for (a = 0; a + 1 < m; a += 2) {
            int32_t x, x0 = (int32_t)(xs[a] + 0.5f), x1 = (int32_t)(xs[a + 1] + 0.5f);
            for (x = x0; x < x1; x++)
                if (!dither || px_dith(x, y, dither))
                    px_pset(x, y, c);
        }
    }
}

/* MOPE's own face, 4 x 6: droopy. Its crossbars sag (A E F H P R hang low), its tops are rounded, its
 * S slides, its T leans, its V is lopsided. Each glyph six rows of four bits, one hex digit a row
 * (8 = the leftmost pixel). */
static const uint32_t FONTD[] = {
    /* A-Z */
    0x699F99, 0xE9E99E, 0x788887, 0xE9999E, 0x788E87, 0x788E88, 0x788B96, 0x999F99, 0xE4444E, 0x311196,
    0x9AACA9, 0x888887, 0x9FF999, 0x9DDBB9, 0x699996, 0xE99E88, 0x6999A5, 0xE99EA9, 0x78421E, 0xF44444,
    0x999996, 0x999952, 0x999FF9, 0x996699, 0x995222, 0xF1248F,
    /* 0-9 */
    0x69BD96, 0x4C444E, 0x69124F, 0xE1611E, 0x8AAF22, 0xF8E11E, 0x68E996, 0xF12244, 0x696996, 0x699716,
    /* . ! ? - : ' , / + */
    0x000004, 0x444404, 0x691202, 0x000E00, 262208 /* (0x040040: the build would take it for a register) */, 0x440000, 0x000048, 0x112488, 0x044E40,
};
static uint32_t fontd(char ch)
{
    static const char P[] = ".!?-:',/+";
    int i;
    if (ch >= 'a' && ch <= 'z')
        ch = (char)(ch - 32);
    if (ch >= 'A' && ch <= 'Z')
        return FONTD[ch - 'A'];
    if (ch >= '0' && ch <= '9')
        return FONTD[26 + ch - '0'];
    for (i = 0; P[i]; i++)
        if (P[i] == ch)
            return FONTD[36 + i];
    return 0;
}
#define PX_FONT_H 6
static int32_t px_text_w(const char *s)
{
    int32_t n = 0;
    while (*s++)
        n++;
    return n ? n * 5 - 1 : 0;
}
static void px_text(int32_t x, int32_t y, const char *s, uint8_t c)
{
    for (; *s; s++, x += 5) {
        const uint32_t g = fontd(*s);
        int r, k;
        for (r = 0; r < 6; r++)
            for (k = 0; k < 4; k++)
                if (g >> ((5 - r) * 4 + (3 - k)) & 1u)
                    px_pset(x + k, y + r, c);
    }
}
/* text with a dark outline: readable over anything */
static void px_text_o(int32_t x, int32_t y, const char *s, uint8_t c, uint8_t o)
{
    px_text(x - 1, y, s, o);
    px_text(x + 1, y, s, o);
    px_text(x, y - 1, s, o);
    px_text(x, y + 1, s, o);
    px_text(x, y, s, c);
}
static void px_text_c(int32_t cx, int32_t y, const char *s, uint8_t c, uint8_t o)
{
    px_text_o(cx - px_text_w(s) / 2, y, s, c, o);
}

/* to the panel: 16 rows of pixels at a time, doubled into gfx.c's canvas (240 x 32), one blit each */
#define PX_BAND 16
static void px_present(void)
{
    int32_t b, y, x;
    for (b = 0; b < PX_H; b += PX_BAND) {
        uint16_t *o;
        lcd_sync();                                     /* the last band's transfer may still read the canvas */
        o = cv_px;
        for (y = b; y < b + PX_BAND; y++) {
            const uint8_t *row = px_fb + y * PX_W;
            uint16_t *o2 = o + 240;
            for (x = 0; x < PX_W; x++) {
                uint16_t c = px_pal_sw[row[x] < PC_N ? row[x] : 0];
                o[2 * x] = o[2 * x + 1] = o2[2 * x] = o2[2 * x + 1] = c;
            }
            o += 480;
        }
        lcd_blit(0, (uint32_t)(2 * b), 240, 2 * PX_BAND, cv_px);
    }
}
