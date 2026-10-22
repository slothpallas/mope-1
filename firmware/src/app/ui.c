/* SPDX-License-Identifier: GPL-3.0-only */
/* MOPE-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too. (Its frame, knob acceleration and autosave, is FiMba-1's and FoMni's.)
 *
 * No menus. Hold a part's button: its drawer opens and the four knobs turn that part; let go and it
 * slides away, the knobs back on the body (turning one then shows the body's drawer a moment):
 *
 *   HOME  BODY       Table  Position  Detune  Sub
 *   EDIT  EYES       Cutoff  Reso  Env amount  Mod decay
 *   ENV   MOUTH      Attack  Decay  Sustain  Release
 *   LFO   VIBE       Rate  Depth  Shape  Destination
 *   FX    TEARS      Chorus  Delay  Time  Feedback
 *   SEL   WALLS      Reverb  Size  Fry  Tape
 *   SEQ   TAPE DECK  Style  Tempo  Swing  Human
 *   GLO   MIXER      Voice  Drums  Bass  Gunk (the room's texture: rain, crackle; louder when idle)
 *   GLO tap, then hold: SETUP  Glide  MIDI channel  MIDI out  Tune
 *   ARP   ARP        Mode  Rate  Octaves  Gate (a tap: the arpeggiator on / off; held: its page)
 *
 *   ALGORITHM  the instrument: only its wave (the table)   PRESETS  a whole saved sound (wave, filter,
 *   envelope, LFO, effects; its card shows what it brought)   SELECT  the drum tape (style)
 *   A page button opens that part's schematic under the blob (the envelope's shape, the LFO's wave, the
 *   filter's curve...); HOME again, or a while untouched, hides it: the blob alone, bigger
 *   PLAY    the drums on / off       REC  hold (the keys stay down)          ARP  the arpeggiator
 *   OCT- / OCT+  the octave          SAVE  saves (held: back to the defaults?)
 *   The 27 keys play F3..G5. The lowest and the highest together: it gives up. */

enum { PG_BODY, PG_EYES, PG_MOUTH, PG_VIBE, PG_TEARS, PG_WALLS, PG_DECK, PG_SETUP, PG_MIX, PG_ARP, NPAGES };
static const char *const PAGE_NAME[NPAGES] = {"BODY", "EYES", "MOUTH", "VIBE", "TEARS", "WALLS", "TAPE DECK", "SETUP",
                                              "MIXER", "ARP"};
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_EDIT, B_ENV, B_LFO, B_FX, B_SEL, B_SEQ, B_GLO, B_GLO, B_ARP};
/* the knobs' names, under their meters in the schematic */
static const char *const PAGE_LABEL[NPAGES][4] = {
    {"WAVE", "POS", "DTUN", "SUB"}, {"CUT", "RES", "ENV", "MDEC"}, {"ATK", "DEC", "SUS", "REL"},
    {"RATE", "DPTH", "SHPE", "DEST"}, {"CHOR", "DLY", "TIME", "FDBK"}, {"VERB", "SIZE", "FRY", "TAPE"},
    {"STYL", "BPM", "SWNG", "HUMN"}, {"GLID", "CH", "OUT", "TUNE"}, {"VOX", "DRUM", "BASS", "GUNK"},
    {"MODE", "RATE", "OCT", "GATE"}};
#define LINGER_MS 600u                 /* a page's drawer after its button is let go (it slides away) */
#define TOUCH_MS 2500u                 /* the drawer after a knob turned with no button held */
#define TAP_MS 300u                    /* ARP let go sooner: a tap (on / off), not a look at its page */
#define GLO_AGAIN_MS 400u              /* GLO pressed again this soon after: the setup, not the mixer */
/* In the browser one mouse can't hold a button and turn a knob: there the page buttons latch (a click
 * opens the page and leaves it open; the same button again, or another, closes it). */
