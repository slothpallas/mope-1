/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1's instrument: see synth.h. The plate reverb and the half-rate delay line are FiMba-1's
 * (FoMni's before it), reshaped. */
#include "synth.h"
#include "fastmath.h"

#ifdef OM_HOST
#define SY_POOL
#else
#define SY_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

/* ------------------------------------------------------------- params --- */
static const sy_param_t PARAMS[P_NPARAMS] = {
    {"Table", 0, SY_NTABLE - 1, 0}, {"Position", 0, 100, 20}, {"Detune", 0, 100, 25}, {"Sub", 0, 100, 30},
    {"Cutoff", 0, 100, 58}, {"Reso", 0, 100, 20}, {"Env amt", 0, 100, 35}, {"Mod decay", 0, 100, 45},
    {"Attack", 0, 100, 10}, {"Decay", 0, 100, 50}, {"Sustain", 0, 100, 60}, {"Release", 0, 100, 50},
    {"LFO rate", 0, 100, 30}, {"LFO depth", 0, 100, 25}, {"LFO shape", 0, LFO_NSHAPE - 1, LFO_SINE},
    {"LFO dest", 0, LD_N - 1, LD_POS},
    {"Chorus", 0, 100, 30}, {"Delay", 0, 100, 22}, {"Time", 0, 100, 50}, {"Feedback", 0, 100, 45},
    {"Reverb", 0, 100, 38}, {"Size", 0, 100, 62}, {"Fry", 0, 100, 0}, {"Tape", 0, 100, 35},
    {"Style", 0, ST_N - 1, ST_FUNERAL}, {"Tempo", 50, 200, 84}, {"Drums", 0, 100, 70}, {"Bass", 0, 100, 60},
    {"Glide", 0, 100, 0}, {"Octave", -3, 3, 0}, {"MIDI ch", 0, 16, 0}, {"MIDI out", 0, 1, 0},
    {"Tune", -100, 100, 0}, {"Volume", 0, 100, 80},
    {"Voice", 0, 100, 80}, {"Gunk", 0, 100, 30}, {"Swing", 0, 100, 0}, {"Human", 0, 100, 15},
    {"Arp mode", 0, ARP_NMODE - 1, ARP_UP}, {"Arp rate", 0, ARP_NRATE - 1, 3}, {"Arp oct", 1, 3, 1},
    {"Arp gate", 5, 100, 55},
};
const sy_param_t *sy_param_info(int i) { return &PARAMS[i < 0 ? 0 : i >= P_NPARAMS ? P_NPARAMS - 1 : i]; }

const char *const SY_TABLE_NAME[SY_NTABLE] = {"SIGH", "MOAN", "PULSE", "CRUSH", "WARBLE", "FOLD", "CHURCH", "SYNC"};

volatile sy_tel_t sy_tel;

/* ----------------------------------------------------------- commands --- */
enum { C_SET, C_ON, C_OFF, C_BEND, C_MOD, C_DRUMS, C_ARP, C_PANIC };
typedef struct {
    uint8_t c;
    int16_t a, b;
} cmd_t;
#define QN 128u
static cmd_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(int c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN) {
        sy_tel.dropped++;
        return;
    }
    q[w % QN].c = (uint8_t)c;
    q[w % QN].a = (int16_t)a;
    q[w % QN].b = (int16_t)b;
    BARRIER();
    q_w = w + 1;
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
void sy_set(int p, int v) { post(C_SET, p, v); }
void sy_note_on(int note, int vel) { post(C_ON, clampi(note, 0, 127), clampi(vel, 1, 127)); }
void sy_note_off(int note) { post(C_OFF, clampi(note, 0, 127), 0); }
void sy_bend(int v) { post(C_BEND, clampi(v, -8192, 8191), 0); }
void sy_mod(int v) { post(C_MOD, clampi(v, 0, 127), 0); }
void sy_drums(int on) { post(C_DRUMS, on, 0); }
void sy_arp(int on) { post(C_ARP, on, 0); }
void sy_panic(void) { post(C_PANIC, 0, 0); }

static int16_t par[P_NPARAMS];
static uint32_t rng = 0x2545F491u;
static inline uint32_t rnd(void)
{
    uint32_t x = rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng = x;
}
static inline float rndf(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static inline float rnds(void) { return rndf() * 2.0f - 1.0f; }
static inline float pf(int p) { return (float)par[p] * 0.01f; }        /* a 0..100 value as 0..1 */
/* a time knob: 1 ms .. ~8 s, doubling every 7.7 steps */
static inline float knob_s(int p) { return 0.001f * fm_exp2f(pf(p) * 13.0f); }
static inline float coef_s(float s) { return 1.0f - fm_expf(-1.0f / (s * SY_SR)); }

/* ------------------------------------------------------------- tables --- */
/* Each frame is drawn in the time domain (1024 points), taken apart into its harmonics (a DFT), and
 * rebuilt band-limited at five sizes. Five lookups of one frame are each other's mip levels: the
 * voice picks the one whose top harmonic stays under Nyquist for its pitch. int16: 64 KiB in all. */
#define SRC_N 1024
#define NMIP 5
static const uint16_t MIP_LEN[NMIP] = {256, 128, 64, 32, 32};
static const uint8_t MIP_H[NMIP] = {96, 48, 24, 12, 5};
static const uint16_t MIP_OFF[NMIP] = {0, 256, 384, 448, 480};
#define FRAME_N 512                                     /* the five, one after the other */
static int16_t wt[SY_NTABLE][SY_NFRAME][FRAME_N] SY_POOL;
static float sintab[SRC_N] SY_POOL;
static float src[SRC_N] SY_POOL;
#define WT_S (1.0f / 32767.0f)

static inline float st(int i) { return sintab[i & (SRC_N - 1)]; }
static inline float tri(float x) { x -= fm_floorf(x); return x < 0.5f ? 4.0f * x - 1.0f : 3.0f - 4.0f * x; }
static inline float saw(float x) { x -= fm_floorf(x); return 2.0f * x - 1.0f; }

/* frame f (0..1) of table t at x (0..1): the recipes */
static float recipe(int t, float m, float x)
{
    float s = fm_sin_turns(x), y;
    switch (t) {
    case 0: {                                           /* SIGH: a sine leaning into a soft saw */
        float sw = 0.0f;
        int h;
        for (h = 1; h <= 24; h++)
            sw += fm_sin_turns(x * (float)h) / (float)h * (h & 1 ? 1.0f : 0.7f);
        return s * (1.0f - m) + sw * 0.6f * m + 0.25f * m * (1.0f - m) * tri(x);
    }
    case 1: {                                           /* MOAN: a voice from "oo" to "ah" to "ee" */
        static const float F1[3] = {300.0f, 750.0f, 280.0f}, F2[3] = {870.0f, 1150.0f, 2250.0f};
        static float last_m = -1.0f, amp[61];           /* the vowel's harmonics: once a frame, not a sample */
        float sum = 0.0f;
        int h;
        if (m != last_m) {
            float k = m * 2.0f, f1, f2;
            int i = k >= 1.0f ? 1 : 0;
            k -= (float)i;
            f1 = F1[i] + (F1[i + 1] - F1[i]) * k;
            f2 = F2[i] + (F2[i + 1] - F2[i]) * k;
            for (h = 1; h <= 60; h++) {
                float fr = 110.0f * (float)h, a1 = (fr - f1) / 120.0f, a2 = (fr - f2) / 180.0f;
                amp[h] = fm_expf(-a1 * a1) + 0.6f * fm_expf(-a2 * a2) + 0.15f / (float)h;
            }
            last_m = m;
        }
        for (h = 1; h <= 60; h++)
            sum += amp[h] * fm_sin_turns(x * (float)h);
        return sum;
    }
    case 2: {                                           /* PULSE: saw, then a square, then a thin pulse */
        float w = 0.5f + 0.45f * m;
        return m < 0.25f ? saw(x) * (1.0f - 4.0f * m) + (saw(x) - saw(x + 0.5f)) * 4.0f * m * 0.5f
                         : 0.5f * (saw(x) - saw(x + w));
    }
    case 3: {                                           /* CRUSH: a sine on fewer and fewer steps */
        float lv = 2.0f + 30.0f * (1.0f - m) * (1.0f - m), hold = 64.0f - 56.0f * m;
        float xq = fm_floorf(x * hold) / hold, v = fm_sin_turns(xq) + 0.3f * m * fm_sin_turns(3.0f * xq);
        return fm_floorf(v * lv + 0.5f) / lv;
    }
    case 4: {                                           /* WARBLE: a sine off a stretched tape, saturated */
        float wx = x + 0.08f * m * fm_sin_turns(x) + 0.03f * m * fm_sin_turns(3.0f * x);
        y = fm_sin_turns(wx) + 0.35f * m * fm_sin_turns(2.0f * wx);
        return fm_tanhf(y * (1.0f + 2.5f * m));
    }
    case 5: {                                           /* FOLD: a sine through a wavefolder */
        float g = 1.0f + 5.0f * m;
        return fm_sin_pi(fm_wrap_pi(s * g * 1.5707963f));
    }
    case 6: {                                           /* CHURCH: drawbars, pulled out one by one */
        static const float H[7] = {1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f};
        static const float A0[7] = {1.0f, 0.5f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f};
        static const float A1[7] = {0.7f, 0.8f, 0.6f, 0.7f, 0.5f, 0.6f, 0.4f};
        int i;
        for (y = 0.0f, i = 0; i < 7; i++)
            y += (A0[i] + (A1[i] - A0[i]) * m) * fm_sin_turns(x * H[i]);
        return y;
    }
    default: {                                          /* SYNC: a saw hard-synced, the ratio rising */
        float r = 1.0f + 5.0f * m, p = x * r;
        return saw(p) * (1.0f - 0.5f * x);
    }
    }
}

void (*sy_boot_tick)(void);                             /* called now and then while the tables build */
static void tables_build(void)
{
    int t, f, i, h, m;
    static float ha[97], hb[97];
    for (i = 0; i < SRC_N; i++)
        sintab[i] = fm_sin_turns((float)i / (float)SRC_N);
    for (t = 0; t < SY_NTABLE; t++)
        for (f = 0; f < SY_NFRAME; f++) {
            float pk = 0.0f, g;
            const float mm = (float)f / (float)(SY_NFRAME - 1);
            if (sy_boot_tick)
                sy_boot_tick();                         /* (the device's watchdog) */
            for (i = 0; i < SRC_N; i++)
                src[i] = recipe(t, mm, (float)i / (float)SRC_N);
            for (h = 1; h <= 96; h++) {                 /* the harmonics (the DC is dropped) */
                float a = 0.0f, b = 0.0f;
                for (i = 0; i < SRC_N; i++) {
                    a += src[i] * st(h * i);
                    b += src[i] * st(h * i + SRC_N / 4);
                }
                ha[h] = a * (2.0f / (float)SRC_N);
                hb[h] = b * (2.0f / (float)SRC_N);
            }
            for (m = 0; m < NMIP; m++) {
                const int n = MIP_LEN[m], step = SRC_N / n;
                float tmp[256];
                for (i = 0; i < n; i++) {
                    float v = 0.0f;
                    for (h = 1; h <= MIP_H[m]; h++) {
                        /* the top few fade out: no edge in the spectrum, no ringing */
                        float w = h > MIP_H[m] - 3 ? 0.25f * (float)(MIP_H[m] - h + 1) : 1.0f;
                        v += w * (ha[h] * st(h * i * step) + hb[h] * st(h * i * step + SRC_N / 4));
                    }
                    tmp[i] = v;
                    if (m == 0 && fm_fabsf(v) > pk)
                        pk = fm_fabsf(v);
                }
                if (m == 0)
                    pk = pk > 1e-6f ? pk : 1.0f;
                g = 30000.0f / pk;
                for (i = 0; i < n; i++) {
                    float v = fm_clampf(tmp[i] * g, -32767.0f, 32767.0f);
                    wt[t][f][MIP_OFF[m] + i] = (int16_t)(v < 0.0f ? v - 0.5f : v + 0.5f);
                }
            }
        }
}

/* one table read: frames f0 and f0+1 mixed by fr, at phase ph, mip m */
static inline float wt_read(const int16_t *a, const int16_t *b, float fr, uint32_t len, float ph)
{
    float x = ph * (float)len, k;
    uint32_t i = (uint32_t)x, j;
    k = x - (float)i;
    i &= len - 1u;
    j = (i + 1u) & (len - 1u);
    {
        float va = (float)a[i] + ((float)a[j] - (float)a[i]) * k;
        float vb = (float)b[i] + ((float)b[j] - (float)b[i]) * k;
        return (va + (vb - va) * fr) * WT_S;
    }
}

float sy_shape(int table, float pos, float phase)
{
    float p = fm_clampf(pos, 0.0f, 1.0f) * (float)(SY_NFRAME - 1), fr;
    int f = (int)p;
    if (f >= SY_NFRAME - 1)
        f = SY_NFRAME - 2;
    fr = p - (float)f;
    table = table < 0 ? 0 : table >= SY_NTABLE ? SY_NTABLE - 1 : table;
    phase -= fm_floorf(phase);
    return wt_read(wt[table][f], wt[table][f + 1], fr, 256u, phase);
}

/* ------------------------------------------------------------- voices --- */
enum { ST_OFF, ST_ATT, ST_DEC, ST_SUS, ST_REL };
typedef struct {
    uint8_t stage, gate, note;
    float env, menv, vel;
    float ph[2], phsub;
    float cents, cents_to;                             /* now, and where the glide goes */
    float drift, drift_to;
    float s1[2], s2[2];                                /* the filter, per oscillator (they pan apart) */
    uint32_t age;
} voice_t;
static voice_t vc[SY_NVOICE];
static uint32_t age_n;
static float bend_c, mod_amt;

static int voice_find(int note)
{
    int i, best = -1;
    uint32_t oldest = 0xFFFFFFFFu;
    for (i = 0; i < SY_NVOICE; i++)                    /* the same note again: its voice */
        if (vc[i].stage != ST_OFF && vc[i].note == note)
            return i;
    for (i = 0; i < SY_NVOICE; i++)
        if (vc[i].stage == ST_OFF)
            return i;
    for (i = 0; i < SY_NVOICE; i++)                    /* the oldest one let go */
        if (!vc[i].gate && vc[i].age < oldest) {
            oldest = vc[i].age;
            best = i;
        }
    if (best < 0)
        for (i = 0; i < SY_NVOICE; i++)                /* the oldest of all */
            if (vc[i].age < oldest) {
                oldest = vc[i].age;
                best = i;
            }
    sy_tel.steals++;
    return best;
}

static void note_on(int note, int vel)
{
    int i = voice_find(note);
    voice_t *v = &vc[i];
    float c = (float)note * 100.0f;
    if (v->stage == ST_OFF) {                          /* a fresh voice: from silence, no glide */
        v->cents = c;
        v->ph[0] = rndf();
        v->ph[1] = rndf();
        v->phsub = 0.0f;
        v->s1[0] = v->s1[1] = v->s2[0] = v->s2[1] = 0.0f;
        v->drift = v->drift_to = 0.0f;
    } else if (par[P_GLIDE] == 0) {
        v->cents = c;
    }
    /* a stolen or repeated voice keeps its level: the attack starts from where it is (no click) */
    v->cents_to = c;
    v->note = (uint8_t)note;
    v->vel = 0.35f + 0.65f * (float)vel / 127.0f;
    v->gate = 1;
    v->stage = ST_ATT;
    v->menv = v->menv > 0.0f ? v->menv : 0.0f;
    v->age = ++age_n;
    sy_tel.notes++;
    sy_tel.last_note = (int16_t)note;
}

static void note_off(int note)
{
    int i;
    for (i = 0; i < SY_NVOICE; i++)
        if (vc[i].gate && vc[i].note == note) {
            vc[i].gate = 0;
            vc[i].stage = ST_REL;
        }
}

/* ---------------------------------------------------------- arpeggiator --- */
/* While it is on, held notes don't sound: they become its pattern, low to high, over 1-3 octaves, and it
 * plays them one at a time at the tempo (or all at once, STAB), each for Gate of a step. With the drums
 * playing it starts each bar with them. */
static const float ARP_DIV[ARP_NRATE] = {1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};   /* steps a beat */
static uint8_t arp_on, arp_held[16], arp_nh, arp_vel = 90;
static int16_t arp_cur = -1;                            /* sounding: a note, or -2 the STAB chord */
static float arp_t, arp_len = 1.0f;
static uint32_t arp_seq;

static void arp_add(int n)
{
    int i, j;
    for (i = 0; i < arp_nh; i++)
        if (arp_held[i] == n)
            return;
    if (arp_nh >= 16)
        return;
    for (i = 0; i < arp_nh && arp_held[i] < n; i++)
        ;
    for (j = arp_nh; j > i; j--)
        arp_held[j] = arp_held[j - 1];
    arp_held[i] = (uint8_t)n;
    if (!arp_nh) {                                      /* the first: from the top of the pattern, now */
        arp_seq = 0;
        arp_t = arp_len;
    }
    arp_nh++;
}
static void arp_remove(int n)
{
    int i, j;
    for (i = 0; i < arp_nh; i++)
        if (arp_held[i] == n) {
            for (j = i; j + 1 < arp_nh; j++)
                arp_held[j] = arp_held[j + 1];
            arp_nh--;
            return;
        }
}
int sy_arp_held(uint8_t *out, int max)
{
    int i, n = arp_nh < max ? arp_nh : max;
    for (i = 0; i < n; i++)
        out[i] = arp_held[i];
    return n;
}
/* the index into held x octaves at step seq (the same for the screen as for the sound, but WANDER) */
static int arp_index(int total, uint32_t seq)
{
    const int mode = par[P_ARPMODE];
    if (total <= 1)
        return 0;
    if (mode == ARP_DOWN)
        return total - 1 - (int)(seq % (uint32_t)total);
    if (mode == ARP_UPDOWN) {
        const uint32_t per = (uint32_t)(2 * total - 2), p = seq % per;
        return p < (uint32_t)total ? (int)p : (int)(per - p);
    }
    if (mode == ARP_WANDER)
        return (int)((seq * 2654435761u >> 7) % (uint32_t)total);   /* (a fixed walk: the screen can draw it) */
    return (int)(seq % (uint32_t)total);
}
int sy_arp_note(int held_n, int seq)
{
    const int oct = par[P_ARPOCT], total = held_n * (par[P_ARPMODE] == ARP_STAB ? 1 : oct);
    int k;
    if (held_n <= 0 || held_n > arp_nh)
        return -1;
    if (par[P_ARPMODE] == ARP_STAB)
        return arp_held[0];
    k = arp_index(total, (uint32_t)seq);
    return arp_held[k % held_n] + 12 * (k / held_n);
}
static void arp_release(void)
{
    int i;
    if (arp_cur == -2) {
        for (i = 0; i < arp_nh; i++)
            note_off(arp_held[i]);
    } else if (arp_cur >= 0) {
        note_off(arp_cur);
    }
    arp_cur = -1;
    sy_tel.arp_note = -1;
}
static void arp_tick(void)                             /* one sample */
{
    if (!arp_on)
        return;
    arp_len = SY_SR * 60.0f / (float)par[P_TEMPO] / ARP_DIV[par[P_ARPRATE]];
    if (!arp_nh) {
        if (arp_cur != -1)
            arp_release();
        return;
    }
    arp_t += 1.0f;
    if (arp_cur != -1 && arp_t >= arp_len * pf(P_ARPGATE))
        arp_release();
    if (arp_t >= arp_len) {
        int i;
        arp_t -= arp_len;
        if (arp_t > arp_len)
            arp_t = 0.0f;
        if (arp_cur != -1)
            arp_release();
        if (par[P_ARPMODE] == ARP_STAB) {
            for (i = 0; i < arp_nh; i++)
                note_on(arp_held[i], arp_vel);
            arp_cur = -2;
            sy_tel.arp_note = arp_held[0];
        } else {
            const int n = sy_arp_note(arp_nh, (int)arp_seq);
            note_on(n > 127 ? 127 : n, arp_seq % (uint32_t)(arp_nh * par[P_ARPOCT]) == 0u ? 110 : arp_vel);
            arp_cur = (int16_t)(n > 127 ? 127 : n);
            sy_tel.arp_note = arp_cur;
        }
        arp_seq++;
        sy_tel.arp_seq = arp_seq;
    }
}
static void arp_set(int on)
{
    int i;
    if (on && !arp_on) {                                /* what is held now becomes the pattern */
        arp_nh = 0;
        for (i = 0; i < SY_NVOICE; i++)
            if (vc[i].gate) {
                arp_add(vc[i].note);
                note_off(vc[i].note);
            }
        arp_on = 1;
        arp_t = arp_len;
    } else if (!on && arp_on) {                         /* and back: what is held sounds again */
        arp_release();
        arp_on = 0;
        for (i = 0; i < arp_nh; i++)
            note_on(arp_held[i], arp_vel);
        arp_nh = 0;
    }
    sy_tel.arp_on = arp_on;
}

/* ----------------------------------------------------------------- gunk --- */
/* The room's texture: rain on the window (filtered noise and drops), a record's crackle, the room's
 * rumble. It swells when nothing plays and steps back when something does. */
static float gk_idle, gk_rl, gk_rr, gk_hl, gk_hr, gk_rum, gk_crk, gk_drop, gk_dph, gk_dhz;
static void gunk_chunk(void)
{
    const float idle = sy_tel.voices || sy_tel.drums_on ? 0.0f : 1.0f;
    gk_idle += (idle - gk_idle) * (idle > gk_idle ? 0.002f : 0.03f);   /* slow back in, quick out */
}
static inline void gunk(float *l, float *r, float *peak)
{
    const float a = pf(P_GUNK);
    float g, nl, nr, x;
    if (a <= 0.0f)
        return;
    g = a * a * (0.25f + 0.75f * gk_idle) * 0.12f;
    nl = rnds();
    nr = rnds();
    gk_rl += 0.25f * (nl - gk_rl);                      /* rain: soft noise, its top and bottom cut */
    gk_rr += 0.25f * (nr - gk_rr);
    gk_hl += 0.02f * (gk_rl - gk_hl);
    gk_hr += 0.02f * (gk_rr - gk_hr);
    gk_rum += 0.0015f * (nl - gk_rum);                  /* the room: a low rumble */
    if ((rnd() & 4095u) < 3u)                           /* crackle */
        gk_crk = rnds() * 0.9f;
    gk_crk *= 0.85f;
    if ((rnd() & 8191u) < 2u) {                         /* a drop on the glass: a short high ping */
        gk_drop = 0.5f + 0.5f * rndf();
        gk_dhz = (2200.0f + 2600.0f * rndf()) / SY_SR;
    }
    gk_dph += gk_dhz;
    gk_dph -= (float)(int)gk_dph;
    gk_drop *= 0.9985f;
    x = gk_crk + gk_drop * 0.18f * fm_sin_turns(gk_dph) + gk_rum * 4.0f;
    nl = ((gk_rl - gk_hl) * 0.55f + x) * g;
    nr = ((gk_rr - gk_hr) * 0.55f + x) * g;
    *l += nl;
    *r += nr;
    x = fm_fabsf(nl) + fm_fabsf(nr);
    *peak = x > *peak ? x : *peak;
}

/* ---------------------------------------------------------------- LFO --- */
static float lfo_ph, lfo_val, lfo_rand, lfo_rand_to;
static float lfo_tick(uint32_t n)
{
    const float hz = 0.05f * fm_exp2f(pf(P_LFORATE) * 9.0f);
    float v;
    lfo_ph += hz * (float)n / SY_SR;
    if (lfo_ph >= 1.0f) {
        lfo_ph -= fm_floorf(lfo_ph);
        lfo_rand_to = rnds();
    }
    switch (par[P_LFOSHAPE]) {
    case LFO_TRI: v = tri(lfo_ph + 0.25f); break;
    case LFO_SQUARE: v = lfo_ph < 0.5f ? 1.0f : -1.0f; break;
    case LFO_SAW: v = 1.0f - 2.0f * lfo_ph; break;
    case LFO_RANDOM:
        lfo_rand += 0.3f * (lfo_rand_to - lfo_rand);
        v = lfo_rand;
        break;
    default: v = fm_sin_turns(lfo_ph); break;
    }
    lfo_val += 0.5f * (v - lfo_val);                   /* a square's edges, not quite a click */
    return lfo_val;
}

/* ------------------------------------------------------------ render: voices --- */
#define SUB 16                                         /* control rate: every 16 frames */
static float vbl[SUB], vbr[SUB];

static void render_voices(float *ol, float *or_, uint32_t n, float lfo)
{
    const float depth = pf(P_LFODEPTH), dest_pos = par[P_LFODEST] == LD_POS ? lfo * depth * 0.5f : 0.0f;
    const float dest_cut = par[P_LFODEST] == LD_CUTOFF ? lfo * depth * 4.0f : 0.0f;
    const float dest_pitch = par[P_LFODEST] == LD_PITCH ? lfo * depth * 100.0f : 0.0f;
    const float dest_vol = par[P_LFODEST] == LD_VOLUME ? 1.0f - depth * 0.5f * (1.0f + lfo) : 1.0f;
    const float att = 1.0f / (knob_s(P_ATTACK) * SY_SR), dec = coef_s(knob_s(P_DECAY) * 0.3f);
    const float rel = coef_s(knob_s(P_RELEASE) * 0.3f), sus = pf(P_SUSTAIN);
    const float mdec = coef_s(knob_s(P_MODDEC) * 0.3f);
    const float det = pf(P_DETUNE) * pf(P_DETUNE) * 40.0f + 2.0f;   /* cents apart, each way */
    const float spread = 0.15f + 0.35f * pf(P_DETUNE), sub = pf(P_SUB) * 0.8f;
    const float k = 2.0f - 1.94f * pf(P_RESO);          /* the SVF's damping: 1/Q */
    const float glide = par[P_GLIDE] ? 1.0f - fm_expf(-(float)SUB / (0.002f * fm_exp2f(pf(P_GLIDE) * 9.0f) * SY_SR)) : 1.0f;
    const int tbl = par[P_TABLE];
    float pos = fm_clampf(pf(P_POS) + dest_pos + mod_amt, 0.0f, 1.0f) * (float)(SY_NFRAME - 1);
    int f0 = (int)pos, i;
    float fr, amp_max = 0.0f, menv_max = 0.0f;
    uint8_t nv = 0, ng = 0, stage = ST_OFF;
    if (f0 >= SY_NFRAME - 1)
        f0 = SY_NFRAME - 2;
    fr = pos - (float)f0;
    for (i = 0; i < SY_NVOICE; i++) {
        voice_t *v = &vc[i];
        uint32_t j0;
        if (v->stage == ST_OFF)
            continue;
        nv++;
        ng += v->gate;
        for (j0 = 0; j0 < n; j0 += SUB) {
            float c, hz, inc[2], incs, cut, g, a1, a2, a3;
            uint32_t j, m, len;
            const int16_t *ta, *tb;
            /* pitch: the glide, a slow drift (an old synth's), the bend, the LFO */
            v->cents += (v->cents_to - v->cents) * glide;
            if ((rnd() & 255u) == 0u)
                v->drift_to = rnds() * 4.0f;
            v->drift += 0.002f * (v->drift_to - v->drift);
            c = v->cents + v->drift + bend_c + dest_pitch + (float)par[P_TUNE];
            hz = 8.1757989f * fm_exp2f(c * (1.0f / 1200.0f));
            for (m = 0; m < NMIP - 1u && (float)MIP_H[m] * hz * fm_exp2f(det / 1200.0f) > 21000.0f; m++)
                ;
            len = MIP_LEN[m];
            ta = wt[tbl][f0] + MIP_OFF[m];
            tb = wt[tbl][f0 + 1] + MIP_OFF[m];
            inc[0] = hz * fm_exp2f(-det / 1200.0f) / SY_SR;
            inc[1] = hz * fm_exp2f(det / 1200.0f) / SY_SR;
            incs = hz * 0.5f / SY_SR;
            /* the cutoff: the knob, the mod envelope, the LFO, a little of the key */
            cut = 30.0f * fm_exp2f(pf(P_CUTOFF) * 9.3f + pf(P_ENVAMT) * 6.0f * v->menv + dest_cut +
                                   0.5f * (v->cents - 6000.0f) / 1200.0f);
            cut = fm_clampf(cut, 20.0f, 18000.0f);
            g = fm_tanf(FM_PI * cut / SY_SR);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
            for (j = 0; j < SUB; j++) {
                float e = v->env, o0, o1, s, y[2];
                int q;
                switch (v->stage) {                     /* the amp envelope, a sample at a time */
                case ST_ATT:
                    e += att;
                    if (e >= 1.0f) {
                        e = 1.0f;
                        v->stage = ST_DEC;
                    }
                    break;
                case ST_DEC:
                    e += (sus - e) * dec;
                    if (e - sus < 0.001f)
                        v->stage = ST_SUS;
                    break;
                case ST_SUS:
                    e += (sus - e) * 0.001f;
                    break;
                case ST_REL:
                    e -= e * rel;
                    break;
                }
                v->env = e;
                o0 = wt_read(ta, tb, fr, len, v->ph[0]);
                o1 = wt_read(ta, tb, fr, len, v->ph[1]);
                v->ph[0] += inc[0];
                v->ph[1] += inc[1];
                v->ph[0] -= (float)(int)v->ph[0];
                v->ph[1] -= (float)(int)v->ph[1];
                v->phsub += incs;
                v->phsub -= (float)(int)v->phsub;
                s = fm_sin_turns(v->phsub) * sub;
                {
                    const float in[2] = {o0 + s, o1 + s};
                    for (q = 0; q < 2; q++) {           /* Simper's SVF, low-pass */
                        float v3 = in[q] - v->s2[q], v1 = a1 * v->s1[q] + a2 * v3, v2 = v->s2[q] + a2 * v->s1[q] + a3 * v3;
                        v->s1[q] = 2.0f * v1 - v->s1[q];
                        v->s2[q] = 2.0f * v2 - v->s2[q];
                        y[q] = v2;
                    }
                }
                e = e * e * v->vel * dest_vol * 0.32f;  /* (squared: the envelope's end is soft) */
                vbl[j] = (y[0] * (0.5f + spread) + y[1] * (0.5f - spread)) * e;
                vbr[j] = (y[1] * (0.5f + spread) + y[0] * (0.5f - spread)) * e;
            }
            for (j = 0; j < SUB; j++) {
                ol[j0 + j] += vbl[j];
                or_[j0 + j] += vbr[j];
            }
            /* the mod envelope: up with the attack, down with Mod decay */
            if (v->stage == ST_ATT)
                v->menv += (1.0f - v->menv) * fm_clampf(att * (float)SUB, 0.0f, 1.0f);
            else
                v->menv -= v->menv * fm_clampf(mdec * (float)SUB, 0.0f, 1.0f);
            v->s1[0] = fm_flush(v->s1[0]);
            v->s1[1] = fm_flush(v->s1[1]);
            v->s2[0] = fm_flush(v->s2[0]);
            v->s2[1] = fm_flush(v->s2[1]);
        }
        if (v->stage == ST_REL && v->env < 0.0005f) {
            v->stage = ST_OFF;
            v->env = 0.0f;
        }
        if (v->env >= amp_max) {
            amp_max = v->env;
            stage = v->stage;
        }
        if (v->menv > menv_max)
            menv_max = v->menv;
    }
    sy_tel.amp_env = amp_max;
    sy_tel.amp_stage = stage;
    sy_tel.mod_env = menv_max;
    sy_tel.voices = nv;
    sy_tel.gates = ng;
}

/* ---------------------------------------------------------------- fry --- */
/* Drive into a tanh; past 55 the bits and the sample rate go too: deep-fried */
/* Level-matched: the drive's gain is taken back out as much as a typical level (0.3) would gain, so
 * more Fry is more grit, not more loud. It glides (a preset change doesn't slam it in). */
static float fry_hold_l, fry_hold_r, fry_cnt, fry_now, fry_g = 1.0f, fry_comp = 1.0f;
static void fry_chunk(void)                            /* every 16 frames */
{
    fry_now += (pf(P_FRY) - fry_now) * 0.012f;
    if (fry_now < 1e-4f && par[P_FRY] == 0)
        fry_now = 0.0f;
    fry_g = 1.0f + fry_now * fry_now * 24.0f;
    fry_comp = 0.3f / fm_tanhf(0.3f * fry_g) / (1.0f + 1.86f * fry_now * fry_now);   /* (squashed is louder: fitted) */
}
static inline void fry(float *l, float *r)
{
    const float a = fry_now, g = fry_g, comp = fry_comp;
    float yl, yr;
    if (a <= 0.0f)
        return;
    yl = fm_tanhf(*l * g + 0.08f * a);                  /* (-1..1: the crush works on the full swing) */
    yr = fm_tanhf(*r * g + 0.08f * a);
    if (a > 0.55f) {
        const float t = (a - 0.55f) / 0.45f, lv = 6.0f + 400.0f * (1.0f - t) * (1.0f - t), hold = 1.0f + 7.0f * t;
        if ((fry_cnt += 1.0f) >= hold) {
            fry_cnt -= hold;
            fry_hold_l = fm_floorf(yl * lv + 0.5f) / lv;
            fry_hold_r = fm_floorf(yr * lv + 0.5f) / lv;
        }
        yl = fry_hold_l;
        yr = fry_hold_r;
    }
    *l = yl * comp;
    *r = yr * comp;
}

/* ------------------------------------------------------------- chorus --- */
/* three taps a side wandering around 10-20 ms: more of the same blob, slightly late */
#define CH_N 1024u
static float ch_buf[CH_N] SY_POOL;
static uint32_t ch_w;
static float ch_ph;
static inline float line_read(const float *b, uint32_t mask, uint32_t w, float back)
{
    uint32_t ib = (uint32_t)back, i0 = (w - ib) & mask, i1 = (i0 - 1u) & mask;
    float f = back - (float)ib;
    return b[i0] + (b[i1] - b[i0]) * f;
}
static inline void chorus(float *l, float *r, float amt, const float *d)
{
    float m = (*l + *r) * 0.5f, tl, tr;
    ch_buf[ch_w] = m;
    ch_w = (ch_w + 1u) & (CH_N - 1u);
    if (amt <= 0.0f)
        return;
    tl = line_read(ch_buf, CH_N - 1u, ch_w, d[0]) + line_read(ch_buf, CH_N - 1u, ch_w, d[2]) * 0.7f;
    tr = line_read(ch_buf, CH_N - 1u, ch_w, d[1]) + line_read(ch_buf, CH_N - 1u, ch_w, d[3]) * 0.7f;
    *l = *l * (1.0f - 0.3f * amt) + tl * amt * 0.6f;
    *r = *r * (1.0f - 0.3f * amt) + tr * amt * 0.6f;
}

/* -------------------------------------------------------------- delay --- */
/* One int16 line at 22.05 kHz, two taps: echoes left, right, left. Its time wobbles with Tape, its
 * loop gets darker each time round. (FiMba-1's line.) */
#define DLY_N 32768u
static int16_t dly_buf[DLY_N] SY_POOL;
static uint32_t dw;
static float d_t = 8000.0f, d_lp, d_l, d_r, d_pl, d_pr, d_in, d_wob, d_peak;
#define LINE_IN 0.3f
static inline int16_t to16(float x)
{
    x = fm_tanhf(x * LINE_IN) * 32767.0f;
    return (int16_t)(x < 0.0f ? x - 0.5f : x + 0.5f);
}
static inline float dly_read(float back)
{
    uint32_t ib = (uint32_t)back, i0 = (dw - ib) & (DLY_N - 1u), i1 = (i0 - 1u) & (DLY_N - 1u);
    float f = back - (float)ib;
    return ((float)dly_buf[i0] + ((float)dly_buf[i1] - (float)dly_buf[i0]) * f) * (1.0f / (32767.0f * LINE_IN));
}
static int d_half;
static inline void delay(float in, float *l, float *r)
{
    if (!d_half) {                                      /* one step every other sample */
        const float fb = pf(P_FEEDBACK) * 0.95f, tt = (60.0f + 640.0f * pf(P_TIME)) * 22.05f;
        float a, b;
        d_t += 0.0008f * (tt - d_t);
        a = dly_read(d_t * (1.0f + d_wob));
        b = dly_read(2.0f * d_t * (1.0f + d_wob));
        d_lp += 0.4f * (b - d_lp);
        dly_buf[dw] = to16(d_in * 0.5f + in * 0.5f + fb * fb * d_lp);
        dw = (dw + 1u) & (DLY_N - 1u);
        d_pl = d_l;
        d_pr = d_r;
        d_l = a;
        d_r = b * fb;
        d_in = in;
    } else {
        d_in = in;
    }
    d_half ^= 1;
    *l = d_half ? d_pl + (d_l - d_pl) * 0.5f : d_l;    /* (between steps: halfway) */
    *r = d_half ? d_pr + (d_r - d_pr) * 0.5f : d_r;
    {
        float a = fm_fabsf(*l) + fm_fabsf(*r);
        d_peak = a > d_peak ? a : d_peak * 0.9999f;
    }
}

/* ------------------------------------------------------------- reverb --- */
/* Dattorro's plate (JAES 1997), as FiMba-1 and FoMni have it; Size sets the tank's decay and its
 * damping (a closet is dull and short, the void is long and bright). */
#define DS(n) (((n) * 14818 + 5000) / 10000)
#define PRE_N 442
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] SY_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_damp = 0.6f, pl_s, pl_c = 1.0f, pl_peak;

#ifdef OM_HOST
/* the device's .pool is 0x54000 bytes and the build keeps 8 KiB spare; the screen takes 120 x 120 +
 * 240 x 24 x 2 (blob.c, gfx.c). Checked here because the device build cannot run where the tests do. */
_Static_assert(sizeof pl_buf + sizeof dly_buf + sizeof wt + sizeof sintab + sizeof src + sizeof ch_buf +
                   2u * 512u * 4u + 120u * 120u + 240u * 24u * 2u <= 0x54000u - 8192u,
               "the .pool would overflow");
#endif

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.7f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = pl_s + 0.000128f * pl_c, c = pl_c - 0.000128f * s;
        pl_s = s;
        pl_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * pl_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += pl_damp * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, 0.5f);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * pl_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += pl_damp * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, 0.5f);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}
