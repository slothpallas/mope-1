/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1's instrument: a wavetable synth that is tired, and a drum machine that plays anyway.
 *
 *   VOICES  SY_NVOICE voices. Each: two wavetable oscillators (the same table, detuned apart and
 *           panned apart), a sine sub an octave down, a per-voice drift, a resonant low-pass (Simper's
 *           SVF) with drive, an amp ADSR and a mod envelope (attack-decay) on the cutoff.
 *   TABLES  SY_NTABLE tables of SY_NFRAME frames, generated at boot from harmonic recipes (no flash
 *           cost), mipmapped: 256 / 128 / 64 / 32 samples, fewer harmonics each, so nothing aliases.
 *           Position morphs between neighbouring frames.
 *   LFO     one, global: sine, triangle, square, saw, random; to position, cutoff, pitch or volume.
 *   FX      fry (drive and crush), chorus, the tears (a ping-pong delay, half rate, int16, each echo
 *           darker and thinner, a tape's wobble), the room (eight breathing walls: a feedback delay
 *           network), tape (saturation, wow, a darker top), limiter.
 *   DRUMS   synthesised: kick (808 and gabber), snare, clap, hats, perc, a sliding 808 bass. Styles,
 *           like an old toy keyboard's, but newer: drill, city pop, gabber... each a main bar, a
 *           fill, a tempo and a swing. The auto-bass plays the style's bass line on the lowest held key.
 *
 * Threads: sy_render() runs in the audio interrupt. Everything else posts commands that the render
 * drains at the start of a block. sy_tel is written by the render, read by the screen. Float only,
 * no libm: mopemath.h. */
#pragma once
#include <stdint.h>

#define SY_SR 44100.0f
#define SY_NVOICE 6                    /* raise once the FM-1's budget is measured */
#define SY_NTABLE 16
#define SY_NFRAME 8

/* parameters. All saved; ranges in sy_param_info(). New ones go at the end (old saves keep loading). */
enum {
    P_TABLE, P_POS, P_DETUNE, P_SUB,                 /* BODY:  the blob's shape */
    P_CUTOFF, P_RESO, P_ENVAMT, P_MODDEC,            /* EYES:  the filter */
    P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE,         /* MOUTH: the amp envelope */
    P_LFORATE, P_LFODEPTH, P_LFOSHAPE, P_LFODEST,    /* VIBE:  the LFO */
    P_CHORUS, P_DELAY, P_TIME, P_FEEDBACK,           /* TEARS: chorus copies and the delay */
    P_REVERB, P_SIZE, P_FRY, P_TAPE,                 /* WALLS: the room, the fry, the tape */
    P_STYLE, P_TEMPO, P_DRUMS, P_BASS,               /* TAPE DECK: drums; MIX: their levels (Bass 0: no bass) */
    P_GLIDE, P_OCTAVE, P_MIDICH, P_MIDIOUT,          /* SETUP */
    P_TUNE, P_VOLUME,
    P_SYNTH, P_GUNK, P_SWING, P_HUMAN,               /* MIX: the voices' level, the texture; DECK: feel */
    P_ARPMODE, P_ARPRATE, P_ARPOCT, P_ARPGATE,       /* ARP */
    P_LOOPBARS,                                      /* LOOP: 1, 2 or 4 bars */
    P_BTABLE, P_BTUNE, P_BMEET, P_BLEVEL,            /* SHADOW: oscillator B (its table, semitones, how it meets A) */
    P_FTYPE, P_DRIVE,                                /* EYES: the filter's kind, a push into it */
    P_MATTACK, P_MWAVE,                              /* SIGH: the mod envelope's attack (0: the mouth's), its sweep of the table */
    P_LFO2RATE, P_LFO2DEPTH, P_LFO2SHAPE, P_LFO2DEST, /* TREMBLE: the second LFO */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} sy_param_t;
const sy_param_t *sy_param_info(int i);

