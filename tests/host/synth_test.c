/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1's instrument on the host, without libm (as on the device): every style's bar fits, nothing
 * aliases, nothing blows up, envelopes take their time, voices get stolen politely; demo WAVs of the
 * tables and of every drum style land in OUTDIR for listening.
 *   synth_test OUTDIR */
#include <stdio.h>
#include <string.h>
#include "synth.h"
#include "mopemath.h"

int sy_style_check(int s);
static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok   "); else { printf("  FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int32_t blk[512];
static FILE *wav;
static uint32_t wav_n;
static float peak;
static int bad;

static void wav_open(const char *dir, const char *name)
{
    char p[512];
    static const uint8_t h[44] = {0};
    snprintf(p, sizeof p, "%s/%s.wav", dir, name);
    wav = fopen(p, "wb");
    if (wav)
        fwrite(h, 1, 44, wav);
    wav_n = 0;
}
static void put32(uint32_t v) { fwrite(&v, 4, 1, wav); }
static void wav_close(void)
{
    uint16_t a[4] = {1, 2, 4, 16};
    if (!wav)
        return;
    fseek(wav, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, wav);
    put32(36 + wav_n * 4);
    fwrite("WAVEfmt ", 1, 8, wav);
    put32(16);
    fwrite(a, 2, 2, wav);
    put32(44100);
    put32(44100 * 4);
    fwrite(a + 2, 2, 2, wav);
    fwrite("data", 1, 4, wav);
    put32(wav_n * 4);
    fclose(wav);
    wav = 0;
}

static void run(uint32_t ms)
{
    uint32_t blocks = ms * 441u / 2560u + 1u, b, k;
    for (b = 0; b < blocks; b++) {
        sy_render(blk, 256, 4096);
        for (k = 0; k < 512; k++) {
            float x = (float)blk[k] / 8388608.0f;
            int16_t s = (int16_t)(x * 32767.0f);
            if (x != x)
                bad = 1;
            x = mm_fabsf(x);
            peak = x > peak ? x : peak;
            if (wav)
                fwrite(&s, 2, 1, wav);
        }
        if (wav)
            wav_n += 256;
    }
}

/* the energy of a sine at hz in a mono capture (Goertzel), relative to the capture's */
static float cap[8192];
/* a click in ms of output: the left side's biggest step from one sample to the next, as a multiple of the steps
 * around it (smooth sound: a few; a jump: many) */