static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
}

/* --------------------------------------------------------------- tape --- */
/* the whole mix through a short line whose length wanders (wow 0.5 Hz, flutter 6.5 Hz), a softer top
 * and saturation, as Tape rises */
#define TP_N 512u
static float tp_l[TP_N] SY_POOL, tp_r[TP_N] SY_POOL;
static uint32_t tp_w;
static float wow_ph, flut_ph, tp_lpl, tp_lpr;

/* -------------------------------------------------------------- drums --- */
/* A style: a bar of steps (16, or 24: triplets), its fill (every fourth bar), a tempo, a swing, a kit.
 * Tracks are strings, one character a step, spaces ignored; "main|fill". Hits: x accent, o normal,
 * - ghost, . nothing. Bass: 0-9 a b a semitone over the root (+ an octave up), A-L the same sliding
 * into it (^ the octave, sliding), _ held, . let go. */
enum { K_808, K_GABBER, K_SOFT, K_CLUB, K_LOG, K_LOFI, K_NKIT };
enum { PERC_RIM, PERC_COWBELL, PERC_TOM, PERC_SQUEAK };
typedef struct {
    float k_f0, k_f1, k_pdec, k_dec, k_drive;          /* kick: from Hz, to Hz, pitch fall s, decay s, drive */
    float s_tone, s_dec, s_noise;                      /* snare */
    float h_dec, oh_dec;                               /* hats */
    uint8_t perc;
    float p_f, p_dec;
    float b_dec, b_drive, b_glide, b_knock;            /* bass: decay s, drive, glide s, the log drum's knock */
    float crackle;                                     /* a record's surface */
} kit_t;
static const kit_t KITS[K_NKIT] = {
    /* 808: long boomy kick, the bass is the 808 */
    {160.0f, 48.0f, 0.03f, 0.6f, 1.6f, 190.0f, 0.18f, 0.7f, 0.045f, 0.3f, PERC_COWBELL, 560.0f, 0.12f, 1.4f, 2.2f, 0.07f, 0.0f, 0.0f},
    /* gabber: a kick distorted into a scream */
    {260.0f, 55.0f, 0.04f, 0.32f, 9.0f, 220.0f, 0.12f, 0.8f, 0.03f, 0.2f, PERC_RIM, 1700.0f, 0.03f, 0.3f, 4.0f, 0.02f, 0.0f, 0.0f},
    /* soft: brushed, a rim, warm bass */
    {110.0f, 52.0f, 0.02f, 0.28f, 1.1f, 230.0f, 0.12f, 0.45f, 0.03f, 0.22f, PERC_RIM, 1500.0f, 0.04f, 0.5f, 1.2f, 0.04f, 0.0f, 0.0f},
    /* club: tight, punchy, a squeak */
    {190.0f, 50.0f, 0.025f, 0.22f, 2.0f, 210.0f, 0.15f, 0.85f, 0.03f, 0.18f, PERC_SQUEAK, 900.0f, 0.09f, 0.35f, 2.0f, 0.03f, 0.0f, 0.0f},
    /* log: amapiano's log drum is the bass: a knock, a bend, wood */
    {120.0f, 50.0f, 0.02f, 0.3f, 1.3f, 240.0f, 0.1f, 0.5f, 0.025f, 0.15f, PERC_TOM, 220.0f, 0.15f, 0.32f, 1.8f, 0.02f, 1.0f, 0.0f},
    /* lofi: dusty, slow, a record crackling */
    {95.0f, 48.0f, 0.02f, 0.35f, 1.3f, 200.0f, 0.2f, 0.4f, 0.04f, 0.3f, PERC_RIM, 1200.0f, 0.05f, 0.6f, 1.2f, 0.05f, 0.0f, 1.0f},
};