#ifdef OM_WEB
#define LATCH_PAGES 1
#else
#define LATCH_PAGES 0
#endif
#define K_NONE 255
static const uint8_t PAGE_KNOB[NPAGES][4] = {
    {P_TABLE, P_POS, P_DETUNE, P_SUB},
    {P_CUTOFF, P_RESO, P_ENVAMT, P_MODDEC},
    {P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE},
    {P_LFORATE, P_LFODEPTH, P_LFOSHAPE, P_LFODEST},
    {P_CHORUS, P_DELAY, P_TIME, P_FEEDBACK},
    {P_REVERB, P_SIZE, P_FRY, P_TAPE},
    {P_STYLE, P_TEMPO, P_SWING, P_HUMAN},
    {P_GLIDE, P_MIDICH, P_MIDIOUT, P_TUNE},
    {P_SYNTH, P_DRUMS, P_BASS, P_GUNK},
    {P_ARPMODE, P_ARPRATE, P_ARPOCT, P_ARPGATE},
};
#define SAVE_HOLD_MS 1500u
#define RESET_ASK_MS 5000u
#define KEY0_NOTE 53                   /* the lowest key, F3 */

/* moods: the presets, Table .. Tape (P_TABLE .. P_TAPE) */
#define NMOOD 8
#define MOOD_N (P_TAPE + 1)
static const char *const MOOD_NAME[NMOOD] = {"MONDAY", "3AM", "LEFT ON READ", "RAIN AGAIN", "NPC", "DEEP FRIED",
                                             "CHURCH OF NOTHING", "HOLD MUSIC"};
static const int8_t MOOD[NMOOD][MOOD_N] = {
    {0, 20, 25, 30, 58, 20, 35, 45, 10, 50, 60, 50, 30, 12, 0, 0, 30, 22, 50, 45, 38, 62, 0, 35},
    {1, 40, 15, 20, 45, 35, 50, 55, 35, 60, 70, 65, 15, 30, 0, 0, 45, 35, 65, 55, 60, 85, 0, 50},
    {6, 30, 10, 0, 70, 10, 10, 30, 5, 40, 80, 40, 40, 20, 0, 2, 20, 40, 40, 60, 45, 70, 0, 25},
    {4, 60, 35, 25, 50, 25, 40, 50, 20, 55, 50, 70, 20, 25, 1, 1, 50, 45, 55, 65, 55, 80, 0, 60},
    {2, 55, 0, 40, 65, 30, 45, 25, 2, 30, 40, 20, 60, 10, 2, 0, 0, 15, 30, 30, 15, 30, 0, 10},
    {5, 70, 30, 50, 80, 55, 40, 40, 2, 45, 70, 30, 70, 40, 3, 1, 20, 10, 20, 20, 20, 40, 62, 40},
    {6, 80, 40, 10, 60, 15, 20, 60, 60, 70, 90, 85, 10, 15, 0, 0, 60, 30, 70, 50, 85, 98, 0, 40},
    {0, 10, 20, 10, 55, 10, 20, 40, 15, 50, 70, 55, 55, 25, 0, 2, 70, 20, 45, 30, 30, 40, 0, 70},
};
/* what a level-up feels like */
static const char *const LEVEL_UP[] = {"OK.", "COOL. I GUESS.", "LEVEL UP. NOTHING CHANGED.", "+1 SADNESS",
                                       "ACHIEVEMENT: STILL HERE", "GG", "IT LISTENED.", "NEW SKILL: NONE"};
static const char *const LFO_DEST_NAME[LD_N] = {"TO: SHAPE", "TO: EYES", "TO: VOICE", "TO: VOLUME"};
static const char *const LFO_SHAPE_NAME[LFO_NSHAPE] = {"SWAY", "BOB", "BOUNCE", "NOD", "TWITCH"};
static const char *const ARP_MODE_NAME[ARP_NMODE] = {"UP", "DOWN", "UP AND DOWN", "WANDER", "STAB"};
static const char *const ARP_RATE_NAME[ARP_NRATE] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};

static struct {
    uint8_t page;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t dirty;
    uint32_t act_t;                    /* the last touch or note */
    uint32_t focus_t;                  /* the last page or knob change: the HUD shows for a while */
    int8_t touched;
    char cap[32];
    uint32_t cap_until;
    uint8_t arp, hold, pancake, mood;
    uint8_t panel;                     /* the schematic is open */
    int8_t held_btn;                   /* the page button held down (its page shows), -1 none */
    uint32_t held_t, page_until, glo_rel_t;
    uint32_t preset_t;                 /* when PRESETS last turned: its card shows a moment (0: never) */
    uint8_t key_note[NKEYS];           /* the note each key started (its note off) */
    uint32_t latched;                  /* hold: keys let go while held, still sounding */
    uint32_t save_t, reset_ask;
    uint32_t last_draw;
} ui;