static float spikiness(uint32_t ms)
{
    uint32_t blocks = ms * 441u / 2560u + 1u, b, k;
    float x[5] = {0, 0, 0, 0, 0}, worst = 0.0f;
    for (b = 0; b < blocks; b++) {
        sy_render(blk, 256, 4096);
        for (k = 0; k < 256; k++) {
            float d1, d0, d2, d3, around;
            x[0] = x[1];
            x[1] = x[2];
            x[2] = x[3];
            x[3] = x[4];
            x[4] = (float)blk[2 * k] / 8388608.0f;
            d0 = mm_fabsf(x[1] - x[0]);
            d1 = mm_fabsf(x[2] - x[1]);                 /* (the step in the middle, against the ones beside it) */
            d2 = mm_fabsf(x[3] - x[2]);
            d3 = mm_fabsf(x[4] - x[3]);
            around = (d0 + d2 + d3) / 3.0f + 1e-4f;
            if (b > 2 && d1 / around > worst)
                worst = d1 / around;
        }
    }
    return worst;
}
static void capture(void)
{
    uint32_t b, k;
    for (b = 0; b < 32; b++) {
        sy_render(blk, 256, 4096);
        for (k = 0; k < 256; k++)
            cap[b * 256 + k] = (float)blk[2 * k] / 8388608.0f;
    }
}
static float goertzel(float hz)
{
    float c = 2.0f * mm_cosf(MM_TWO_PI * hz / 44100.0f), s1 = 0.0f, s2 = 0.0f, w;
    int i;
    for (i = 0; i < 8192; i++) {
        w = 0.5f - 0.5f * mm_cosf(MM_TWO_PI * (float)i / 8192.0f);
        float s = cap[i] * w + c * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

static void quiet(void)
{
    sy_panic();
    run(50);
}

/* the mean level (rms) over ms of output, both sides */
static float level(uint32_t ms)
{
    uint32_t blocks = ms * 441u / 2560u + 1u, b, k;
    float e = 0.0f;
    for (b = 0; b < blocks; b++) {
        sy_render(blk, 256, 4096);
        for (k = 0; k < 512; k++) {
            const float x = (float)blk[k] / 8388608.0f;
            if (x != x)
                bad = 1;
            e += x * x;
        }
    }
    return mm_sqrtf(e / (float)(blocks * 512u));
}
/* how long a note's room takes to fall 60 dB (-1: longer than max_ms) */
static int room_t60(int size, int max_ms)
{
    float ref, t;
    quiet();
    run(400);
    sy_set(P_SIZE, size);
    run(20);
    sy_note_on(57, 110);
    run(200);
    sy_note_off(57);
    run(20);
    ref = level(50);
    for (t = 50.0f; t < (float)max_ms; t += 50.0f)
        if (level(50) < ref * 0.001f)
            return (int)t;
    return -1;
}
/* when the first echo of a short blip arrives, ms after the blip (-1: none): the blip dies away to
 * silence, then the first sound back is the echo */
static int first_echo(int time)
{
    uint32_t b, k, n = 0;
    int silent = 0;
    sy_set(P_TIME, time);
    quiet();
    run(3000);                                          /* (a new Time glides there) */
    sy_note_on(69, 110);
    for (b = 0; b < 600; b++, n += 64u) {
        if (b == 4)
            sy_note_off(69);
        float most = 0.0f;
        sy_render(blk, 64, 4096);
        for (k = 0; k < 128; k++) {
            const float a = mm_fabsf((float)blk[k] / 8388608.0f);
            if (silent && a > 0.02f)
                return (int)((n + k / 2u) * 1000u / 44100u);
            most = a > most ? a : most;
        }
        if (b > 4 && most < 0.002f)                     /* a whole block (1.5 ms) quiet: the blip is gone */
            silent = 1;
    }
    return -1;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int s, t, i, h;
    sy_init();

    printf("styles\n");
    for (s = 0; s < ST_N; s++) {
        int c = sy_style_check(s);
        CHECK(c == 0, "%s: every track fits its bar%s", sy_style_name(s), c ? " (a track does not)" : "");
    }

    printf("tables\n");
    for (t = 0; t < SY_NTABLE; t++) {
        float mx = 0.0f, mn = 0.0f, x;
        for (i = 0; i < 256; i++) {
            x = sy_shape(t, 0.5f, (float)i / 256.0f);
            mx = x > mx ? x : mx;
            mn = x < mn ? x : mn;
        }
        CHECK(mx > 0.3f && mn < -0.3f && mx <= 1.01f, "%s: a full shape (%.2f .. %.2f)", SY_TABLE_NAME[t], (double)mn, (double)mx);
    }

    printf("plain\n");
    {   /* table 0: a sine, a square at frame 4, a saw at frame 7 (its harmonics, from a 64-point DFT) */
        float hm[8][3];
        const float at[3] = {0.0f, 4.0f / 7.0f, 1.0f};
        int k, j;
        for (k = 0; k < 3; k++)
            for (h = 1; h <= 4; h++) {
                float re = 0.0f, im = 0.0f;
                for (j = 0; j < 64; j++) {
                    const float x = sy_shape(0, at[k], (float)j / 64.0f), ph = (float)(h * j) / 64.0f;
                    re += x * mm_sin_turns(ph);
                    im += x * mm_sin_turns(ph + 0.25f);
                }
                hm[h][k] = mm_sqrtf(re * re + im * im);
            }
        CHECK(hm[2][0] < hm[1][0] * 0.01f && hm[3][0] < hm[1][0] * 0.01f, "PLAIN starts as a sine (h2 %.3f, h3 %.3f of h1)",
              (double)(hm[2][0] / hm[1][0]), (double)(hm[3][0] / hm[1][0]));
        CHECK(hm[2][1] < hm[1][1] * 0.03f && hm[3][1] / hm[1][1] > 0.28f && hm[3][1] / hm[1][1] < 0.38f,
              "...is a square in the middle (h2 %.3f, h3 %.3f)", (double)(hm[2][1] / hm[1][1]), (double)(hm[3][1] / hm[1][1]));
        CHECK(hm[2][2] / hm[1][2] > 0.42f && hm[2][2] / hm[1][2] < 0.58f, "...and a saw at the end (h2 %.3f)",
              (double)(hm[2][2] / hm[1][2]));
    }

    printf("aliasing\n");
    /* a high note on a bright table: nothing between its harmonics, no fold-back */
    for (i = 0; i < P_NPARAMS; i++)
        sy_set(i, sy_param_info(i)->def);
    sy_set(P_TABLE, 7);
    sy_set(P_POS, 100);
    sy_set(P_CUTOFF, 100);
    sy_set(P_ENVAMT, 0);
    sy_set(P_DETUNE, 0);
    sy_set(P_SUB, 0);
    sy_set(P_CHORUS, 0);
    sy_set(P_DELAY, 0);
    sy_set(P_REVERB, 0);
    sy_set(P_TAPE, 0);
    sy_set(P_SUSTAIN, 100);
    sy_set(P_LFODEPTH, 0);
    sy_set(P_GUNK, 0);                                  /* (no room noise in a measurement) */
    for (t = 0; t < SY_NTABLE; t++) {                   /* every table, at its brightest frame */
        float f0 = 2093.0f, sig, worst = 0.0f;
        int h;
        sy_set(P_TABLE, t);
        run(20);
        sy_note_on(96, 100);                            /* C7, 2093 Hz */
        run(300);
        capture();
        sig = goertzel(f0);
        for (h = 1; h < 10; h++) {                      /* between the harmonics: where aliases would land */
            float a = goertzel(f0 * (float)h + f0 * 0.37f), b = goertzel(f0 * (float)h + f0 * 0.71f);
            worst = a > worst ? a : worst;
            worst = b > worst ? b : worst;
        }
        CHECK(worst < sig * 1e-5f, "C7 on %s: aliases %.1f dB under the fundamental", SY_TABLE_NAME[t],
              (double)(10.0f * mm_log2f(worst / sig + 1e-20f) * 0.30103f));
        sy_note_off(96);
        quiet();
    }

    printf("lfo destinations\n");
    {   /* an LFO across the tables, round all sixteen: the wave changes, and it never clicks (no single step out
         * of line with the ones around it, as a switch from one table to the next without the crossfade is) */
        float spikes;
        sy_set(P_POS, 30);
        sy_set(P_CUTOFF, 100);
        sy_set(P_SUSTAIN, 100);
        sy_set(P_DETUNE, 0);
        sy_set(P_SUB, 0);
        sy_set(P_TABLE, 8);
        sy_set(P_LFODEST, LD_TABLE);
        sy_set(P_LFORATE, 55);
        sy_set(P_LFOSHAPE, LFO_SINE);
        sy_set(P_LFODEPTH, 100);
        sy_note_on(45, 100);
        run(300);
        spikes = spikiness(3000);
        CHECK(sy_tel.wave_tbl != 8.0f, "LFO to the tables: it scans them (at %.2f)", (double)sy_tel.wave_tbl);
        CHECK(spikes < 8.5f && !bad, "...without a click (worst step %.1fx the ones around it; without the crossfade ~10x)",
              (double)spikes);
        sy_note_off(48);
        sy_note_off(45);
        sy_set(P_LFODEPTH, 0);
        quiet();
    }
    {   /* an LFO on the walls: the reverb opens with it, though the knob is at 0 */
        float most = 0.0f;
        int k;
        sy_set(P_REVERB, 0);
        sy_set(P_LFODEST, LD_WALLS);
        sy_set(P_LFORATE, 30);
        sy_set(P_LFODEPTH, 100);
        sy_note_on(60, 100);
        for (k = 0; k < 40; k++) {
            run(50);
            most = sy_tel.verb > most ? sy_tel.verb : most;
        }
        sy_note_off(60);
        CHECK(most > 0.02f, "LFO to the walls: the room opens with it (%.3f)", (double)most);
        sy_set(P_LFODEPTH, 0);
        sy_set(P_LFODEST, LD_POS);
        for (k = 0; k < P_NPARAMS; k++)
            sy_set(k, sy_param_info(k)->def);
        quiet();
    }

    printf("punches\n");
    {
        int k, fx, eq;
        float lo, mid, hi, before, after, most, least;
        for (k = 0; k < P_NPARAMS; k++)
            sy_set(k, sy_param_info(k)->def);
        sy_set(P_GUNK, 0);
        sy_set(P_TAPE, 0);
        sy_set(P_CHORUS, 0);
        sy_set(P_DELAY, 0);
        sy_set(P_REVERB, 0);
        sy_set(P_LFODEPTH, 0);
        sy_set(P_DETUNE, 0);
        sy_set(P_SUB, 0);
        sy_set(P_POS, 0);                               /* (PLAIN at its start: a sine) */
        sy_set(P_CUTOFF, 100);
        sy_set(P_SUSTAIN, 100);
        quiet();
        /* HALF and DOUBLE: a 440 Hz sine comes out an octave down, an octave up */
        sy_note_on(69, 100);
        run(600);
        for (fx = PU_HALF; fx <= PU_DOUBLE; fx++) {
            sy_punch(fx, 1);
            run(400);
            capture();
            lo = goertzel(220.0f);
            mid = goertzel(440.0f);
            hi = goertzel(880.0f);
            if (fx == PU_HALF)
                CHECK(lo > mid * 10.0f, "HALF: an octave down (220 Hz %.0f dB over 440)", (double)(10.0f * mm_log2f(lo / mid) * 0.30103f));
            else
                CHECK(hi > mid * 10.0f, "DOUBLE: an octave up (880 Hz %.0f dB over 440)", (double)(10.0f * mm_log2f(hi / mid) * 0.30103f));
            sy_punch(fx, 0);
            run(100);
        }
        /* the white keys beside: HALF with MORE two octaves down, with TWIST a fifth down; DOUBLE with MORE two up */
        sy_punch(PU_HALF, 1 + PM_MORE);
        run(400);
        capture();
        CHECK(goertzel(110.0f) > goertzel(220.0f) * 10.0f && sy_tel.punch_mod[PU_HALF] == PM_MORE, "HALF + MORE: two octaves down");
        sy_punch(PU_HALF, 1 + PM_TWIST);                /* (held already: only its way changes) */
        run(400);
        capture();
        CHECK(goertzel(293.3f) > goertzel(220.0f) * 10.0f, "HALF + TWIST: a fifth down");
        sy_punch(PU_HALF, 0);
        run(100);
        sy_punch(PU_DOUBLE, 1 + PM_MORE);
        run(400);
        capture();
        CHECK(goertzel(1760.0f) > goertzel(880.0f) * 10.0f, "DOUBLE + MORE: two octaves up");
        sy_punch(PU_DOUBLE, 0);
        run(100);
        capture();
        CHECK(goertzel(440.0f) > goertzel(220.0f) * 100.0f && sy_tel.punch == 0, "let go: back to the note as it was");
        /* the knobs: HALF's knob 1, how far down: a fourth .. two octaves */
        sy_punch_knob(PU_HALF, 0, 0);
        sy_punch(PU_HALF, 1);
        run(400);
        capture();
        CHECK(goertzel(330.0f) > goertzel(220.0f) * 10.0f, "HALF, knob 1 down: only a fourth down");
        sy_punch(PU_HALF, 0);
        sy_punch_knob(PU_HALF, 0, PU_KMAX);
        run(100);
        sy_punch(PU_HALF, 1);
        run(400);
        capture();
        CHECK(goertzel(110.0f) > goertzel(220.0f) * 10.0f, "HALF, knob 1 up: two octaves down");
        sy_punch(PU_HALF, 0);
        sy_punch_knob(PU_HALF, 0, SY_PUNCH_KDEF[PU_HALF][0]);
        run(100);
        {   /* GRAIN comes in slowly: its first 186 ms against a second on */
            float slow, full;
            sy_punch(PU_GRAIN, 1);
            capture();
            slow = (goertzel(220.0f) + goertzel(880.0f)) / goertzel(440.0f);
            run(800);
            capture();
            full = (goertzel(220.0f) + goertzel(880.0f)) / goertzel(440.0f);
            sy_punch(PU_GRAIN, 0);
            run(200);
            CHECK(full > slow * 4.0f, "GRAIN: slowly in (its first 186 ms %.4f of the scatter it has later, %.3f)",
                  (double)slow, (double)full);
        }
        /* STOP: slower, then nothing */
        before = level(100);
        sy_punch(PU_STOP, 1);
        run(900);
        after = level(100);
        CHECK(after < before * 0.01f, "STOP: then nothing (%.4f of it)", (double)(after / before));
        sy_punch(PU_STOP, 0);
        run(50);
        CHECK(level(100) > before * 0.5f, "...and back when let go");
        /* GATE: open, shut, open, shut */
        sy_punch(PU_GATE, 1);
        run(100);
        most = 0.0f;
        least = 1.0f;
        for (k = 0; k < 40; k++) {
            const float lv = level(10);
            most = lv > most ? lv : most;
            least = lv < least ? lv : least;
        }
        CHECK(least < most * 0.1f, "GATE: open and shut (%.0f dB between)", (double)(20.0f * mm_log2f(most / (least + 1e-9f)) * 0.30103f));
        {   /* GATE + MORE: twice as many openings in the same time */
            int opens[2], m;
            for (m = 0; m < 2; m++) {
                int was = 1;
                sy_punch(PU_GATE, 1 + (m ? PM_MORE : 0));
                run(60);
                opens[m] = 0;
                for (k = 0; k < 200; k++) {             /* (2.9 ms a look, 0.58 s) */
                    sy_render(blk, 128, 4096);
                    const int open = sy_tel.punch_gate > 0.5f;
                    opens[m] += open && !was;
                    was = open;
                }
            }
            CHECK(opens[1] >= opens[0] * 2 - 1 && opens[0] >= 3, "GATE + MORE: 1/32s (%d openings, then %d)", opens[0], opens[1]);
        }
        sy_punch(PU_GATE, 0);
        run(100);
        /* CRUSH: the samples held, a fifth of the rate */
        sy_punch(PU_CRUSH, 1);
        run(100);
        sy_render(blk, 256, 4096);
        for (eq = 0, k = 1; k < 256; k++)
            eq += blk[2 * k] == blk[2 * k - 2];
        CHECK(eq > 160, "CRUSH: each sample held (%d of 255 the same as the last)", eq);
        sy_punch(PU_CRUSH, 0);
        run(100);
        sy_note_off(69);
        quiet();
        /* SWEEP: a bright saw goes dark */
        sy_set(P_POS, 100);                             /* (PLAIN's end: a saw) */
        sy_note_on(57, 100);
        run(300);
        capture();
        before = goertzel(3960.0f);
        sy_punch(PU_SWEEP, 1);
        run(1600);
        capture();
        after = goertzel(3960.0f);
        CHECK(after < before * 0.01f, "SWEEP: the top closes (%.0f dB down at 4 kHz)", (double)(10.0f * mm_log2f(before / (after + 1e-20f)) * 0.30103f));
        sy_punch(PU_SWEEP, 0);
        run(300);
        /* REPEAT: the note changes, but what comes out is the kept slice, again and again */
        sy_punch(PU_REPEAT, 1);
        run(40);
        sy_note_off(57);
        sy_note_on(64, 100);
        run(400);
        capture();
        CHECK(goertzel(220.0f) > goertzel(329.6f) * 10.0f, "REPEAT: the slice from before goes round (not the new note)");
        sy_punch(PU_REPEAT, 0);
        run(200);
        capture();
        CHECK(goertzel(329.6f) > goertzel(220.0f) * 10.0f, "...let go: the new note");
        sy_note_off(64);
        quiet();
        /* WASH: the room holds on long after the note */
        sy_set(P_REVERB, 40);
        sy_set(P_SIZE, 30);
        sy_note_on(60, 100);
        run(300);
        sy_note_off(60);
        run(2000);
        before = level(200);
        quiet();
        sy_note_on(60, 100);
        run(300);
        sy_punch(PU_WASH, 1);
        sy_note_off(60);
        run(2000);
        after = level(200);
        CHECK(after > before * 10.0f, "WASH: the room holds it (%.0f dB more, 2 s on)", (double)(20.0f * mm_log2f(after / (before + 1e-9f)) * 0.30103f));
        sy_punch(PU_WASH, 0);
        sy_set(P_REVERB, 0);
        quiet();
        run(3000);
        {   /* on the beat: with the band playing a punch waits for the next step; a tap plays one step; stopped: at once */
            int frames = 0, held = 0, waited;
            float steplen;
            sy_set(P_TEMPO, 120);
            sy_drums(1);
            run(400);
            steplen = 44100.0f * 60.0f / 120.0f * 4.0f / (float)(sy_tel.steps ? sy_tel.steps : 16);
            sy_punch(PU_CRUSH, 1);
            sy_render(blk, 16, 4096);
            waited = (sy_tel.punch_wait >> PU_CRUSH & 1u) && !(sy_tel.punch >> PU_CRUSH & 1u);
            while (!(sy_tel.punch >> PU_CRUSH & 1u) && frames < 44100) {
                sy_render(blk, 16, 4096);
                frames += 16;
            }
            CHECK(waited && frames <= (int)steplen + 32, "on the beat: it waits for the step (%d frames, a step %.0f)", frames, (double)steplen);
            sy_punch(PU_CRUSH, 0);
            run(200);
            sy_punch(PU_GATE, 1);                       /* a tap, shorter than a step */
            sy_render(blk, 16, 4096);
            sy_punch(PU_GATE, 0);
            for (frames = 0; frames < 44100; frames += 16) {
                sy_render(blk, 16, 4096);
                if (sy_tel.punch >> PU_GATE & 1u)
                    held += 16;
            }
            CHECK(held > (int)(steplen * 0.9f) && held < (int)(steplen * 1.1f) + 32, "...a tap: one step of it (%d frames)", held);
            sy_drums(0);
            run(200);
            sy_punch(PU_CRUSH, 1);
            sy_render(blk, 16, 4096);
            CHECK(sy_tel.punch >> PU_CRUSH & 1u, "...the band stopped: at once");
            sy_punch(PU_CRUSH, 0);
            run(4000);                                  /* (the band's last hits and the limiter: settled) */
            sy_set(P_TEMPO, sy_param_info(P_TEMPO)->def);
        }
        /* power-on: up from nothing, slowly */
        sy_note_on(60, 100);
        run(300);
        before = level(50);
        sy_fade_in(3000u);
        after = level(100);
        run(2900);
        most = level(100);
        CHECK(after < before * 0.01f && most > before * 0.7f, "fade in: silent at first (%.4f), all there after 3 s (%.2f)",
              (double)(after / before), (double)(most / before));
        sy_note_off(60);
        quiet();
        /* every one on silence: silence; all of them together: nothing breaks; a wrong one: nothing */
        for (fx = 0; fx < PU_N; fx++) {
            sy_punch(fx, 1);
            run(150);
            CHECK(level(100) < 1e-4f, "%d on silence: silence", fx);
            sy_punch(fx, 0);
            run(50);
        }
        sy_punch(99, 1);
        sy_punch(-1, 1);
        sy_note_on(48, 127);
        sy_note_on(55, 127);
        for (fx = 0; fx < PU_N; fx++)
            sy_punch(fx, 4);
        bad = 0;
        peak = 0.0f;
        run(2000);
        for (fx = 0; fx < PU_N; fx++)
            sy_punch(fx, 0);
        run(300);
        CHECK(!bad && peak <= 1.0f && sy_tel.punch == 0, "all twelve at once (all their ways), then none: no NaN, nothing over (peak %.2f)", (double)peak);
        for (eq = 0; eq < 2; eq++) {                    /* every knob all the way down, then up: each alone, nothing breaks */
            int kk;
            for (fx = 0; fx < PU_N; fx++)
                for (kk = 0; kk < PU_KN; kk++)
                    sy_punch_knob(fx, kk, eq ? PU_KMAX : 0);
            bad = 0;
            peak = 0.0f;
            for (fx = 0; fx < PU_N; fx++) {
                sy_punch(fx, 1 + (fx & 3));
                run(500);
                sy_punch(fx, 0);
                run(60);
            }
            CHECK(!bad && peak <= 1.0f, "every knob %s, each punch: no NaN, nothing over (peak %.2f)", eq ? "up" : "down", (double)peak);
        }
        for (fx = 0; fx < PU_N * PU_KN; fx++)
            sy_punch_knob(fx / PU_KN, fx % PU_KN, SY_PUNCH_KDEF[fx / PU_KN][fx % PU_KN]);
        sy_note_off(48);
        sy_note_off(55);
        for (k = 0; k < P_NPARAMS; k++)
            sy_set(k, sy_param_info(k)->def);
        quiet();
    }

    printf("envelope\n");
    sy_set(P_ATTACK, 80);                               /* ~1.4 s */
    sy_set(P_TABLE, 0);
    sy_note_on(60, 100);
    run(100);
    CHECK(sy_tel.amp_env < 0.2f, "a slow attack is still rising after 100 ms (%.2f)", (double)sy_tel.amp_env);
    run(2500);
    CHECK(sy_tel.amp_env > 0.5f, "...and has risen after 2.6 s (%.2f)", (double)sy_tel.amp_env);
    sy_note_off(60);
    sy_set(P_RELEASE, 10);
    run(400);
    CHECK(sy_tel.voices == 0, "a short release ends the voice (%d left)", sy_tel.voices);
    sy_set(P_ATTACK, 10);
    sy_set(P_RELEASE, 50);

    printf("voices\n");
    {
        uint32_t st0 = sy_tel.steals;
        for (i = 0; i < SY_NVOICE + 2; i++)
            sy_note_on(48 + i, 100);
        run(50);
        CHECK(sy_tel.steals - st0 == 2u, "two notes past the voices steal two (%u)", sy_tel.steals - st0);
        CHECK(sy_tel.gates == SY_NVOICE, "every voice is held (%d)", sy_tel.gates);
        for (i = 0; i < SY_NVOICE + 2; i++)
            sy_note_off(48 + i);
        run(50);
        CHECK(sy_tel.gates == 0, "all let go");
    }
    quiet();
    {   /* the load guard: two late blocks, a voice goes; calm again, it comes back */
        int k;
        for (i = 0; i < SY_NVOICE; i++)
            sy_note_on(48 + i, 100);
        run(30);
        sy_load(95);
        sy_load(95);
        run(80);                                        /* (it fades out in ~45 ms) */
        CHECK(sy_tel.voices == SY_NVOICE - 1, "two blocks over the budget: one voice fewer (%d)", sy_tel.voices);
        sy_note_on(80, 100);
        run(80);
        CHECK(sy_tel.voices == SY_NVOICE - 1, "...and a new note takes a sounding one's place (%d)", sy_tel.voices);
        for (k = 0; k < 400; k++)
            sy_load(40);
        sy_note_on(81, 100);
        run(30);
        CHECK(sy_tel.voices == SY_NVOICE, "calm for two seconds: all of them again (%d)", sy_tel.voices);
        for (i = 0; i < SY_NVOICE; i++)
            sy_note_off(48 + i);
        sy_note_off(80);
        sy_note_off(81);
        quiet();
    }
    {   /* a heavy sound (the shadow, drive, a band-pass) sounds fewer voices: the FM-1 keeps up */
        sy_set(P_BLEVEL, 60);
        sy_set(P_DRIVE, 50);
        sy_set(P_FTYPE, FT_BAND);
        for (i = 0; i < SY_NVOICE; i++)
            sy_note_on(48 + i, 100);
        run(60);
        CHECK(sy_tel.voices >= 3 && sy_tel.voices < SY_NVOICE, "a heavy sound: %d voices of %d", sy_tel.voices, SY_NVOICE);
        for (i = 0; i < SY_NVOICE; i++)
            sy_note_off(48 + i);
        sy_set(P_BLEVEL, 0);
        sy_set(P_DRIVE, 0);
        sy_set(P_FTYPE, FT_LOW);
        quiet();
    }
    {   /* VOWEL: a saw at 110 Hz says A (its 7th harmonic, 770 Hz, up) and then I (its 3rd, 330, and 21st, 2310, up) */
        float a7, a3, i7, i3, i21, a21;
        int k;
        for (i = 0; i < P_NPARAMS; i++)
            sy_set(i, sy_param_info(i)->def);
        sy_set(P_GUNK, 0);
        sy_set(P_REVERB, 0);
        sy_set(P_DELAY, 0);
        sy_set(P_CHORUS, 0);
        sy_set(P_TAPE, 0);
        sy_set(P_LFODEPTH, 0);
        sy_set(P_ENVAMT, 0);
        sy_set(P_SUSTAIN, 100);
        sy_set(P_POS, 100);                             /* (PLAIN's end: a saw) */
        sy_set(P_FTYPE, FT_VOWEL);
        sy_set(P_RESO, 70);
        sy_set(P_CUTOFF, 7);                            /* (0.07 * 9.3 octaves, less the key's -0.6: A) */
        quiet();
        sy_note_on(45, 100);
        run(400);
        a7 = a3 = a21 = i7 = i3 = i21 = 0.0f;
        for (k = 0; k < 10; k++) {                      /* (over whole beats of the voice's two halves, 2 cents apart) */
            capture();
            a7 += goertzel(770.0f), a3 += goertzel(330.0f), a21 += goertzel(2310.0f);
            run(90);
        }
        sy_set(P_CUTOFF, 57);                           /* (half way: I) */
        run(300);
        for (k = 0; k < 10; k++) {
            capture();
            i7 += goertzel(770.0f), i3 += goertzel(330.0f), i21 += goertzel(2310.0f);
            run(90);
        }
        sy_note_off(45);
        CHECK(a7 > a3 * 2.0f && i3 > i7 * 2.0f && i21 > a21 * 2.0f, "VOWEL: A then I (770 Hz %.0f dB over 330; then 330 %.0f dB over 770)",
              (double)(10.0f * mm_log2f(a7 / a3) * 0.30103f), (double)(10.0f * mm_log2f(i3 / i7) * 0.30103f));
        sy_set(P_FTYPE, FT_LOW);
        sy_set(P_RESO, sy_param_info(P_RESO)->def);
        quiet();
    }
    {   /* Level: +-50 is +-12 dB */
        float l0, l1;
        for (i = 0; i < P_NPARAMS; i++)                 /* a plain, steady note: no effects (Tape saturates) */
            sy_set(i, sy_param_info(i)->def);
        sy_set(P_GUNK, 0);
        sy_set(P_SUSTAIN, 100);
        sy_set(P_LFODEPTH, 0);
        sy_set(P_REVERB, 0);
        sy_set(P_DELAY, 0);
        sy_set(P_CHORUS, 0);
        sy_set(P_TAPE, 0);
        sy_set(P_DETUNE, 0);                            /* (one steady tone: no beating to measure across) */
        sy_set(P_LFO2DEPTH, 0);
        sy_set(P_SYNTH, 30);                            /* (quiet enough that nothing squashes it) */
        run(1500);
        sy_note_on(60, 100);
        run(600);
        l0 = level(3200);                               /* (the voice's two halves beat ~0.6 Hz even at Detune 0: whole beats) */
        sy_set(P_LEVEL, 25);
        run(300);
        l1 = level(3200);
        sy_set(P_LEVEL, 0);
        sy_note_off(60);
        quiet();
        CHECK(l1 > l0 * 1.6f && l1 < l0 * 2.5f, "Level +25: twice as loud (x%.2f)", (double)(l1 / l0));
    }

    printf("demos\n");
    for (i = 0; i < P_NPARAMS; i++)
        sy_set(i, sy_param_info(i)->def);
    for (t = 0; t < SY_NTABLE; t++) {
        char n[32];
        snprintf(n, sizeof n, "table_%d_%s", t, SY_TABLE_NAME[t]);
        sy_set(P_TABLE, t);
        wav_open(dir, n);
        peak = 0.0f;
        for (i = 0; i <= 4; i++) {                      /* a sad chord, the position walking up */
            sy_set(P_POS, i * 25);
            sy_note_on(48, 90);
            sy_note_on(51, 80);
            sy_note_on(55, 80);
            sy_note_on(58, 70);
            run(700);
            sy_note_off(48);
            sy_note_off(51);
            sy_note_off(55);
            sy_note_off(58);
            run(300);
        }
        run(1200);
        wav_close();
        CHECK(!bad && peak > 0.05f && peak < 1.0f, "%s: sound, no NaN, peak %.2f", SY_TABLE_NAME[t], (double)peak);
    }
    sy_set(P_TABLE, 0);
    sy_set(P_BASS, 60);                                 /* (the bass's fader: up, the auto-bass plays) */
    for (s = 0; s < ST_N; s++) {
        char n[48];
        uint32_t k0 = sy_tel.kick_n + sy_tel.snare_n + sy_tel.hat_n + sy_tel.perc_n;
        float secs = 4.0f * 4.0f * 60.0f / (float)sy_style_tempo(s);   /* four bars (the fourth: the fill) */
        snprintf(n, sizeof n, "style_%02d_%s", s, sy_style_name(s));
        for (i = 0; n[i]; i++)
            if (n[i] == ' ')
                n[i] = '_';
        sy_set(P_STYLE, s);
        sy_set(P_TEMPO, sy_style_tempo(s));
        sy_drums(1);
        sy_note_on(45, 70);                             /* A minor, held: the bass follows its root */
        sy_note_on(48, 60);
        sy_note_on(52, 60);
        wav_open(dir, n);
        peak = 0.0f;
        run((uint32_t)(secs * 1000.0f));
        sy_note_off(45);
        sy_note_off(48);
        sy_note_off(52);
        sy_drums(0);
        run(1500);
        wav_close();
        CHECK(!bad && peak > 0.05f && sy_tel.kick_n + sy_tel.snare_n + sy_tel.hat_n + sy_tel.perc_n > k0,
              "%s: it plays (%u hits), peak %.2f", sy_style_name(s),
              sy_tel.kick_n + sy_tel.snare_n + sy_tel.hat_n + sy_tel.perc_n - k0, (double)peak);
    }
    printf("bass\n");
    {   /* always minor: C major held -> A minor (every note in C major); C minor held -> C minor */
        static const int CHORD[2][3] = {{60, 64, 67}, {60, 63, 67}};
        static const uint16_t SCALE[2] = {0xAB5u, 0x5ADu};   /* pitch classes: C major; C natural minor */
        int c, n;
        sy_set(P_STYLE, ST_HYPERPOP);                       /* (its line reaches up: +, ^, 5, 7) */
        sy_set(P_TEMPO, 200);
        for (c = 0; c < 2; c++) {
            uint16_t seen = 0;
            int first = -1;
            for (n = 0; n < 3; n++)
                sy_note_on(CHORD[c][n], 80);
            sy_drums(1);
            for (n = 0; n < 400; n++) {                     /* ~2.3 s: two bars and more */
                int32_t b[512];
                sy_render(b, 256, 4096);
                if (sy_tel.bass_note >= 0) {
                    seen |= (uint16_t)(1u << (sy_tel.bass_note % 12));
                    if (first < 0)
                        first = sy_tel.bass_note % 12;
                }
            }
            sy_drums(0);
            for (n = 0; n < 3; n++)
                sy_note_off(CHORD[c][n]);
            run(300);
            CHECK(seen && !(seen & ~SCALE[c]), "%s held: the bass stays in %s (notes 0x%03X)", c ? "C minor" : "C major",
                  c ? "C minor" : "A minor", seen);
            CHECK(first == (c ? 0 : 9), "...and its root is %s (%d)", c ? "C" : "A", first);
            sy_tel.bass_note = -1;
        }
    }
    {   /* the bass's ways: ROOT once a bar (the chord unchanged), PULSE eight a bar, OCTAVES the root and up,
         * FOLLOW the lowest note held as it is; off: nothing */
        int w, n;
        uint32_t hits[BS_N];
        int16_t lo_note = 127, hi_note = 0, follow = -1;
        sy_set(P_STYLE, ST_FUNERAL);
        sy_set(P_TEMPO, 120);                           /* (a bar: 2 s) */
        for (w = BS_ROOT; w < BS_N; w++) {
            sy_set(P_BSTYLE, w);
            sy_note_on(w == BS_FOLLOW ? 64 : 57, 80);  /* A minor (FOLLOW: C major, which the others take as A minor) */
            sy_note_on(60, 80);
            run(50);
            sy_drums(1);
            run(30);
            {
                const uint32_t h0 = sy_tel.bass_n;
                for (n = 0; n < 345; n++) {             /* (2 s: one bar) */
                    int32_t b[512];
                    sy_render(b, 256, 4096);
                    if (w == BS_OCTAVES && sy_tel.bass_note >= 0) {
                        lo_note = sy_tel.bass_note < lo_note ? sy_tel.bass_note : lo_note;
                        hi_note = sy_tel.bass_note > hi_note ? sy_tel.bass_note : hi_note;
                    }
                    if (w == BS_FOLLOW && sy_tel.bass_note >= 0)
                        follow = sy_tel.bass_note;
                }
                hits[w] = sy_tel.bass_n - h0;
            }
            sy_drums(0);
            sy_note_off(w == BS_FOLLOW ? 64 : 57);
            sy_note_off(60);
            run(300);
        }
        CHECK(hits[BS_ROOT] == 1, "bass ROOT: one long note a bar (%u)", (unsigned)hits[BS_ROOT]);
        CHECK(hits[BS_PULSE] == 8, "bass PULSE: eight a bar (%u)", (unsigned)hits[BS_PULSE]);
        CHECK(hi_note - lo_note == 12, "bass OCTAVES: the root and an octave up (%d .. %d)", lo_note, hi_note);
        CHECK(follow == 36, "bass FOLLOW: the lowest note held as it is (C, not A minor: %d)", follow);
        sy_set(P_BSTYLE, BS_PULSE);
        sy_set(P_BASSON, 0);
        sy_note_on(57, 80);
        sy_drums(1);
        n = (int)sy_tel.bass_n;
        run(2000);
        CHECK((int)sy_tel.bass_n == n, "auto bass off: no bass");
        sy_drums(0);
        sy_note_off(57);
        sy_set(P_BASSON, 1);
        sy_set(P_BSTYLE, BS_BEAT);
        run(300);
    }
    {   /* voices: MONO one voice, the newest; let go, back to the one before. UNISON: four on one note */
        sy_set(P_VMODE, VM_MONO);
        run(20);
        sy_note_on(60, 90);
        sy_note_on(64, 90);
        run(100);
        CHECK(sy_tel.voices == 1 && sy_tel.last_note == 64, "MONO: one voice, the newest (%d voices)", sy_tel.voices);
        sy_note_off(64);
        run(100);
        CHECK(sy_tel.voices == 1, "...let go: still one, back on the first");
        sy_note_off(60);
        run(800);
        sy_set(P_VMODE, VM_UNISON);
        run(20);
        sy_note_on(60, 90);
        run(100);
        CHECK(sy_tel.voices == 4, "UNISON: four voices on one note (%d)", sy_tel.voices);
        sy_note_off(60);
        sy_set(P_VMODE, VM_POLY);
        run(800);
    }
    printf("arpeggiator\n");
    {
        static const int UP[6] = {60, 64, 67, 72, 76, 79};    /* C E G, two octaves */
        int16_t got[6];
        int n = 0, k, ok = 1, m;
        for (k = 0; k < P_NPARAMS; k++)
            sy_set(k, sy_param_info(k)->def);
        sy_set(P_GUNK, 0);
        sy_set(P_TEMPO, 120);
        sy_set(P_ARPRATE, 3);                           /* 1/16 at 120: 125 ms a step */
        sy_set(P_ARPOCT, 2);
        sy_arp(1);
        sy_note_on(67, 90);
        sy_note_on(60, 90);
        sy_note_on(64, 90);                             /* (held in any order: played low to high) */
        for (k = 0; k < 400 && n < 6; k++) {
            int16_t last = sy_tel.arp_note;
            int32_t b[512];
            sy_render(b, 256, 4096);
            if (sy_tel.arp_note >= 0 && sy_tel.arp_note != last)
                got[n++] = sy_tel.arp_note;
        }
        for (k = 0; k < 6; k++)
            ok &= k < n && got[k] == UP[k];
        CHECK(ok, "UP over two octaves: C E G C' E' G' (%d %d %d %d %d %d)", n > 0 ? got[0] : -1, n > 1 ? got[1] : -1,
              n > 2 ? got[2] : -1, n > 3 ? got[3] : -1, n > 4 ? got[4] : -1, n > 5 ? got[5] : -1);
        CHECK(sy_tel.gates <= 1, "one note at a time (%d held in the voices)", sy_tel.gates);
        for (m = 0, k = 0; k < 6; k++)
            m += sy_arp_note(3, k) == UP[k];
        CHECK(m == 6, "the screen's preview agrees with what it plays");
        sy_note_off(60);
        sy_note_off(64);
        sy_note_off(67);
        sy_arp(0);
        run(600);
        CHECK(sy_tel.gates == 0 && sy_tel.arp_on == 0, "off, and quiet when let go");
    }
    printf("gunk\n");
    {
        float p0, p1;
        int k;
        sy_set(P_GUNK, 0);
        sy_set(P_REVERB, 0);
        sy_set(P_DELAY, 0);
        run(2500);
        p0 = sy_tel.out_peak;
        sy_set(P_GUNK, 80);
        for (k = 0, p1 = 0.0f; k < 400; k++) {          /* (it swells in slowly) */
            int32_t b[512];
            sy_render(b, 256, 4096);
            p1 = sy_tel.lvl[3] > p1 ? sy_tel.lvl[3] : p1;
        }
        CHECK(p0 < 1e-3f && p1 > 0.005f && p1 < 0.25f, "gunk: silence without it (%.5f), a quiet texture with it (%.4f)", (double)p0, (double)p1);
        CHECK(sy_tel.busy < 1e-4f, "...and it doesn't count as playing (%.5f)", (double)sy_tel.busy);
        sy_set(P_GUNK, 0);
    }
    printf("loop\n");
    {
        /* 120 bpm, one bar: 2 s, a 1/16 every 125 ms (12 ticks). REC while stopped: a bar's count-in. */
        const sy_ev_t *ev;
        int k, n, on_grid = 1, played = 0;
        for (k = 0; k < P_NPARAMS; k++)
            sy_set(k, sy_param_info(k)->def);
        sy_set(P_GUNK, 0);
        sy_set(P_TEMPO, 120);
        sy_set(P_LOOPBARS, 0);
        sy_set(P_STYLE, ST_OFFICE);
        sy_rec(1);
        run(500);
        CHECK(sy_tel.rec == 1 && !sy_tel.drums_on, "REC while stopped: counting in, the band waits");
        run(1600);                                       /* (the count-in is 2 s) */
        CHECK(sy_tel.rec == 2 && sy_tel.drums_on, "after a bar: recording, the band plays");
        for (k = 0; k < 3; k++) {                        /* three notes, a little off the grid */
            run(260 + k * 7);
            sy_note_on(60 + 3 * k, 100);
            run(140);
            sy_note_off(60 + 3 * k);
        }
        run(300);
        sy_rec(0);
        run(20);
        ev = sy_loop_events(&n);
        for (k = 0; k < n; k++)
            on_grid &= ev[k].t % 12 == 0 && ev[k].len > 0;
        CHECK(n == 3 && on_grid && sy_tel.rec == 0, "three notes in the loop, on the 1/16 grid (%d)", n);
        for (k = 0; k < 400; k++) {                      /* the next pass plays them */
            int32_t b[512];
            sy_render(b, 256, 4096);
            played |= sy_tel.gates > 0;
        }
        CHECK(played, "the loop plays them back");
        sy_rec(1);                                       /* playing: records at once, on top */
        run(30);
        CHECK(sy_tel.rec == 2, "REC while playing: at once");
        sy_note_on(72, 90);
        run(100);
        sy_note_off(72);
        sy_rec(0);
        run(20);
        sy_loop_events(&n);
        CHECK(n == 4, "overdub: a fourth (%d)", n);
        sy_loop_undo();
        run(20);
        sy_loop_events(&n);
        CHECK(n == 3, "undo: the take is gone, the three stay (%d)", n);
        sy_loop_step(15, 48);                            /* step 16: a note */
        run(20);
        sy_loop_events(&n);
        CHECK(n == 4 && sy_tel.loop_sel == 15, "a step toggled on (%d)", n);
        sy_loop_step(15, 48);
        run(20);
        sy_loop_events(&n);
        CHECK(n == 3, "...and off again (%d)", n);
        sy_drums(0);
        run(300);
        CHECK(sy_tel.gates == 0, "stopped: nothing hangs");
        sy_loop_clear();
        run(20);
        sy_loop_events(&n);
        CHECK(n == 0, "cleared");
    }
    CHECK(sy_tel.dropped == 0u, "no command lost");

    printf("the room and the tears\n");
    {
        int t0, t1, t2, e1, e2;
        sy_set(P_GUNK, 0);
        sy_set(P_DRUMS, 0);
        sy_set(P_BASS, 0);
        sy_set(P_CHORUS, 0);
        sy_set(P_TAPE, 0);
        sy_set(P_FRY, 0);
        sy_set(P_ATTACK, 0);
        sy_set(P_RELEASE, 0);
        sy_set(P_DELAY, 0);
        sy_set(P_REVERB, 100);
        t0 = room_t60(0, 4000);
        t1 = room_t60(50, 8000);
        t2 = room_t60(100, 8000);
        CHECK(t0 > 600 && t0 < 1600, "a closet: about a second (%d ms)", t0);
        CHECK(t1 > t0 + 500 && t1 < 3500, "a bigger room rings longer (%d ms)", t1);
        CHECK(t2 < 0, "the void: still going after 8 s");
        {   /* the walls are int16, rounded: a tail must still fade to nothing, not hang on as a hum */
            float tail;
            quiet();
            sy_set(P_SIZE, 50);
            run(20);
            for (i = 48; i < 72; i += 4)
                sy_note_on(i, 127);
            run(500);
            for (i = 48; i < 72; i += 4)
                sy_note_off(i);
            run(25000);
            tail = level(500);
            CHECK(tail < 1e-5f, "the room fades to nothing (%.1e after 25 s)", (double)tail);
        }
        sy_panic();
        sy_set(P_SIZE, 100);
        sy_set(P_DELAY, 100);
        sy_set(P_FEEDBACK, 100);
        sy_set(P_TIME, 0);
        bad = 0;
        peak = 0.0f;
        for (i = 48; i < 72; i += 4)
            sy_note_on(i, 127);
        run(10000);
        for (i = 48; i < 72; i += 4)
            sy_note_off(i);
        run(10000);
        CHECK(!bad && peak < 1.0f, "flat out (Size, Feedback): no runaway (peak %.2f)", (double)peak);
        sy_set(P_REVERB, 0);
        sy_set(P_SIZE, 0);                              /* (the void would ring on under the echoes) */
        sy_set(P_FEEDBACK, 0);
        e1 = first_echo(0);
        e2 = first_echo(50);
        CHECK(e1 > 45 && e1 < 80, "Time 0: the first tear at ~60 ms (%d)", e1);
        CHECK(e2 > 340 && e2 < 420, "Time 50: at ~380 ms (%d)", e2);
        sy_set(P_DELAY, 22);
        sy_set(P_FEEDBACK, 45);
        sy_set(P_TIME, 50);
        sy_set(P_SIZE, 62);
        sy_set(P_REVERB, 38);
    }
    printf(fails ? "SYNTH TESTS FAILED (%d)\n" : "synth: all passed\n", fails);
    return fails != 0;
}