typedef struct {
    const char *name;
    uint8_t bpm, steps, swing, kit;
    const char *kick, *snare, *clap, *hat, *ohat, *perc, *bass;
} style_t;
#define NOTHING ""
static const style_t STYLES[ST_N] = {
    {"GRIEF DRILL", 142, 24, 0, K_808,
     "x..... ...... ...x.. ......|x..... ...... ...x.. ..x...",
     "...... ...... x..... ......|...... ...... x..... ...x..",
     NOTHING,
     "x..x.. x..x.x x..x.. xxx.x.|x..x.. x..x.. xxxxxx x..x..",
     NOTHING,
     "...... ....-. ...... ......",
     "0_____ ______ .....A ___3__|0_____ __.... A_____ ^_____"},
    {"EMPTY CITY POP", 112, 16, 8, K_SOFT,
     "x... ..x. x... ..x.|x... ..x. x.x. x.x.",
     ".... x... .... x...|.... x... .... x-xx",
     NOTHING,
     "o..- o..- o..- o..-",
     "..x. ..x. ..x. ..x.",
     ".... .... ..-. ....",
     "0.+. ..0. 5.+. .5..|0.+. ..0. 5.5. 7.a."},
    {"GLOOM GABBER", 190, 16, 0, K_GABBER,
     "x... x... x... x...|x... x... x.x. xxxx",
     NOTHING,
     ".... .... .... ....|.... .... .... x.x.",
     "..x. ..x. ..x. ..x.",
     NOTHING, NOTHING, NOTHING},
    {"MISSED PHONK CALL", 130, 16, 12, K_808,
     "x... .... ..x. ....|x... .... ..x. .x..",
     ".... x... .... x...",
     NOTHING,
     "x.x. x.x. x.xx x.x.|xxxx x.x. x.x. xxxx",
     NOTHING,
     "x..x ..x. x..x .x..",
     "0___ ____ ..0_ ____|3___ ____ ..2_ __0_"},
    {"JERSEY CRY CLUB", 140, 16, 0, K_CLUB,
     "x... x..x ..x. x.x.|x... x..x ..x. xxxx",
     NOTHING,
     ".... x... .... x...",
     "..x. ..x. ..x. ..x.",
     NOTHING,
     "...- ...- ...- ...-",
     NOTHING},
    {"HEARTBREAKCORE", 172, 16, 0, K_CLUB,
     "x.x. .... ..x. ....|x.x. ...x .x.. x...",
     ".... x..x .x.. x..x|.x.x x.xx -xxx xxxx",
     NOTHING,
     "xxxx xxxx xxxx xxxx",
     NOTHING,
     ".... ...- .... -...",
     "0... .... 0... ....|0... ..0. 0.0. 0000"},
    {"AMAPIA-NO", 113, 16, 10, K_LOG,
     "x... x... x... x...",
     "..-. ..-. ..x. ..-.",
     NOTHING,
     "o-o- o-o- o-o- o-o-",
     NOTHING,
     NOTHING,
     "..0. .0.. 0.+. ..3.|..0. .0.. 0.^. 5.3."},
    {"COLD FOOTWORK", 160, 16, 0, K_CLUB,
     "x..x ..x. .x.. x..x|x..x ..x. .x.x ..x.",
     NOTHING,
     ".... x... .... x...",
     "x.xx x.xx x.xx x.xx",
     NOTHING,
     "..-. .-.. -..- ....",
     "0..0 ..0. .0.. 0..0"},
    {"DEMBOW OUT", 100, 16, 0, K_CLUB,
     "x... x... x... x...",
     "...x ..x. ...x ..x.|...x ..x. ...x xxxx",
     NOTHING,
     "o.o. o.o. o.o. o.o.",
     NOTHING,
     "..-. ..-. ..-. ..-.",
     NOTHING},
    {"VAPORWEEP", 78, 16, 18, K_LOFI,
     "x... .... x.x. ....",
     ".... x... .... x...",
     NOTHING,
     "o.-. o.-. o.-. o.-.",
     "..x. .... ..x. ....",
     NOTHING,
     "0... ..0. 5... ..7.|0... ..0. 5... 3.2."},
    {"HYPERFLOP", 160, 16, 0, K_808,
     "x... ..x. x... ..x.|x.x. x.x. xxxx xxxx",
     ".... x... .... x...|.... x... x.x. xxxx",
     NOTHING,
     "xxxx xxxx xxxx xxxx",
     NOTHING, NOTHING,
     "0_.. +_.. 5_.. 7_..|0_.. +_.. ^_.. ^___"},
    {"FUNERAL BOSSA", 84, 16, 0, K_SOFT,
     "x..x x..x x..x x..x",
     NOTHING,
     NOTHING,
     "o-o- o-o- o-o- o-o-",
     NOTHING,
     "x..x ..x. ..x. .x..",
     "0..7 ..0. 5..0 ..5.|0..7 ..0. 5..3 ..2."},
    {"OFFICE AMBIVALENT", 64, 16, 0, K_LOFI,
     "x... .... .... ....",
     NOTHING,
     NOTHING,
     ".... .... ..-. ....",
     NOTHING,
     "..-. .... .... -...",
     "0___ ____ ____ ____"},
};
const char *sy_style_name(int s) { return STYLES[s < 0 ? 0 : s >= ST_N ? ST_N - 1 : s].name; }
int sy_style_tempo(int s) { return STYLES[s < 0 ? 0 : s >= ST_N ? ST_N - 1 : s].bpm; }
int sy_style_steps(int s) { return STYLES[s < 0 ? 0 : s >= ST_N ? ST_N - 1 : s].steps; }