static void ui_caption(const char *s, uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < sizeof ui.cap - 1u && s && s[i]; i++)
        ui.cap[i] = s[i];
    ui.cap[i] = 0;
    ui.cap_until = plat_ms() + ms;
}
void ui_say(const char *a, const char *b) { ui_caption(b && b[0] ? b : a, 1500u); }
int ui_dirty(void) { return ui.dirty != 0; }

/* levels: every level takes a little longer than the last */
static uint32_t xp_for(uint32_t lv) { return 24u * lv * lv; }
static uint32_t xp_level(uint32_t xp)
{
    uint32_t lv = 0;
    while (xp_for(lv + 1u) <= xp && lv < 1000u)
        lv++;
    return lv;
}

#include "px.c"
#include "blob.c"

/* ------------------------------------------------------------ values --- */
static void mark_dirty(void) { ui.dirty = 1; }

static void knob_set(int k, int v)
{
    const sy_param_t *p = sy_param_info(k);
    v = v < p->lo ? p->lo : v > p->hi ? p->hi : v;
    if (v == proj.par[k])
        return;
    proj.par[k] = (int16_t)v;
    sy_set(k, v);
    mark_dirty();
}

static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (range <= 24 || !a || accel_off)
        return s;
    if (a > 1)
        dt /= a;
    m = dt < 15u ? 6u : dt < 30u ? 4u : dt < 60u ? 2u : 1u;
    return s * (int32_t)m;
}

/* the few words it says, for values that are choices */
static void say_choice(int k)
{
    switch (k) {
    case P_TABLE: {                                      /* the instrument's wave alone, nothing else */
        static char b[16];
        const char *n = SY_TABLE_NAME[proj.par[k]];
        int i = 0;
        b[0] = 'W', b[1] = 'A', b[2] = 'V', b[3] = 'E', b[4] = ':', b[5] = ' ';
        for (; n[i] && i < 9; i++)
            b[6 + i] = n[i];
        b[6 + i] = 0;
        ui_caption(b, 1300u);
        break;
    }
    case P_STYLE: ui_caption(sy_style_name(proj.par[k]), 1500u); break;
    case P_LFODEST: ui_caption(LFO_DEST_NAME[proj.par[k]], 1200u); break;
    case P_LFOSHAPE: ui_caption(LFO_SHAPE_NAME[proj.par[k]], 1200u); break;
    case P_MIDIOUT: ui_caption(proj.par[k] ? "MIDI OUT: YES" : "MIDI OUT: NO", 1200u); break;
    case P_ARPMODE: ui_caption(ARP_MODE_NAME[proj.par[k]], 1200u); break;
    case P_ARPRATE: ui_caption(ARP_RATE_NAME[proj.par[k]], 1200u); break;
    case P_ARPOCT: ui_caption(proj.par[k] == 1 ? "ONE OCTAVE" : proj.par[k] == 2 ? "TWO OCTAVES" : "THREE OCTAVES", 1200u); break;
    case P_MIDICH: {
        static char b[16];
        int c = proj.par[k];
        if (!c) {
            ui_caption("MIDI: EVERY CHANNEL", 1200u);
        } else {                                     /* (setup is the one place a number is honest) */
            b[0] = 'C';
            b[1] = 'H';
            b[2] = ' ';
            b[3] = (char)(c >= 10 ? '1' : '0' + c);
            b[4] = (char)(c >= 10 ? '0' + c - 10 : 0);
            b[5] = 0;
            ui_caption(b, 1200u);
        }
        break;
    }
    default: break;
    }
}

static void turn(int role, int k, int32_t e)
{
    const sy_param_t *p = sy_param_info(k);
    if (!e || k == K_NONE)
        return;
    knob_set(k, proj.par[k] + accel(role, e, p->hi - p->lo));
    if (k == P_STYLE)                                    /* a new tape: its own tempo */
        knob_set(P_TEMPO, sy_style_tempo(proj.par[P_STYLE]));
    say_choice(k);
}

