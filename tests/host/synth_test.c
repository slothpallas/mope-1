/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1's instrument on the host, without libm (as on the device): every style's bar fits, nothing
 * aliases, nothing blows up, envelopes take their time, voices get stolen politely; demo WAVs of the
 * tables and of every drum style land in OUTDIR for listening.
 *   synth_test OUTDIR */
#include <stdio.h>
#include <string.h>
#include "synth.h"
#include "fastmath.h"

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
            x = fm_fabsf(x);
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
    float c = 2.0f * fm_cosf(FM_TWO_PI * hz / 44100.0f), s1 = 0.0f, s2 = 0.0f, w;
    int i;
    for (i = 0; i < 8192; i++) {
        w = 0.5f - 0.5f * fm_cosf(FM_TWO_PI * (float)i / 8192.0f);
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

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int s, t, i;
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
    sy_note_on(96, 100);                                /* C7, 2093 Hz */
    run(300);
    capture();
    {
        float f0 = 2093.0f, sig = goertzel(f0), worst = 0.0f;
        int h;
        for (h = 1; h < 10; h++) {                      /* between the harmonics: where aliases would land */
            float a = goertzel(f0 * (float)h + f0 * 0.37f), b = goertzel(f0 * (float)h + f0 * 0.71f);
            worst = a > worst ? a : worst;
            worst = b > worst ? b : worst;
        }
        CHECK(worst < sig * 1e-5f, "C7 on SYNC: aliases %.1f dB under the fundamental",
              (double)(10.0f * fm_log2f(worst / sig + 1e-20f) * 0.30103f));
    }
    quiet();

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
    printf(fails ? "SYNTH TESTS FAILED (%d)\n" : "synth: all passed\n", fails);
    return fails != 0;
}