/* parsed: per style, per track, main and fill, a byte per step */
enum { TR_KICK, TR_SNARE, TR_CLAP, TR_HAT, TR_OHAT, TR_PERC, TR_BASS, TR_N };
#define MAXSTEP 24
static uint8_t pat[ST_N][TR_N][2][MAXSTEP];
char sy_style_step(int s, int track, int fill, int step)
{
    if (s < 0 || s >= ST_N || track < 0 || track >= TR_N || step < 0 || step >= MAXSTEP)
        return '.';
    return (char)pat[s][track][fill ? 1 : 0][step];
}

static void parse_track(const char *s, uint8_t *main_, uint8_t *fill, int steps)
{
    int half = 0, n = 0, k;
    for (k = 0; k < MAXSTEP; k++)
        main_[k] = fill[k] = '.';
    for (; *s; s++) {
        if (*s == ' ')
            continue;
        if (*s == '|') {
            half = 1;
            n = 0;
            continue;
        }
        if (n < steps)
            (half ? fill : main_)[n++] = (uint8_t)*s;
    }
    if (!half)
        for (k = 0; k < MAXSTEP; k++)
            fill[k] = main_[k];
}

/* the length of a track's halves, for the tests: 0 if they fit the style's bar */
int sy_style_check(int s);
int sy_style_check(int s)
{
    const style_t *y = &STYLES[s];
    const char *tr[TR_N] = {y->kick, y->snare, y->clap, y->hat, y->ohat, y->perc, y->bass};
    int t;
    for (t = 0; t < TR_N; t++) {
        const char *p = tr[t];
        int n = 0;
        if (!*p)
            continue;
        for (;; p++) {
            if (*p == ' ')
                continue;
            if (*p == '|' || !*p) {
                if (n != y->steps)
                    return t + 1;
                if (!*p)
                    break;
                n = 0;
                continue;
            }
            n++;
        }
    }
    return 0;
}