static void set_page(int pg)
{
    ui.preset_t = 0;                                     /* (the preset's card gives way) */
    ui.panel = 1;
    ui.page = (uint8_t)pg;
    ui.focus_t = plat_ms();
    ui.touched = -1;
}

static void load_mood(int m)
{
    int i;
    ui.mood = (uint8_t)m;
    for (i = 0; i < MOOD_N; i++)
        knob_set(i, MOOD[m][i]);
    ui.preset_t = plat_ms() | 1u;                        /* its card: the name, and everything it changed */
}

/* -------------------------------------------------------------- notes --- */
static int midi_ch(void) { return proj.par[P_MIDICH] ? proj.par[P_MIDICH] - 1 : 0; }
static void note_out(int on, int n, int vel)
{
    uint32_t st;
    if (!proj.par[P_MIDIOUT])
        return;
    st = (on ? 0x90u : 0x80u) | (uint32_t)midi_ch();
    plat_midi_out(st >> 4 | st << 8 | (uint32_t)n << 16 | (uint32_t)(on ? vel : 0) << 24);
}
static void xp_gain(void)
{
    uint32_t before = xp_level(proj.xp);
    proj.xp++;
    if (xp_level(proj.xp) > before) {
        ui_caption(LEVEL_UP[xp_level(proj.xp) % (sizeof LEVEL_UP / sizeof LEVEL_UP[0])], 2500u);
        mark_dirty();
    }
}
static void play(int n, int vel)
{
    sy_note_on(n, vel);
    note_out(1, n, vel);
    xp_gain();
}
static void stop(int n)
{
    sy_note_off(n);
    note_out(0, n, 0);
}

static void key_down(int k)
{
    int n = KEY0_NOTE + k + 12 * proj.par[P_OCTAVE];
    n = n < 0 ? 0 : n > 127 ? 127 : n;
    if (ui.latched >> k & 1u) {                          /* held already (hold): again */
        stop(ui.key_note[k]);
        ui.latched &= ~(1u << k);
    }
    ui.key_note[k] = (uint8_t)n;
    play(n, 100);
}
static void key_up(int k)
{
    if (ui.hold) {
        ui.latched |= 1u << k;
        return;
    }
    stop(ui.key_note[k]);
}
static void release_latched(void)
{
    int k;
    for (k = 0; k < NKEYS; k++)
        if ((ui.latched >> k & 1u) && !(ui.keys >> k & 1u))
            stop(ui.key_note[k]);
    ui.latched = 0;
}

/* ------------------------------------------------------------ buttons --- */
static void reset_defaults(void)
{
    int i;
    uint32_t xp = proj.xp;                               /* (the levels stay: it remembers) */
    project_defaults();
    proj.xp = xp;
    for (i = 0; i < P_NPARAMS; i++)
        sy_set(i, proj.par[i]);
    mark_dirty();
    ui_caption("FORGOT EVERYTHING.", 1800u);
}

static void button(int b)
{
    int i;
    if (ui.reset_ask) {
        ui.reset_ask = 0;
        if (b == B_SAVE) {
            reset_defaults();
            return;
        }
        ui_caption("KEPT.", 900u);
        return;
    }
    if (LATCH_PAGES && b != B_ARP && b == ui.held_btn && !(b == B_GLO && ui.page == PG_MIX)) {
        ui.held_btn = -1;                                /* (latched: the same button again closes it) */
        ui.page_until = plat_ms() + LINGER_MS;
        return;
    }
    if (b == B_GLO) {                                    /* held: the mixer; a tap first (latched: again): the setup */
        set_page(LATCH_PAGES ? (ui.held_btn == B_GLO ? PG_SETUP : PG_MIX)
                             : plat_ms() - ui.glo_rel_t < GLO_AGAIN_MS ? PG_SETUP : PG_MIX);
        ui.held_btn = (int8_t)b;
        return;
    }
    for (i = 0; i < NPAGES; i++)                         /* a part's button: its page while held */
        if (PAGE_BTN[i] == b) {
            set_page(i);
            ui.held_btn = (int8_t)b;
            ui.held_t = plat_ms();
            return;
        }
    switch (b) {
    case B_PLAY:
        sy_drums(!sy_tel.drums_on);
        ui_caption(sy_tel.drums_on ? "STOP." : sy_style_name(proj.par[P_STYLE]), 1200u);
        break;
    case B_REC:
        ui.hold = !ui.hold;
        if (!ui.hold)
            release_latched();
        ui_caption(ui.hold ? "HOLDING ON." : "LET GO.", 1200u);
        break;
    case B_OCTDN:
    case B_OCTUP:
        knob_set(P_OCTAVE, proj.par[P_OCTAVE] + (b == B_OCTUP ? 1 : -1));
        ui_caption(b == B_OCTUP ? "UP. WHY." : "DOWN.", 900u);
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            ui_caption("SAVED. FOR WHAT.", 1300u);
        } else {
            ui_caption("COULD NOT SAVE.", 1500u);
        }
        break;
    default: break;
    }
}