enum { LFO_SINE, LFO_TRI, LFO_SQUARE, LFO_SAW, LFO_RANDOM,
       LFO_RISE,                                         /* a slow rise, then a drop */
       LFO_DRIFT,                                        /* random, but smooth: a new place every cycle, glided to */
       LFO_STEPS,                                        /* four steps up */
       LFO_THROB,                                        /* two quick pulses, then rest: a heartbeat */
       LFO_NSHAPE };
/* the LFO's value for a shape at a phase 0..1 (DRIFT and RANDOM: from the two random places given) */
float sy_lfo_shape(int shape, float phase, float from, float to);
enum { ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_WANDER, ARP_STAB, ARP_NMODE };
#define ARP_NRATE 6                                 /* 1/4 1/8 1/8T 1/16 1/16T 1/32 */
enum { LD_POS, LD_CUTOFF, LD_PITCH, LD_VOLUME, LD_SHADOW, LD_N };   /* where an LFO goes */
enum { MEET_MIX, MEET_FM, MEET_RING, MEET_SYNC, MEET_N };             /* how oscillator B meets A */
enum { FT_LOW, FT_BAND, FT_HIGH, FT_NOTCH, FT_COMB, FT_N };           /* the filter's kinds */

/* drum styles: each named after its genre, sadly (Grief drill, Empty city pop, Gloom gabber, Missed phonk
 * call, Jersey cry club, Heartbreakcore, Amapia-no, Cold footwork, Dembow out, Vaporweep, Hyperflop,
 * Funeral bossa, Office ambivalent). The bass under them is always in a minor key (synth.c). */
enum {
    ST_DRILL, ST_CITYPOP, ST_GABBER, ST_PHONK, ST_JERSEY, ST_BREAKCORE, ST_AMAPIANO, ST_FOOTWORK,
    ST_DEMBOW, ST_VAPOR, ST_HYPERPOP, ST_FUNERAL, ST_OFFICE, ST_N
};
const char *sy_style_name(int s);
/* a drum style's step, as written (synth.c): track 0 kick 1 snare 2 clap 3 hat 4 open hat 5 perc 6 bass;
 * fill 0 the main bar, 1 the fill. '.' nothing; x o - hits; bass: digits, letters, _ */
enum { SY_TR_KICK, SY_TR_SNARE, SY_TR_CLAP, SY_TR_HAT, SY_TR_OHAT, SY_TR_PERC, SY_TR_BASS, SY_TR_N };
char sy_style_step(int s, int track, int fill, int step);
int sy_style_steps(int s);
int sy_style_tempo(int s);                         /* its usual tempo, bpm */

extern const char *const SY_TABLE_NAME[SY_NTABLE];

/* ---- commands (main loop -> render). The main loop is the only producer. */
void sy_init(void);                                 /* builds the tables: ~a few 100 ms on the device */
extern void (*sy_boot_tick)(void);                  /* if set, called during sy_init (feed the watchdog) */
void sy_set(int param, int value);
void sy_note_on(int note, int vel);                 /* MIDI note 0..127 (the octave is the caller's) */
void sy_note_off(int note);
void sy_bend(int v);                                /* -8192..8191: +-2 semitones */
void sy_mod(int v);                                 /* mod wheel 0..127: adds to position */
void sy_drums(int on);                              /* the tape deck: play / stop (starts on a bar) */
void sy_arp(int on);                                /* the arpeggiator: held notes become its pattern */
int sy_arp_held(uint8_t *out, int max);

/* ---- the loop: what you play, recorded against the drums' clock, played back with them. 192 ticks a
 * bar (a 1/16 is 12); 1, 2 or 4 bars (P_LOOPBARS). REC while stopped: a bar's count-in, then it records;
 * while playing: it records at once, on top (overdub). Notes snap to the 1/16 grid. */