typedef struct {
    float ph, f, amp, pe;                              /* phase, freq now, level, pitch envelope */
} osc_t;
static osc_t dk, dsn, dp, db;                          /* kick, snare tone, perc, bass */
static float sn_env, cl_env, cl_t, h_env, oh_env, hp1, hp1x, hp2, hp2x, sn_hp, sn_hpx, cl_bp, crk;
static float b_target, b_glide, b_gate, b_lp;
static uint8_t dr_on, dr_start;
#define abass_on (par[P_BASS] > 0)
static uint8_t dr_style, dr_pending, dr_step, dr_fired;
static uint32_t dr_bar;
static float dr_t, dr_len, dr_jit;

static float hit_vel(uint8_t c) { return c == 'x' ? 1.0f : c == 'o' ? 0.7f : c == '-' ? 0.35f : 0.0f; }
static float hv(uint8_t c) { return hit_vel(c) * (1.0f - pf(P_HUMAN) * 0.35f * rndf()); }   /* Human: uneven hands */

/* The bass is always sad: natural minor. What it is the minor of: held notes that sound minor (a minor
 * third over the lowest, no major third) are their own minor; anything else (a major chord, one note) is
 * taken as a major key, and the bass plays its relative minor, a minor third down: hold C, it plays A
 * minor, every note of it in C major too. In E1..D#2. */