/* a page button let go: its drawer slides away (ARP let go quickly: a tap, the arpeggiator on / off) */
static void button_up(int b)
{
    const uint32_t now = plat_ms();
    if (b != ui.held_btn)
        return;
    if (LATCH_PAGES && (b != B_ARP || now - ui.held_t >= TAP_MS))
        return;                                          /* (latched: it stays until clicked again) */
    ui.held_btn = -1;
    ui.page_until = now + LINGER_MS;
    if (b == B_GLO)
        ui.glo_rel_t = now;
    if (b == B_ARP && now - ui.held_t < TAP_MS) {
        ui.arp = !ui.arp;
        sy_arp(ui.arp);
        ui_caption(ui.arp ? ARP_MODE_NAME[proj.par[P_ARPMODE]] : "ARP: OFF", 1200u);
        if (ui.arp)
            ui.page_until = now + 1500u;                 /* (a look at what it will play) */
    }
}

static void midi_cc(uint32_t cc, uint32_t v)
{
    switch (cc) {
    case 1: sy_mod((int)v); break;
    case 7: knob_set(P_VOLUME, (int)(v * 100u / 127u)); break;
    case 71: knob_set(P_RESO, (int)(v * 100u / 127u)); break;
    case 74: knob_set(P_CUTOFF, (int)(v * 100u / 127u)); break;
    case 73: knob_set(P_ATTACK, (int)(v * 100u / 127u)); break;
    case 72: knob_set(P_RELEASE, (int)(v * 100u / 127u)); break;
    case 91: knob_set(P_REVERB, (int)(v * 100u / 127u)); break;
    case 93: knob_set(P_CHORUS, (int)(v * 100u / 127u)); break;
    case 94: knob_set(P_DELAY, (int)(v * 100u / 127u)); break;
    case 70: knob_set(P_POS, (int)(v * 100u / 127u)); break;
    case 123: sy_panic(); break;
    default: break;
    }
}