#define SY_TPB 192                                  /* ticks a bar */
#define SY_LOOP_MAX 192                             /* notes in the loop */
typedef struct {
    uint16_t t;                                     /* where it starts, ticks from the loop's start */
    uint8_t note, vel, len, flags;                  /* len: ticks (up to 255), flags: the engine's */
} sy_ev_t;
void sy_rec(int on);                                /* record on / off (off and stopped: a count-in, then on) */
void sy_loop_undo(void);                            /* the loop as it was before this take */
void sy_loop_clear(void);
void sy_loop_step(int step, int note);              /* a 1/16 step: a note there, or none (toggles) */
void sy_loop_select(int step);                      /* a step chosen (-1: none): the knobs edit it */
void sy_loop_nudge(int field, int delta);           /* 0 note, 1 length, 2 velocity: the chosen step's notes, or
                                                     * with none chosen the whole loop's */
void sy_loop_load(const sy_ev_t *ev, int n);        /* from a saved project (ev stays valid until drained) */
int sy_loop_copy(sy_ev_t *dst, int max);            /* into a project, to save */
const sy_ev_t *sy_loop_events(int *n);              /* for the screen: read-only, may change under you */             /* the notes it is cycling, low to high (for the screen) */
int sy_arp_note(int held_n, int seq);               /* the note it plays at step seq over held_n notes (-1: a rest; STAB: the lowest) */
void sy_panic(void);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void sy_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* the shape of a table at a position (0..1), one cycle, phase 0..1: what the screen draws. Read-only
 * tables, safe from any thread once sy_init has run. */
float sy_shape(int table, float pos, float phase);

/* ---- state for the screen (written by the render) */
#define SY_SCOPE 64
typedef struct {
    float amp_env;                /* the loudest voice's amp envelope, 0..1 */
    float mod_env;                /* its mod envelope, 0..1 */
    float lfo;                    /* -1..1 */
    float lfo_phase;              /* 0..1 */
    float lfo2, lfo2_phase;       /* the second LFO (TREMBLE), the same way */
    float shadow;                 /* oscillator B's level now (with the LFOs), 0..1 */
    float wave_env;               /* the loudest voice's sweep of the table (SIGH), -1..1 */
    float wave_pos;               /* where in the table it is now, with every modulation, 0..1 */
    uint8_t amp_stage;            /* the loudest voice's envelope: 0 off, 1 attack, 2 decay, 3 sustain, 4 release */
    float out_peak;               /* the output's peak over the last block, before the limiter */
    float echo;                   /* what the delay is sending back, 0..~1 */
    float verb;                   /* the reverb's level, 0..~1 */
    float scope[SY_SCOPE];        /* the last block, the voices alone, one point per 4 frames */
    uint32_t clips;               /* blocks the limiter had to fight */
    uint32_t notes, steals;       /* notes started, voices stolen (counters) */
    uint32_t kick_n, snare_n, hat_n, perc_n, bass_n;   /* drum hits (counters) */
    uint8_t voices;               /* voices sounding */
    uint8_t gates;                /* keys held down in the voices */
    uint8_t step, bar;            /* the drums: the step in the bar, bars played */
    uint8_t steps;                /* steps in this style's bar */
    uint8_t drums_on, style;      /* playing, the style playing (a new one waits for the bar) */
    int16_t last_note;            /* the newest note, -1 none */
    int16_t bass_note;            /* the auto-bass's last note (MIDI), -1 none */
    uint32_t dropped;             /* commands lost to a full queue */
    float lvl[4];                 /* the mixer: voices, drums, bass, gunk; each its peak over the last block */
    float busy;                   /* the output's peak without the gunk (is anything playing?) */
    uint8_t arp_on;
    uint32_t arp_seq;             /* steps the arpeggiator has played */
    int16_t arp_note;             /* sounding now, -1 none */
    uint8_t rec;                  /* the loop: 0 not recording, 1 counting in, 2 recording */
    uint8_t count;                /* the count-in's beat, 0..3 */
    uint16_t loop_pos;            /* the loop's playhead, ticks */
    uint16_t loop_ticks;          /* its length, ticks */
    uint16_t loop_n;              /* notes in it */
    int16_t loop_sel;             /* the last stepped step (1/16s), -1 none */
    uint32_t loop_ver;            /* changes whenever the loop does (to save it) */
} sy_tel_t;
extern volatile sy_tel_t sy_tel;