static int bass_root(void)
{
    int i, lo = 128, r, minor = 0, major = 0, n = 0;
    uint8_t held[16];
    if (arp_on)                                          /* the arpeggiator's chord, not the note it is on */
        for (i = 0; i < arp_nh; i++)
            held[n++] = arp_held[i];
    else
        for (i = 0; i < SY_NVOICE; i++)
            if (vc[i].gate)
                held[n++] = vc[i].note;
    for (i = 0; i < n; i++)
        if (held[i] < lo)
            lo = held[i];
    for (i = 0; i < n; i++) {
        const int iv = (held[i] - lo) % 12;
        minor |= iv == 3;
        major |= iv == 4;
    }
    r = lo < 128 ? lo : sy_tel.last_note >= 0 ? sy_tel.last_note : 36;
    if (!(minor && !major))
        r -= 3;                                         /* the relative minor */
    while (r >= 40)
        r -= 12;
    while (r < 28)
        r += 12;
    return r;
}
/* a pattern's step, snapped onto natural minor (0 2 3 5 7 8 10): no bright note gets in */
static const uint8_t MINOR_SNAP[12] = {0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 10, 10};
static int minor_of(int semi) { return semi / 12 * 12 + MINOR_SNAP[semi % 12]; }

static void drum_step(void)
{
    const kit_t *k = &KITS[STYLES[dr_style].kit];
    const int fill = (dr_bar & 3u) == 3u;
    uint8_t c;
    float v;
    if ((v = hv(pat[dr_style][TR_KICK][fill][dr_step])) > 0.0f) {
        dk.amp = v;
        dk.pe = 1.0f;
        dk.ph = 0.0f;
        sy_tel.kick_n++;
    }
    if ((v = hv(pat[dr_style][TR_SNARE][fill][dr_step])) > 0.0f) {
        sn_env = v;
        dsn.amp = v;
        dsn.pe = 1.0f;
        dsn.ph = 0.0f;
        sy_tel.snare_n++;
    }
    if ((v = hv(pat[dr_style][TR_CLAP][fill][dr_step])) > 0.0f) {
        cl_env = v;
        cl_t = 0.0f;
        sy_tel.snare_n++;
    }
    if ((v = hv(pat[dr_style][TR_HAT][fill][dr_step])) > 0.0f) {
        h_env = v * 0.8f;
        oh_env = 0.0f;                                  /* the closed hat chokes the open one */
        sy_tel.hat_n++;
    }
    if ((v = hv(pat[dr_style][TR_OHAT][fill][dr_step])) > 0.0f) {
        oh_env = v * 0.7f;
        sy_tel.hat_n++;
    }
    if ((v = hv(pat[dr_style][TR_PERC][fill][dr_step])) > 0.0f) {
        dp.amp = v;
        dp.pe = 1.0f;
        dp.ph = 0.0f;
        sy_tel.perc_n++;
    }
    c = pat[dr_style][TR_BASS][fill][dr_step];
    if (abass_on && c != '_') {
        int semi = -1, slide = 0;
        if (c >= '0' && c <= '9')
            semi = c - '0';
        else if (c == 'a' || c == 'b')
            semi = 10 + c - 'a';
        else if (c == '+')
            semi = 12;
        else if (c >= 'A' && c <= 'L')
            semi = c - 'A', slide = 1;
        else if (c == '^')
            semi = 12, slide = 1;
        if (semi >= 0) {
            const int bn = bass_root() + minor_of(semi);
            float hz = 8.1757989f * fm_exp2f((float)bn * (1.0f / 12.0f));
            sy_tel.bass_note = (int16_t)bn;
            b_target = hz;
            if (!slide || b_gate <= 0.0f) {
                db.f = k->b_knock > 0.0f ? hz * 1.5f : hz;  /* the log drum falls into its note */
                db.amp = 1.0f;
                if (b_gate <= 0.0f)
                    db.ph = 0.0f;
            }
            b_glide = slide ? 1.0f - fm_expf(-1.0f / (k->b_glide * 1.6f * SY_SR)) : 1.0f - fm_expf(-1.0f / (0.004f * SY_SR));
            if (k->b_knock > 0.0f)
                b_glide = 1.0f - fm_expf(-1.0f / (0.03f * SY_SR));
            b_gate = 1.0f;
            sy_tel.bass_n++;
        } else if (c == '.') {
            b_gate = 0.0f;
        }
    }
}