static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu, type = st & 0xF0u;
        if (st >= 0xF0u)
            continue;
        if (proj.par[P_MIDICH] && (st & 0x0Fu) + 1u != (uint32_t)proj.par[P_MIDICH])
            continue;
        if (type == 0x90u && d2) {
            sy_note_on((int)d1, (int)d2);
            xp_gain();
        } else if (type == 0x80u || type == 0x90u) {
            sy_note_off((int)d1);
        } else if (type == 0xB0u) {
            midi_cc(d1, d2);
        } else if (type == 0xC0u) {
            load_mood((int)(d1 % NMOOD));
        } else if (type == 0xE0u) {
            sy_bend((int)(d2 << 7 | d1) - 8192);
        }
        ui.act_t = plat_ms();
    }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            if (i == B_SAVE)
                ui.save_t = plat_ms();
            else
                button((int)i);                         /* the rest act on the press */
        } else if (i == B_SAVE && !(ui.btn_used & m)) {
            button((int)i);
        } else if (i != B_SAVE) {
            button_up((int)i);
        }
    }
    if ((btn >> B_SAVE & 1u) && !(ui.btn_used >> B_SAVE & 1u) && plat_ms() - ui.save_t >= SAVE_HOLD_MS) {
        ui.reset_ask = plat_ms() + RESET_ASK_MS;         /* SAVE held: forget everything? */
        ui.btn_used |= 1u << B_SAVE;
        ui_caption("FORGET ALL? SAVE: YES", RESET_ASK_MS);
    }
    if (ui.reset_ask && (int32_t)(plat_ms() - ui.reset_ask) >= 0) {
        ui.reset_ask = 0;
        ui_caption("KEPT.", 900u);
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            if (keys >> i & 1u)
                key_down((int)i);
            else
                key_up((int)i);
        }
    /* the lowest and the highest key together: it gives up, flat */
    {
        const uint32_t both = 1u | 1u << (NKEYS - 1);
        uint8_t p = (keys & both) == both;
        if (p && !ui.pancake)
            ui_caption("SAME.", 2000u);
        ui.pancake = p;
    }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_ALGO)) != 0) {                  /* the instrument: its table */
        knob_set(P_TABLE, (proj.par[P_TABLE] + (e > 0 ? 1 : SY_NTABLE - 1)) % SY_NTABLE);
        say_choice(P_TABLE);
        set_page(PG_BODY);
        ui.panel = 1;
        ui.touched = 0;
        ui.page_until = plat_ms() + TOUCH_MS;
        ui.act_t = plat_ms();
    }
    if ((e = plat_enc(EN_SELECT)) != 0) {                /* the drum tape */
        knob_set(P_STYLE, (proj.par[P_STYLE] + (e > 0 ? 1 : ST_N - 1)) % ST_N);
        knob_set(P_TEMPO, sy_style_tempo(proj.par[P_STYLE]));
        say_choice(P_STYLE);
        set_page(PG_DECK);
        ui.touched = 0;
        ui.page_until = plat_ms() + TOUCH_MS;
        ui.act_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {                /* the preset: its mood */
        load_mood((ui.mood + (e > 0 ? 1 : NMOOD - 1)) % NMOOD);
        ui.act_t = plat_ms();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            if (ui.held_btn < 0 && (int32_t)(plat_ms() - ui.page_until) > 0)
                set_page(PG_BODY);                       /* nothing held: the knobs are the body's */
            turn(EN_K1 + (int)i, PAGE_KNOB[ui.page][i], e);
            ui.touched = (int8_t)i;
            ui.panel = 1;
            if (ui.held_btn < 0)
                ui.page_until = plat_ms() + TOUCH_MS;
            ui.preset_t = 0;
            ui.focus_t = ui.act_t = plat_ms();
        }
}

/* ------------------------------------------------------------- lights --- */
static void leds(void)
{
    uint32_t b = 1u << PAGE_BTN[ui.page];
    if (sy_tel.drums_on && sy_tel.step % (sy_tel.steps / 4u ? sy_tel.steps / 4u : 4u) != 0u)
        b |= 1u << B_PLAY;                               /* lit, dark on each beat */
    if (ui.hold)
        b |= 1u << B_REC;
    if (ui.arp)
        b |= 1u << B_ARP;
    plat_leds(b, ui.keys | ui.latched, 0u);
}

/* AUTOSAVE: a few seconds after a change, when it is quiet (a flash erase silences the audio a moment) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || sy_tel.voices || sy_tel.drums_on ||
        sy_tel.busy > 0.0005f)                           /* (the gunk is not "playing") */
        return;
    if (project_save() == 0)
        ui.dirty = 0;
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.page = PG_BODY;
    ui.act_t = ui.focus_t = plat_ms();
    ui.panel = 0;
    ui.held_btn = -1;
    px_init();
    blob_init();
}

void ui_frame(void)
{
    input();
    autosave();
    if (ui.panel && ui.held_btn < 0 && (int32_t)(plat_ms() - ui.page_until) > 0) {
        ui.panel = 0;                                    /* nothing held, nothing turned: the blob alone */
        ui.page = PG_BODY;
    }
    if (plat_ms() - ui.last_draw >= 32u) {               /* ~30 pictures a second */
        ui.last_draw = plat_ms();
        blob_draw();
    }
    leds();
}

void ui_input_only(void) { input(); }