/* one sample of the drums (and the auto-bass): their mix, mono, and the bass apart */
static inline float drums_sample(float *bass_out)
{
    const kit_t *k = &KITS[STYLES[dr_style].kit];
    float o = 0.0f, n = rnds(), x;
    const float inv = 1.0f / SY_SR;
    /* kick: a sine falling in pitch, then driven */
    if (dk.amp > 1e-4f) {
        dk.f = k->k_f1 + (k->k_f0 - k->k_f1) * dk.pe;
        dk.pe -= dk.pe * inv / k->k_pdec;
        dk.ph += dk.f * inv;
        dk.ph -= (float)(int)dk.ph;
        x = fm_sin_turns(dk.ph) * dk.amp;
        dk.amp -= dk.amp * inv * 2.3f / k->k_dec;
        o += fm_tanhf(x * k->k_drive) * (0.8f / fm_tanhf(k->k_drive)) * 0.9f;
    }
    /* snare: a tone and a hiss */
    if (sn_env > 1e-4f) {
        dsn.ph += k->s_tone * (1.0f + 0.5f * dsn.pe) * inv;
        dsn.ph -= (float)(int)dsn.ph;
        dsn.pe *= 0.997f;
        sn_hp += 0.6f * (n - sn_hpx);
        sn_hpx = n;
        x = fm_sin_turns(dsn.ph) * dsn.amp * (1.0f - k->s_noise) + (n - sn_hp * 0.5f) * k->s_noise * sn_env;
        dsn.amp -= dsn.amp * inv * 2.3f / (k->s_dec * 0.5f);
        sn_env -= sn_env * inv * 2.3f / k->s_dec;
        o += x * 0.5f;
    }
    /* clap: three quick bursts, then the tail */
    if (cl_env > 1e-4f) {
        float e;
        cl_t += inv;
        cl_bp += 0.35f * (n - cl_bp);
        x = n - cl_bp;
        e = cl_t < 0.03f ? (fm_floorf(cl_t * 100.0f) - cl_t * 100.0f + 1.0f) : 1.0f;
        o += x * cl_env * e * 0.45f;
        if (cl_t > 0.03f)
            cl_env -= cl_env * inv * 2.3f / 0.14f;
    }
    /* hats: noise through two high-passes */
    if (h_env > 1e-4f || oh_env > 1e-4f) {
        float h;
        hp1 = 0.8f * (hp1 + n - hp1x);
        hp1x = n;
        hp2 = 0.8f * (hp2 + hp1 - hp2x);
        hp2x = hp1;
        h = hp2 * (h_env + oh_env);
        h_env -= h_env * inv * 2.3f / k->h_dec;
        oh_env -= oh_env * inv * 2.3f / k->oh_dec;
        o += h * 0.32f;
    }
    /* perc */
    if (dp.amp > 1e-4f) {
        switch (k->perc) {
        case PERC_COWBELL:
            dp.ph += k->p_f * inv;
            dp.ph -= (float)(int)dp.ph;
            x = (dp.ph < 0.5f ? 0.5f : -0.5f) + (tri(dp.ph * 1.48f) * 0.4f);
            break;
        case PERC_TOM:
            dp.ph += k->p_f * (1.0f + dp.pe) * inv;
            dp.ph -= (float)(int)dp.ph;
            dp.pe *= 0.9993f;
            x = fm_sin_turns(dp.ph);
            break;
        case PERC_SQUEAK:
            dp.ph += k->p_f * (2.0f - dp.pe) * inv;
            dp.ph -= (float)(int)dp.ph;
            dp.pe *= 0.9995f;
            x = fm_sin_turns(dp.ph) * 0.8f;
            break;
        default:
            dp.ph += k->p_f * inv;
            dp.ph -= (float)(int)dp.ph;
            x = fm_sin_turns(dp.ph) * 0.6f + n * 0.3f * dp.amp;
            break;
        }
        o += x * dp.amp * 0.35f;
        dp.amp -= dp.amp * inv * 2.3f / k->p_dec;
    }
    /* a record's surface (lofi kits): sparse crackle and a hum of dust */
    if (k->crackle > 0.0f && dr_on) {
        if ((rnd() & 2047u) == 0u)
            crk = rnds() * 0.5f;
        o += crk * 0.2f + n * 0.004f;
        crk *= 0.9f;
    }
    /* the bass: a sine, glided, driven a little, low-passed */
    *bass_out = 0.0f;
    if (db.amp > 1e-4f) {
        db.f += (b_target - db.f) * b_glide;
        db.ph += db.f * inv;
        db.ph -= (float)(int)db.ph;
        x = fm_sin_turns(db.ph) + 0.15f * fm_sin_turns(2.0f * db.ph);
        x = fm_tanhf(x * k->b_drive * db.amp) / fm_tanhf(k->b_drive);
        b_lp += 0.25f * (x - b_lp);
        *bass_out = b_lp * 0.7f;
        db.amp -= db.amp * inv * 2.3f / (b_gate > 0.0f ? k->b_dec : 0.04f);
    }
    return o;
}

static void drums_tick(void)                           /* one sample of the sequencer */
{
    if (!dr_on)
        return;
    if (dr_start) {
        dr_start = 0;
        dr_step = 0;
        dr_t = 0.0f;
        dr_fired = 0;
        dr_style = dr_pending;
    }
    dr_len = SY_SR * 60.0f / (float)par[P_TEMPO] * 4.0f / (float)STYLES[dr_style].steps;
    if (!dr_fired) {
        /* the style's own swing plus Swing (up to 60% of a step late), plus Human's stumble */
        const float sw = ((dr_step & 1u) ? fm_minf((float)STYLES[dr_style].swing + 40.0f * pf(P_SWING), 60.0f) * 0.01f * dr_len : 0.0f) + dr_jit;
        if (dr_t >= sw) {
            dr_fired = 1;
            drum_step();
            if (dr_step == 0u && arp_on)                 /* the arpeggiator starts each bar with them */
                arp_t = arp_t > 0.5f * arp_len ? arp_len : 0.0f;
            sy_tel.step = dr_step;
            sy_tel.bar = (uint8_t)dr_bar;
            sy_tel.steps = STYLES[dr_style].steps;
            sy_tel.style = dr_style;
        }
    }
    dr_t += 1.0f;
    if (dr_t >= dr_len) {
        dr_t -= dr_len;
        dr_fired = 0;
        dr_jit = rndf() * pf(P_HUMAN) * 0.07f * dr_len;
        if (++dr_step >= STYLES[dr_style].steps) {
            dr_step = 0;
            dr_bar++;
            dr_style = dr_pending;                      /* a new tape waits for the bar */
        }
    }
}

/* ------------------------------------------------------------ commands --- */
static void drain(void)
{
    while (q_r != q_w) {
        cmd_t c = q[q_r % QN];
        BARRIER();
        q_r++;
        switch (c.c) {
        case C_SET:
            if (c.a >= 0 && c.a < P_NPARAMS) {
                const sy_param_t *p = &PARAMS[c.a];
                par[c.a] = (int16_t)clampi(c.b, p->lo, p->hi);
                if (c.a == P_STYLE)
                    dr_pending = (uint8_t)par[P_STYLE];
                if (c.a == P_SIZE) {
                    pl_decay = 0.25f + 0.7f * pf(P_SIZE);
                    pl_damp = 0.25f + 0.6f * pf(P_SIZE);
                }
            }
            break;
        case C_ON:
            if (arp_on) {
                arp_vel = (uint8_t)c.b;
                arp_add(c.a);
                sy_tel.last_note = c.a;
            } else {
                note_on(c.a, c.b);
            }
            break;
        case C_OFF:
            if (arp_on)
                arp_remove(c.a);
            else
                note_off(c.a);
            break;
        case C_BEND: bend_c = (float)c.a * (200.0f / 8192.0f); break;
        case C_MOD: mod_amt = (float)c.a * (0.5f / 127.0f); break;
        case C_DRUMS:
            if (c.a && !dr_on) {
                dr_on = 1;
                dr_start = 1;
                dr_bar = 0;
            } else if (!c.a) {
                dr_on = 0;
                b_gate = 0.0f;
            }
            sy_tel.drums_on = dr_on;
            break;
        case C_ARP: arp_set(c.a); break;
        case C_PANIC: {
            int i;
            for (i = 0; i < SY_NVOICE; i++) {
                vc[i].gate = 0;
                vc[i].stage = ST_OFF;
                vc[i].env = 0.0f;
            }
            dr_on = 0;
            db.amp = dk.amp = dp.amp = 0.0f;
            sn_env = cl_env = h_env = oh_env = 0.0f;
            sy_tel.drums_on = 0;
            arp_cur = -1;
            arp_nh = 0;
            break;
        }
        }
    }
}

/* ------------------------------------------------------------- render --- */
#define BLK 256u
static float bl[BLK], br[BLK];
static float lim_g = 1.0f;

void sy_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    uint32_t j0, j;
    float lfo, peak = 0.0f, gr_min = 1.0f, busy = 0.0f, lv[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float synth_g = pf(P_SYNTH) * 1.25f;
    const float chorus_amt = pf(P_CHORUS), dsend = pf(P_DELAY) * 0.8f, rsend = pf(P_REVERB) * 0.6f;
    const float tape = pf(P_TAPE), drum_g = pf(P_DRUMS) * 0.5f, bass_g = pf(P_BASS) * 0.55f;
    const float vol = pf(P_VOLUME) * pf(P_VOLUME) * 1.6f * (float)gain_q12 * (1.0f / 4096.0f);
    const float tp_sat = 1.0f + tape * 1.8f, tp_comp = 1.0f / fm_tanhf(tp_sat), tp_lp = 0.95f - 0.6f * tape;
    drain();
    if (n > BLK)
        n = BLK;
    lfo = lfo_tick(n);
    sy_tel.lfo = lfo;
    sy_tel.lfo_phase = lfo_ph;
    for (j = 0; j < n; j++)
        bl[j] = br[j] = 0.0f;
    render_voices(bl, br, n, lfo);
    for (j = 0; j < SY_SCOPE; j++) {                    /* the voices alone: the blob's outline */
        uint32_t s = j * n / SY_SCOPE;
        sy_tel.scope[j] = (bl[s] + br[s]) * 1.5f;
    }
    for (j0 = 0; j0 < n; j0 += 16u) {
        float d[4];
        ch_ph += 0.37f * 16.0f / SY_SR;
        ch_ph -= (float)(int)ch_ph;
        d[0] = 441.0f + 180.0f * fm_sin_turns(ch_ph);
        d[1] = 441.0f + 180.0f * fm_sin_turns(ch_ph + 0.25f);
        d[2] = 700.0f + 150.0f * fm_sin_turns(ch_ph * 1.7f + 0.5f);
        d[3] = 700.0f + 150.0f * fm_sin_turns(ch_ph * 1.7f + 0.75f);
        wow_ph += 0.5f * 16.0f / SY_SR;
        wow_ph -= (float)(int)wow_ph;
        flut_ph += 6.5f * 16.0f / SY_SR;
        fry_chunk();
        gunk_chunk();
        flut_ph -= (float)(int)flut_ph;
        d_wob = tape * 0.004f * fm_sin_turns(wow_ph);
        for (j = j0; j < j0 + 16u && j < n; j++) {
            float l = bl[j], r = br[j], el, er, rl, rr, dm, bass;
            fry(&l, &r);
            chorus(&l, &r, chorus_amt, d);
            l *= synth_g;                               /* the mixer: the voices' fader */
            r *= synth_g;
            drums_tick();
            arp_tick();
            dm = drums_sample(&bass) * drum_g;
            bass *= bass_g;
            {
                const float a = fm_fabsf(l) + fm_fabsf(r), b = fm_fabsf(dm), c = fm_fabsf(bass);
                lv[0] = a * 0.5f > lv[0] ? a * 0.5f : lv[0];
                lv[1] = b > lv[1] ? b : lv[1];
                lv[2] = c > lv[2] ? c : lv[2];
            }
            delay((l + r) * 0.5f * dsend, &el, &er);
            reverb((l + r) * 0.5f * rsend + (el + er) * 0.2f * rsend + dm * 0.12f, &rl, &rr);
            {
                float a = fm_fabsf(rl) + fm_fabsf(rr);
                pl_peak = a > pl_peak ? a : pl_peak * 0.99995f;
            }
            l += el + rl + dm + bass;
            r += er + rr + dm + bass;
            {
                const float a = (fm_fabsf(l) > fm_fabsf(r) ? fm_fabsf(l) : fm_fabsf(r)) * vol;
                busy = a > busy ? a : busy;
            }
            gunk(&l, &r, &lv[3]);
            /* tape: a wandering line, saturation, a softer top */
            tp_l[tp_w] = l;
            tp_r[tp_w] = r;
            tp_w = (tp_w + 1u) & (TP_N - 1u);
            if (tape > 0.0f) {
                float back = 40.0f + tape * (30.0f * fm_sin_turns(wow_ph) + 6.0f * fm_sin_turns(flut_ph) + 36.0f);
                l = line_read(tp_l, TP_N - 1u, tp_w, back);
                r = line_read(tp_r, TP_N - 1u, tp_w, back);
                l = fm_tanhf(l * tp_sat) * tp_comp;
                r = fm_tanhf(r * tp_sat) * tp_comp;
                tp_lpl += tp_lp * (l - tp_lpl);
                tp_lpr += tp_lp * (r - tp_lpr);
                l = tp_lpl;
                r = tp_lpr;
            }
            l *= vol;
            r *= vol;
            /* limiter: instant down, slow back up */
            {
                float a = fm_fabsf(l) > fm_fabsf(r) ? fm_fabsf(l) : fm_fabsf(r);
                peak = a > peak ? a : peak;
                if (a * lim_g > 0.95f)
                    lim_g = 0.95f / a;
                else
                    lim_g += (1.0f - lim_g) * 0.00015f;
                gr_min = lim_g < gr_min ? lim_g : gr_min;
                l *= lim_g;
                r *= lim_g;
            }
            {
                float sl = fm_clampf(l, -1.0f, 1.0f) * 8388607.0f, sr = fm_clampf(r, -1.0f, 1.0f) * 8388607.0f;
                out[2 * j] = (int32_t)sl;
                out[2 * j + 1] = (int32_t)sr;
            }
        }
    }
    sy_tel.out_peak = peak;
    sy_tel.busy = busy;
    for (j = 0; j < 4u; j++)
        sy_tel.lvl[j] = lv[j];
    if (gr_min < 0.6f)                                  /* more than ~4.4 dB pulled down: a fight */
        sy_tel.clips++;
    sy_tel.echo = fm_clampf(d_peak * 1.5f, 0.0f, 1.0f);
    sy_tel.verb = fm_clampf(pl_peak * 2.0f, 0.0f, 1.0f);
    for (j = 0; j < SY_NVOICE; j++)
        vc[j].s1[0] = fm_flush(vc[j].s1[0]);
}

void sy_init(void)
{
    int i, s, t;
    for (i = 0; i < P_NPARAMS; i++)
        par[i] = PARAMS[i].def;
    for (i = 0; i < SY_NVOICE; i++)
        vc[i].stage = ST_OFF;
    tables_build();
    plate_init();
    for (s = 0; s < ST_N; s++) {
        const style_t *y = &STYLES[s];
        const char *tr[TR_N] = {y->kick, y->snare, y->clap, y->hat, y->ohat, y->perc, y->bass};
        for (t = 0; t < TR_N; t++)
            parse_track(tr[t], pat[s][t][0], pat[s][t][1], y->steps);
    }
    dr_style = dr_pending = (uint8_t)par[P_STYLE];
    sy_tel.last_note = -1;
    sy_tel.bass_note = -1;
    sy_tel.arp_note = -1;
    sy_tel.steps = STYLES[dr_style].steps;
    sy_tel.style = dr_style;
    pl_decay = 0.25f + 0.7f * pf(P_SIZE);
    pl_damp = 0.25f + 0.6f * pf(P_SIZE);
    q_r = q_w;
}
