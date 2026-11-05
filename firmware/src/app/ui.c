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
 *   SEQ   LOOP       full screen: the cursor's Step and Note, the note's Length, the loop's Bars
 *         held (or pressed twice): the 16 white keys are its steps, OCT- / OCT+ its bars
 *   SEQ again: TAPE DECK  Style  Tempo  Swing  Human
 *   GLO   MIXER      Voice  Drums  Bass  Gunk (the room's texture: rain, crackle; louder when idle)
 *   GLO tap, then hold: SETUP  Glide  MIDI channel  MIDI out  Tune
 *   ARP   ARP        Mode  Rate  Octaves  Gate (a tap: the arpeggiator on / off; held: its page)
 *
 *   ALGORITHM  the instrument: only its wave (the table)   PRESETS  a whole saved sound (wave, filter,
 *   envelope, LFO, effects; its card shows what it brought)   SELECT  the drum tape (style)
 *   A page button opens that part's schematic under the blob (the envelope's shape, the LFO's wave, the
 *   filter's curve...); HOME again, or a while untouched, hides it: the blob alone, bigger
 *   PLAY    the band on / off (drums, bass, the loop)
 *   REC     record what you play into the loop: stopped, after a bar's count-in; playing, at once, on top
 *           HOME + REC: undo the take    SAVE + REC: clear the loop
 *   ARP     the arpeggiator
 *   OCT- / OCT+  the octave          SAVE  saves (held: back to the defaults?)
 *   The 27 keys play F3..G5. The lowest and the highest together: it gives up. */

enum { PG_BODY, PG_EYES, PG_MOUTH, PG_VIBE, PG_TEARS, PG_WALLS, PG_DECK, PG_SETUP, PG_MIX, PG_ARP, PG_LOOP,
       PG_SHADOW, PG_SIGH, PG_TREMBLE, PG_COPIES, PG_DUST, PG_PUNCH, PG_SAVE, NPAGES };
static const char *const PAGE_NAME[NPAGES] = {"BODY", "EYES", "MOUTH", "VIBE", "TEARS", "WALLS", "TAPE DECK", "SETTINGS",
                                              "MIXER", "ARP", "LOOP", "SHADOW", "SIGH", "TREMBLE", "COPIES", "DUST",
                                              "PUNCH", "SAVE AS"};
/* the button that opens it (PG_SAVE: none, SAVE's own flow opens it) */
#define B_NONE 255
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_EDIT, B_ENV, B_LFO, B_SEL, B_SEL, B_SEQ, B_GLO, B_GLO, B_ARP, B_SEQ,
                                         B_HOME, B_ENV, B_LFO, B_SEL, B_SEL, B_FX, B_NONE};
/* the next of its button's pages (pressed again, this one open), round: -1 none. SEL goes round four:
 * the walls, the tears, the copies, the dust. */
static const int8_t PAGE_AGAIN[NPAGES] = {PG_SHADOW, -1, PG_SIGH, PG_TREMBLE, PG_COPIES, PG_TEARS, PG_LOOP, PG_MIX, PG_SETUP,
                                          -1, PG_DECK, PG_BODY, PG_MOUTH, PG_VIBE, PG_DUST, PG_WALLS, -1, -1};
/* which of its button's pages it is (0 its first), and its bit in proj.seen (found: no more nudges) */
static const uint8_t PAGE_NTH[NPAGES] = {0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 2, 3, 0, 0};
static const uint8_t PAGE_SEEN_BIT[NPAGES] = {0, 0, 0, 0, 32, 0, 16, 8, 0, 0, 0, 1, 2, 4, 64, 128, 0, 0};
/* the knobs' names, under their meters in the schematic */
static const char *const PAGE_LABEL[NPAGES][4] = {
    {"WAVE", "POS", "DTUN", "SUB"}, {"CUT", "RES", "TYPE", "DRIV"}, {"ATK", "DEC", "SUS", "REL"},
    {"RATE", "DPTH", "SHPE", "DEST"}, {"DLY", "TIME", "FDBK", "DARK"}, {"VERB", "SIZE", "TONE", "ROOM"},
    {"STYL", "BPM", "SWNG", "HUMN"}, {"MOVE", "SET", "", ""}, {"BPM", "VOX", "DRUM", "BASS"},
    {"MODE", "RATE", "OCT", "GATE"}, {"STEP", "NOTE", "LEN", "BARS"},
    {"WAVE", "TUNE", "MEET", "LVL"}, {"ATK", "DEC", "WAVE", "EYES"}, {"RATE", "DPTH", "SHPE", "DEST"},
    {"CHOR", "RATE", "DPTH", "WIDE"}, {"TAPE", "CRKL", "DUST", "FRY"}, {"", "", "", ""}, {"CURS", "CHAR", "NAME", "SLOT"}};
#define STAY_MS 3000u                  /* a drawer stays this long after the last touch (a press, a turn) */
#define DOUBLE_MS 350u                 /* a part's button pressed twice this fast: its page stays until HOME */
#define TAP_MS 300u                    /* ARP let go sooner: a tap (on / off), not a look at its page */
#define K_NONE 255
static const uint8_t PAGE_KNOB[NPAGES][4] = {
    {P_TABLE, P_POS, P_DETUNE, P_SUB},
    {P_CUTOFF, P_RESO, P_FTYPE, P_DRIVE},
    {P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE},
    {P_LFORATE, P_LFODEPTH, P_LFOSHAPE, P_LFODEST},
    {P_DELAY, P_TIME, P_FEEDBACK, P_DDARK},          /* the tears: the delay */
    {P_REVERB, P_SIZE, P_RTONE, P_GUNK},             /* the walls: the room, its rain */
    {P_STYLE, P_TEMPO, P_SWING, P_HUMAN},
    {P_GLIDE, P_MIDICH, P_MIDIOUT, P_TUNE},
    {P_TEMPO, P_SYNTH, P_DRUMS, P_BASS},             /* the mixer: the one tempo, the levels */
    {P_ARPMODE, P_ARPRATE, P_ARPOCT, P_ARPGATE},
    {K_NONE, K_NONE, K_NONE, P_LOOPBARS},            /* the loop: the stepped step's note, length, velocity */
    {P_BTABLE, P_BTUNE, P_BMEET, P_BLEVEL},          /* the shadow: oscillator B */
    {P_MATTACK, P_MODDEC, P_MWAVE, P_ENVAMT},        /* the sigh: the mod envelope, to the table and the eyes */
    {P_LFO2RATE, P_LFO2DEPTH, P_LFO2SHAPE, P_LFO2DEST},   /* the tremble: the second LFO */
    {P_CHORUS, P_CHRATE, P_CHDEPTH, P_CHWIDTH},      /* the copies: the chorus */
    {P_TAPE, P_CRACKLE, P_DUST, P_FRY},              /* the dust: an old record */
    {K_NONE, K_NONE, K_NONE, K_NONE},                /* the punch: the keys, not the knobs */
    {K_NONE, K_NONE, K_NONE, K_NONE},                /* save as: its own knobs (the name, the slot) */
};
_Static_assert(sizeof PAGE_KNOB / sizeof PAGE_KNOB[0] == NPAGES && sizeof PAGE_LABEL / sizeof PAGE_LABEL[0] == NPAGES &&
                   sizeof PAGE_NAME / sizeof PAGE_NAME[0] == NPAGES && sizeof PAGE_BTN == NPAGES && sizeof PAGE_AGAIN == NPAGES &&
                   sizeof PAGE_NTH == NPAGES && sizeof PAGE_SEEN_BIT == NPAGES,
               "a row for every page");
#define SAVE_HOLD_MS 1500u
#define RESET_ASK_MS 5000u
#define KEY0_NOTE 53                   /* the lowest key, F3 */

/* moods: the presets, Table .. Tape (P_TABLE .. P_TAPE) */
/* The presets: whole sounds (Table .. Tape, P_TABLE .. P_TAPE), plus an octave and a glide, in six
 * kinds. Columns: table pos detune sub | cutoff reso env mdecay | A D S R | lfo rate depth shape dest |
 * chorus delay time feedback | reverb size fry tape; then B table, B tune, B meets, B level | filter, drive |
 * mod attack, env wave | lfo2 rate depth shape dest | level (each preset's loudness, evened out) */
enum { CAT_PAD, CAT_KEYS, CAT_PLUCK, CAT_LEAD, CAT_BASS, CAT_FX, CAT_VOX, NCAT };
static const char *const CAT_NAME[] = {"PAD", "KEYS", "PLUCK", "LEAD", "BASS", "FX", "VOX"};
_Static_assert(sizeof CAT_NAME / sizeof CAT_NAME[0] == NCAT, "a name for each");
#define MOOD_N (P_TAPE + 1)
#define MOOD_X (P_LEVEL - P_BTABLE + 1)
#define MOOD_NN (P_VMODE - P_RTONE + 1)
typedef struct {
    const char *name;
    uint8_t cat;
    int8_t oct, glide;
    int8_t v[MOOD_N];
    int8_t x[MOOD_X];                    /* the second pages and the level: B table .. LFO2 dest, Level */
    uint8_t hn;                          /* 1: its own newer effect knobs (n); 0: as they come */
    int8_t n[MOOD_NN];                   /* walls' tone, tears' dark, copies' rate depth width, crackle, dust, voices */
} mood_t;
static const mood_t MOODS[] = {
    {"MONDAY", CAT_PAD, 0, 0, {0, 20, 25, 30, 58, 20, 35, 45, 10, 50, 60, 50, 30, 12, 0, 0, 30, 22, 50, 45, 38, 62, 0, 20},
     {9, 12, 0, 20, 0, 0, 0, 0, 55, 0, 1, 2, -2}, 0, {0}},
    {"3AM", CAT_PAD, 0, 0, {1, 40, 15, 30, 52, 35, 50, 55, 35, 60, 75, 65, 15, 30, 0, 0, 45, 35, 65, 55, 60, 85, 0, 25},
     {0, 7, 0, 25, 0, 0, 60, 30, 20, 15, 0, 3, 1}, 0, {0}},
    {"RAIN AGAIN", CAT_PAD, 0, 0, {4, 60, 35, 25, 50, 25, 40, 50, 20, 55, 50, 70, 20, 25, 1, 1, 50, 45, 55, 65, 55, 80, 0, 30},
     {0, -12, 0, 0, 0, 0, 0, -40, 70, 20, 4, 1, 3}, 0, {0}},
    {"CHURCH OF NOTHING", CAT_PAD, 0, 0, {6, 80, 40, 10, 60, 15, 20, 60, 60, 70, 90, 85, 10, 15, 0, 0, 60, 30, 70, 50, 85, 98, 0, 10},
     {6, -12, 0, 35, 0, 0, 0, 0, 10, 15, 0, 0, -18}, 0, {0}},
    {"SUNDAY SCARIES", CAT_PAD, 0, 0, {1, 70, 45, 25, 40, 20, 30, 70, 75, 80, 80, 90, 8, 40, 1, 0, 70, 30, 75, 55, 75, 90, 0, 35},
     {10, 1, 0, 15, 0, 0, 0, 0, 15, 20, 6, 4, 8}, 0, {0}},
    {"SEEN 2:14", CAT_PAD, 0, 0, {9, 45, 30, 10, 42, 15, 25, 55, 45, 60, 75, 75, 12, 30, 6, 0, 55, 30, 60, 50, 55, 85, 0, 15},
     {0, -12, 0, 0, 3, 0, 70, 50, 55, 0, 1, 2, 10}, 0, {0}},
    {"WEIGHTED BLANKET", CAT_PAD, 0, 0, {15, 50, 20, 15, 36, 10, 20, 60, 50, 70, 75, 80, 8, 25, 5, 1, 40, 20, 55, 40, 50, 78, 0, 40},
     {1, -12, 0, 30, 0, 25, 0, 0, 55, 0, 1, 2, -15}, 0, {0}},
    {"THE VOID STARES", CAT_PAD, 0, 0, {14, 30, 25, 0, 42, 25, 30, 70, 65, 80, 72, 95, 10, 35, 6, 0, 60, 35, 80, 60, 45, 100, 0, 0},
     {14, -24, 2, 30, 0, 0, 0, 0, 8, 30, 0, 4, -6}, 0, {0}},
    {"SLOW HEART", CAT_PAD, 0, 0, {11, 40, 15, 15, 42, 10, 15, 50, 30, 60, 80, 70, 15, 55, 8, 3, 40, 20, 50, 40, 50, 75, 0, 10},
     {0, -12, 0, 0, 0, 0, 0, 0, 55, 0, 1, 2, -16}, 0, {0}},
    {"LEFT ON READ", CAT_KEYS, 0, 0, {6, 30, 10, 0, 70, 10, 10, 30, 5, 40, 80, 40, 40, 12, 0, 3, 20, 40, 40, 60, 45, 70, 0, 10},
     {8, 12, 0, 15, 0, 15, 0, -30, 55, 0, 1, 2, -22}, 0, {0}},
    {"HOLD MUSIC", CAT_KEYS, 0, 0, {0, 10, 20, 10, 55, 10, 20, 40, 15, 50, 70, 55, 55, 12, 0, 2, 70, 20, 45, 30, 30, 40, 0, 70},
     {0, -12, 0, 0, 1, 0, 0, 0, 55, 0, 1, 2, 3}, 0, {0}},
    {"OPEN PLAN", CAT_KEYS, 0, 0, {0, 30, 20, 25, 58, 15, 45, 35, 1, 55, 30, 40, 45, 15, 0, 0, 40, 20, 40, 30, 30, 45, 5, 15},
     {0, 12, 1, 25, 0, 0, 0, 0, 55, 0, 1, 2, 41}, 0, {0}},
    {"MUSIC BOX", CAT_KEYS, 0, 0, {13, 20, 5, 0, 65, 15, 25, 30, 1, 45, 40, 50, 30, 0, 0, 2, 20, 35, 45, 40, 45, 65, 0, 20},
     {13, 24, 0, 30, 0, 0, 0, 0, 55, 0, 1, 2, 34}, 0, {0}},
    {"GLASS OFFICE", CAT_KEYS, 0, 0, {8, 35, 12, 10, 68, 15, 30, 35, 1, 55, 45, 55, 40, 10, 1, 1, 45, 25, 40, 35, 40, 55, 0, 5},
     {8, 19, 1, 20, 0, 0, 0, 0, 55, 0, 1, 2, 33}, 0, {0}},
    {"ELEVATOR", CAT_KEYS, 0, 0, {14, 25, 10, 15, 64, 10, 20, 30, 1, 50, 55, 45, 50, 20, 0, 3, 50, 20, 35, 30, 35, 50, 0, 30},
     {11, 12, 0, 20, 0, 0, 0, 0, 55, 0, 1, 2, -3}, 0, {0}},
    {"NPC", CAT_PLUCK, 0, 0, {2, 55, 0, 45, 72, 30, 45, 25, 2, 40, 62, 20, 60, 10, 2, 0, 0, 15, 30, 30, 15, 30, 0, 10},
     {2, 7, 3, 40, 0, 0, 0, 0, 55, 0, 1, 2, 6}, 0, {0}},
    {"READ RECEIPT", CAT_PLUCK, 0, 0, {3, 50, 10, 30, 62, 35, 70, 25, 1, 35, 10, 30, 0, 0, 0, 0, 20, 40, 45, 50, 30, 55, 0, 10},
     {0, -12, 0, 0, 4, 0, 0, 0, 55, 0, 1, 2, 60}, 0, {0}},
    {"BUBBLE WRAP", CAT_PLUCK, 1, 0, {1, 70, 5, 30, 72, 40, 80, 30, 1, 42, 10, 25, 70, 20, 4, 0, 10, 30, 25, 35, 25, 40, 0, 0},
     {0, -12, 0, 0, 0, 20, 0, 60, 55, 0, 1, 2, 60}, 0, {0}},
    {"TYPING...", CAT_PLUCK, 0, 0, {13, 70, 5, 25, 74, 25, 60, 24, 0, 44, 20, 25, 0, 0, 0, 0, 10, 40, 30, 45, 25, 40, 0, 20},
     {3, 12, 0, 20, 0, 0, 0, 0, 55, 0, 1, 2, 60}, 0, {0}},
    {"ICICLE", CAT_PLUCK, 0, 0, {14, 60, 8, 30, 78, 30, 62, 25, 0, 46, 20, 30, 0, 0, 0, 0, 15, 45, 50, 50, 40, 65, 0, 5},
     {14, 24, 0, 30, 0, 0, 0, 0, 55, 0, 1, 2, 46}, 0, {0}},
    {"MAIN CHARACTER", CAT_LEAD, 0, 25, {7, 55, 25, 20, 70, 35, 40, 50, 3, 50, 75, 35, 45, 8, 0, 2, 25, 35, 45, 40, 30, 50, 10, 10},
     {7, -12, 0, 35, 0, 30, 0, 0, 55, 0, 1, 2, -12}, 0, {0}},
    {"LOW BATTERY", CAT_LEAD, 0, 10, {4, 60, 15, 10, 60, 20, 20, 40, 5, 40, 80, 40, 55, 20, 0, 2, 20, 30, 55, 45, 35, 60, 0, 70},
     {0, -12, 0, 0, 0, 0, 0, 0, 25, 20, 7, 3, -36}, 0, {0}},
    {"DEEP FRIED", CAT_LEAD, 0, 0, {5, 70, 30, 50, 80, 55, 40, 40, 2, 45, 70, 30, 70, 40, 3, 1, 20, 10, 20, 20, 20, 40, 62, 20},
     {5, 7, 1, 35, 0, 70, 0, 0, 55, 0, 1, 2, -7}, 0, {0}},
    {"CRY ON THE FLOOR", CAT_LEAD, 0, 0, {10, 50, 30, 15, 66, 45, 40, 45, 3, 50, 80, 40, 45, 40, 5, 0, 30, 35, 45, 45, 35, 55, 15, 15},
     {10, 12, 0, 25, 0, 0, 40, 40, 60, 6, 0, 2, 10}, 0, {0}},
    {"BENT OUT OF SHAPE", CAT_LEAD, 0, 0, {12, 40, 20, 15, 70, 30, 45, 40, 2, 45, 75, 35, 50, 35, 7, 0, 25, 30, 40, 35, 30, 50, 20, 30},
     {12, 5, 3, 50, 0, 20, 0, 0, 55, 0, 1, 2, -28}, 0, {0}},
    {"DRAMA", CAT_LEAD, 0, 30, {15, 70, 25, 10, 72, 40, 35, 45, 4, 50, 80, 40, 55, 20, 1, 1, 30, 40, 50, 50, 40, 60, 30, 10},
     {15, -12, 0, 40, 0, 25, 0, 0, 55, 0, 1, 2, -31}, 0, {0}},
    {"SUB-PAR", CAT_BASS, -1, 0, {0, 0, 5, 90, 35, 10, 20, 30, 2, 40, 85, 15, 20, 0, 0, 0, 0, 0, 40, 0, 8, 30, 0, 5},
     {0, -12, 0, 0, 0, 10, 0, 0, 55, 0, 1, 2, -24}, 0, {0}},
    {"BOTTOM TEXT", CAT_BASS, -1, 0, {2, 50, 60, 50, 40, 25, 30, 35, 3, 50, 80, 20, 15, 30, 0, 0, 10, 0, 40, 0, 10, 30, 15, 20},
     {2, 12, 3, 30, 0, 30, 0, 0, 55, 0, 1, 2, -13}, 0, {0}},
    {"DOOMSCROLL", CAT_BASS, -1, 35, {7, 30, 10, 50, 33, 75, 75, 35, 1, 40, 55, 15, 0, 0, 0, 0, 0, 20, 30, 30, 10, 30, 25, 10},
     {0, -12, 0, 0, 0, 45, 0, -50, 55, 0, 1, 2, 11}, 0, {0}},
    {"BASEMENT", CAT_BASS, -1, 0, {5, 40, 15, 60, 45, 30, 40, 40, 2, 45, 70, 20, 10, 20, 1, 1, 0, 0, 40, 0, 12, 25, 40, 25},
     {3, -12, 0, 20, 0, 40, 0, 0, 55, 0, 1, 2, -9}, 0, {0}},
    {"HOLLOW INSIDE", CAT_BASS, -1, 0, {11, 70, 5, 50, 40, 25, 45, 30, 1, 45, 70, 20, 0, 0, 0, 0, 0, 0, 30, 0, 10, 30, 10, 10},
     {11, 12, 2, 20, 3, 0, 0, 0, 55, 0, 1, 2, 1}, 0, {0}},
    {"UNDERSLEPT", CAT_BASS, -1, 0, {15, 60, 15, 60, 38, 35, 40, 35, 1, 45, 80, 20, 20, 30, 6, 1, 0, 0, 30, 0, 10, 25, 25, 30},
     {15, 7, 1, 30, 0, 0, 30, 30, 55, 0, 1, 2, -32}, 0, {0}},
    {"TINNITUS", CAT_FX, 1, 0, {7, 100, 70, 0, 90, 70, 0, 50, 70, 80, 100, 90, 30, 40, 4, 1, 50, 60, 60, 70, 80, 95, 20, 0},
     {8, 24, 2, 30, 1, 0, 0, 0, 5, 10, 6, 2, 33}, 0, {0}},
    {"DIAL TONE", CAT_FX, 0, 0, {3, 90, 5, 30, 75, 40, 10, 30, 1, 50, 100, 40, 3, 50, 3, 1, 0, 50, 70, 60, 40, 60, 35, 40},
     {0, 4, 0, 50, 0, 0, 0, 0, 55, 0, 1, 2, -58}, 0, {0}},
    {"GHOSTED", CAT_FX, 0, 0, {1, 20, 80, 20, 45, 30, 20, 60, 78, 90, 80, 100, 5, 70, 0, 0, 100, 70, 85, 75, 95, 100, 0, 30},
     {0, -12, 0, 0, 4, 0, 0, 0, 10, 25, 6, 1, -16}, 0, {0}},
    {"HEAVY BREATHING", CAT_FX, 0, 0, {9, 100, 40, 0, 70, 40, 20, 60, 50, 70, 90, 85, 8, 70, 5, 3, 50, 40, 60, 55, 70, 85, 0, 20},
     {0, -12, 0, 0, 1, 0, 80, 40, 55, 0, 1, 2, 41}, 0, {0}},
    {"BROKEN RECORD", CAT_FX, 0, 0, {13, 80, 5, 0, 70, 30, 30, 30, 1, 50, 70, 40, 55, 40, 7, 2, 20, 50, 30, 60, 40, 60, 40, 80},
     {0, -12, 0, 0, 0, 0, 0, 0, 40, 50, 2, 3, -11}, 0, {0}},
    {"PHANTOM VIBRATE", CAT_FX, 0, 0, {14, 90, 20, 0, 60, 50, 20, 40, 5, 50, 80, 50, 60, 60, 8, 3, 30, 30, 40, 40, 45, 70, 10, 0},
     {14, -12, 2, 50, 0, 0, 0, 0, 85, 30, 2, 1, 10}, 0, {0}},
    /* 0.4: sounds for the new modulation and effects (LFOs into the tables, the detune, the walls, the
     * tears, the dust; the copies, the dust, the vowel filter) */
    {"RAGE QUIT", CAT_BASS, -1, 0, {0, 100, 75, 40, 42, 30, 30, 35, 1, 45, 85, 20, 15, 20, 0, 1, 15, 0, 40, 0, 10, 30, 10, 15},
     {0, -12, 0, 0, 0, 45, 0, 0, 12, 45, 0, LD_DETUNE, -14}, 1, {50, 50, 30, 50, 100, 0, 0, VM_LEGATO}},
    {"PASSIVE AGGRESSIVE", CAT_BASS, -1, 0, {2, 40, 10, 50, 45, 35, 40, 30, 1, 45, 80, 20, 45, 35, 3, LD_TABLE, 0, 0, 40, 0, 8, 30, 15, 10},
     {0, 0, MEET_FM, 45, 0, 30, 0, 0, 55, 0, 1, 2, -10}, 0, {0}},
    {"OVERDRAFT", CAT_BASS, -1, 0, {0, 100, 10, 40, 50, 30, 55, 30, 1, 40, 75, 15, 25, 30, 0, LD_POS, 0, 0, 40, 0, 8, 30, 20, 10},
     {0, 7, MEET_SYNC, 60, 0, 50, 0, 30, 55, 0, 1, 2, -14}, 0, {0}},
    {"LANDLORD", CAT_BASS, -1, 0, {0, 30, 5, 90, 30, 25, 60, 20, 0, 35, 70, 15, 20, 0, 0, 0, 0, 0, 40, 0, 6, 25, 45, 20},
     {0, -12, 0, 0, 0, 10, 0, 0, 55, 0, 1, 2, -8}, 1, {50, 50, 30, 50, 100, 10, 20, VM_MONO}},
    {"BURNOUT", CAT_BASS, -1, 0, {11, 60, 5, 40, 40, 70, 30, 40, 1, 45, 75, 20, 10, 25, 0, LD_CUTOFF, 0, 0, 40, 0, 8, 30, 20, 10},
     {0, -12, 0, 0, FT_COMB, 25, 0, 0, 55, 0, 1, 2, -7}, 0, {0}},
    {"SCREAM INTO PILLOW", CAT_LEAD, 0, 15, {0, 100, 15, 10, 70, 55, 35, 40, 3, 50, 85, 35, 60, 0, 0, 0, 20, 30, 45, 40, 30, 50, 25, 10},
     {0, 12, MEET_SYNC, 70, 0, 35, 0, 0, 62, 12, 0, LD_PITCH, -28}, 1, {50, 60, 30, 50, 100, 0, 0, VM_LEGATO}},
    {"NOISE COMPLAINT", CAT_LEAD, 0, 0, {3, 50, 20, 10, 72, 35, 30, 40, 2, 45, 80, 35, 20, 50, 0, LD_TEARS, 15, 45, 40, 50, 25, 45, 30, 15},
     {0, 7, MEET_FM, 45, 0, 40, 0, 0, 55, 0, 1, 2, -27}, 1, {50, 70, 30, 50, 100, 0, 0, VM_MONO}},
    {"ALL CAPS", CAT_LEAD, 0, 0, {5, 60, 25, 10, 62, 75, 30, 35, 2, 45, 85, 30, 35, 20, 0, LD_CUTOFF, 20, 25, 45, 35, 25, 45, 35, 10},
     {0, -12, 0, 0, FT_BAND, 60, 0, 0, 55, 0, 1, 2, -17}, 0, {0}},
    {"REPLY ALL", CAT_LEAD, 0, 25, {0, 100, 55, 15, 55, 30, 55, 45, 3, 50, 80, 35, 50, 10, 0, LD_CUTOFF, 50, 25, 45, 40, 30, 50, 0, 15},
     {0, -12, 0, 0, 0, 0, 0, 0, 55, 0, 1, 2, 2}, 1, {50, 50, 40, 60, 100, 0, 0, VM_UNISON}},
    {"CHOIR OF ONE", CAT_VOX, 0, 0, {0, 100, 35, 10, 40, 60, 0, 40, 60, 60, 85, 70, 8, 30, 0, LD_CUTOFF, 40, 15, 55, 40, 60, 70, 0, 20},
     {9, 0, MEET_MIX, 25, FT_VOWEL, 0, 0, 0, 15, 35, 0, LD_COPIES, -10}, 1, {40, 50, 20, 60, 100, 0, 0}},
    {"HUMMING ALONE", CAT_VOX, 0, 0, {0, 40, 10, 30, 85, 45, 0, 40, 20, 50, 90, 55, 20, 10, 6, LD_CUTOFF, 20, 10, 40, 30, 40, 55, 0, 40},
     {9, 0, MEET_MIX, 20, FT_VOWEL, 0, 0, 0, 55, 0, 1, 2, -42}, 1, {40, 50, 30, 50, 100, 20, 15}},
    {"AAH WELL", CAT_VOX, 0, 30, {0, 100, 20, 10, 15, 55, 25, 50, 5, 50, 85, 40, 55, 0, 0, LD_CUTOFF, 20, 25, 45, 40, 30, 50, 0, 10},
     {0, -12, 0, 0, FT_VOWEL, 10, 30, 0, 60, 10, 0, LD_PITCH, -10}, 0, {0}},
    {"SAYS NOTHING", CAT_VOX, 0, 0, {1, 40, 15, 10, 20, 55, 60, 60, 10, 55, 70, 50, 10, 30, 6, LD_TABLE, 25, 20, 40, 40, 40, 55, 0, 15},
     {0, -12, 0, 0, FT_VOWEL, 0, 40, 40, 55, 0, 1, 2, 13}, 0, {0}},
    {"WORN TAPE", CAT_KEYS, 0, 0, {0, 30, 15, 20, 60, 15, 25, 35, 1, 50, 50, 45, 15, 25, 0, LD_DUST, 15, 15, 40, 30, 30, 45, 0, 60},
     {0, 12, MEET_MIX, 15, 0, 0, 0, 0, 55, 0, 1, 2, 2}, 1, {40, 50, 30, 50, 100, 50, 55}},
    {"TIDE GOES OUT", CAT_PAD, 0, 0, {6, 40, 30, 20, 55, 20, 20, 45, 50, 60, 85, 75, 6, 45, 0, LD_WALLS, 30, 20, 50, 45, 50, 75, 0, 20},
     {0, -12, 0, 0, 0, 0, 0, 0, 6, 30, 0, LD_TABLE, -15}, 1, {30, 50, 30, 50, 100, 0, 0}},
    {"SECOND GUESSING", CAT_PLUCK, 0, 0, {13, 60, 20, 20, 68, 30, 55, 25, 0, 40, 20, 30, 65, 40, 7, LD_DETUNE, 30, 30, 45, 45, 30, 50, 0, 10},
     {8, 12, MEET_MIX, 20, 0, 0, 0, 0, 55, 0, 1, 2, 60}, 1, {50, 80, 45, 60, 100, 0, 0}},
};
#define NMOOD ((int)(sizeof MOODS / sizeof MOODS[0]))
/* what a level-up feels like */
static const char *const LEVEL_UP[] = {"OK.", "COOL. I GUESS.", "LEVEL UP. NOTHING CHANGED.", "+1 SADNESS",
                                       "ACHIEVEMENT: STILL HERE", "GG", "IT LISTENED.", "NEW SKILL: NONE"};
static const char *const LFO_DEST_NAME[] = {"TO: WAVE POS", "TO: EYES", "TO: VOICE", "TO: VOLUME", "TO: SHADOW",
                                            "TO: WAVETABLE", "TO: DETUNE", "TO: THE WALLS", "TO: THE TEARS",
                                            "TO: THE COPIES", "TO: THE DUST"};
_Static_assert(sizeof LFO_DEST_NAME / sizeof LFO_DEST_NAME[0] == LD_N, "a name for each");
static const char *const MEET_NAME[] = {"MIX: BESIDE IT", "FM: INSIDE IT", "RING: AROUND IT", "SYNC: TIED TO IT"};
_Static_assert(sizeof MEET_NAME / sizeof MEET_NAME[0] == MEET_N, "a name for each");
static const char *const FTYPE_NAME[] = {"LOW-PASS", "BAND-PASS: A SQUINT", "HIGH-PASS: WIDE AWAKE", "NOTCH: A WINK",
                                             "COMB: THE BLINDS", "VOWEL: A MOUTH"};
_Static_assert(sizeof FTYPE_NAME / sizeof FTYPE_NAME[0] == FT_N, "a name for each");
static const char *const LFO_SHAPE_NAME[] = {"SWAY", "BOB", "BOUNCE", "NOD", "TWITCH", "HEAVE", "WANDER", "STAGGER",
                                                          "THROB"};
_Static_assert(sizeof LFO_SHAPE_NAME / sizeof LFO_SHAPE_NAME[0] == LFO_NSHAPE, "a name for each");
static const char *const ARP_MODE_NAME[] = {"UP", "DOWN", "UP AND DOWN", "WANDER", "STAB"};
_Static_assert(sizeof ARP_MODE_NAME / sizeof ARP_MODE_NAME[0] == ARP_NMODE, "a name for each");
static const char *const ARP_RATE_NAME[] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
_Static_assert(sizeof ARP_RATE_NAME / sizeof ARP_RATE_NAME[0] == ARP_NRATE, "a name for each");

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
    uint32_t held_t, page_until;
    uint8_t latch;                     /* a page pressed twice: it stays until HOME */
    int8_t last_up_btn;                /* the page button last let go, and when (a double press) */
    uint32_t last_up_t;
    uint32_t preset_t;                 /* when PRESETS last turned: its card shows a moment (0: never) */
    uint8_t key_note[NKEYS];           /* the note each key started (its note off) */
    uint32_t stepped;                  /* keys pressed as the loop's steps (their preview to stop) */
    uint32_t punched;                   /* keys punching (FX held when they went down): up, the punch ends */
    uint32_t modkeys;                   /* white keys beside a punch, held: they change it (no note) */
    int8_t pk_fx;                       /* the punch the knobs turn (the last one punched), -1 none yet */
    uint8_t loop_bar, step_note;       /* the bar the steps show; the note a new step gets */
    int16_t step_held;                 /* a step key down on a step with notes: let go untouched, they go */
    int16_t cur_step;                  /* SEQ: the cursor's step (1/16s from the loop's start) */
    uint8_t cur_note, prev_on;         /* ...and its note; a preview sounding */
    uint32_t prev_until;               /* ...till then */
    uint8_t preview;                   /* the note a step key is sounding */
    uint8_t step_turned;               /* ...a knob turned meanwhile: they stay (edited) */
    uint32_t loop_ver;
    uint32_t save_t, reset_ask;
    uint32_t last_draw;
    uint8_t naming;                    /* SAVE AS open: the knobs (and typing) edit the name, SAVE keeps it */
    uint8_t set_row;                   /* SETTINGS: the row chosen (knob 1 moves, knob 2 sets) */
    uint8_t mute;                      /* SY_MUTE_*: the drums, the bass, the loop quiet (not saved) */
    int8_t pat_next;                   /* a pattern asked for, waiting for the bar (-1 none) */
    uint8_t tap_n, tap_skip;           /* tap tempo: HOME taps so far (on the tempo pages); a HOME let through */
    uint32_t tap_last, tap_iv[3];      /* the last tap, the intervals between the last few */
    uint8_t save_slot, cursor;
    char name[PRESET_NAME + 1];
    uint32_t name_dice;
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

/* the loop's steps: while its drawer is open (not while recording: then the keys play) */
static int step_mode(void) { return ui.page == PG_LOOP && ui.panel && !sy_tel.rec; }

#define CAT_MINE NCAT                  /* (yours: their own kind on the card) */
static int mood_n(void) { return NMOOD + PRESET_SLOTS; }
static int mood_used(int m) { return m < NMOOD || (m < mood_n() && (presets.used >> (m - NMOOD) & 1u)); }
static const char *mood_name(int m) { return m < NMOOD ? MOODS[m].name : presets.slot[m - NMOOD].name; }
static int mood_cat(int m) { return m < NMOOD ? MOODS[m].cat : CAT_MINE; }
/* ------------------------------------------------------------ settings --- */
/* SETTINGS (GLO twice): the whole screen, no blob. A list, as on the OP-1's: knob 1 goes up and down it,
 * knob 2 sets the row chosen. Words for the choices, a bar for the amounts. */
typedef struct {
    const char *label;
    uint8_t par;
    const char *const *words;           /* 0: a bar (an amount) */
} set_row_t;
static const char *const SW_VOICES[] = {"POLY", "MONO", "LEGATO", "UNISON"};
static const char *const SW_TEMPO[] = {"ONE FOR ALL", "EACH BEAT'S"};
static const char *const SW_ROOM[] = {"RAIN", "FOREST", "CITY NIGHT", "OFFICE", "SEA"};
static const char *const SW_ONOFF[] = {"OFF", "ON"};
static const char *const SW_BASS[] = {"THE BEAT'S", "ROOT", "PULSE", "OCTAVES", "FOLLOW"};
static const char *const SW_MOUT[] = {"OFF", "KEYS", "NOTES", "ALL"};
static const char *const SW_CLOCK[] = {"OWN", "FOLLOW", "LEAD"};
static const char *const SW_PRESS[] = {"NOTHING", "VIBRATO", "FILTER", "WAVE", "LFO"};
_Static_assert(sizeof SW_VOICES / sizeof SW_VOICES[0] == VM_N && sizeof SW_TEMPO / sizeof SW_TEMPO[0] == TM_N &&
               sizeof SW_ROOM / sizeof SW_ROOM[0] == AM_N && sizeof SW_BASS / sizeof SW_BASS[0] == BS_N &&
               sizeof SW_MOUT / sizeof SW_MOUT[0] == MO_N && sizeof SW_CLOCK / sizeof SW_CLOCK[0] == CK_N &&
               sizeof SW_PRESS / sizeof SW_PRESS[0] == AT_N, "a word for each");
static const set_row_t SET_ROWS[] = {
    {"VOICES", P_VMODE, SW_VOICES}, {"GLIDE", P_GLIDE, 0}, {"TEMPO", P_TEMPOMODE, SW_TEMPO}, {"ROOM", P_AMBI, SW_ROOM},
    {"ROOM LEVEL", P_GUNK, 0},      {"AUTO BASS", P_BASSON, SW_ONOFF}, {"BASS WAY", P_BSTYLE, SW_BASS},
    {"MIDI IN", P_MIDICH, 0},       {"MIDI OUT", P_MIDIOUT, SW_MOUT}, {"CLOCK", P_CLOCK, SW_CLOCK},
    {"AFTERTOUCH", P_PRESS, SW_PRESS}, {"TUNE", P_TUNE, 0}};
#define NSET ((int)(sizeof SET_ROWS / sizeof SET_ROWS[0]))
static const char *set_text(int r)                       /* a row's value in words (0: draw it as a bar) */
{
    static char b[8];
    const set_row_t *w = &SET_ROWS[r];
    const int v = proj.par[w->par];
    if (w->words)
        return w->words[v];
    if (w->par == P_MIDICH) {                            /* (a channel is a number: that is what it is) */
        if (!v)
            return "ALL";
        b[0] = 'C', b[1] = 'H', b[2] = ' ';
        b[3] = (char)(v >= 10 ? '1' : '0' + v);
        b[4] = v >= 10 ? (char)('0' + v - 10) : 0;
        b[5] = 0;
        return b;
    }
    return 0;
}

/* SEQ: the loop's length in steps; the note at a step nearest a pitch */
static int loop_steps(void) { return (sy_tel.loop_ticks ? sy_tel.loop_ticks : SY_TPB) / 12; }
static int loop_nearest(int st, int pitch)              /* the note at that step nearest pitch, -1 none */
{
    int i, n, best = -1;
    const sy_ev_t *ev = sy_loop_events(&n);
    for (i = 0; i < n; i++)
        if (ev[i].t / 12 == (uint16_t)st && (best < 0 || (ev[i].note > pitch ? ev[i].note - pitch : pitch - ev[i].note) <
                                                             (best > pitch ? best - pitch : pitch - best)))
            best = ev[i].note;
    return best;
}
/* the punches' knobs: what each one is (its word, and its symbol around the blob) */
enum { PK_DOWN, PK_UP, PK_DEEP, PK_SMEAR, PK_WINDOW, PK_BRAKE, PK_LONG, PK_SLICE, PK_ROLL, PK_SOFT, PK_PITCH, PK_RATE,
       PK_SIZE, PK_SCATTER, PK_BITS, PK_SCREAM, PK_HANG, PK_DRY, PK_SPEED, PK_FILTER, PK_NKIND };
static const char *const PK_NAME[PK_NKIND] = {"HOW LOW", "HOW HIGH", "HOW DEEP", "SMEAR", "WINDOW", "BRAKE", "HOW LONG", "SLICE",
                                              "ROLL", "SOFT", "PITCH", "RATE", "GRAIN SIZE", "SCATTER", "BITS", "SCREAM",
                                              "HANG", "DRY GONE", "SPEED", "FILTER"};
static const uint8_t PK_KIND[PU_N][PU_KN] = {
    {PK_DOWN, PK_SMEAR}, {PK_UP, PK_WINDOW}, {PK_LONG, PK_BRAKE}, {PK_SLICE, PK_SOFT}, {PK_ROLL, PK_PITCH}, {PK_ROLL, PK_PITCH},
    {PK_RATE, PK_DEEP},  {PK_SIZE, PK_SCATTER}, {PK_BITS, PK_RATE}, {PK_FILTER, PK_SCREAM}, {PK_HANG, PK_DRY}, {PK_SPEED, PK_DEEP},
};

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
    if (range <= 48 || !a || accel_off)
        return s;
    if (a > 1)
        dt /= a;
    m = dt < 15u ? 6u : dt < 30u ? 4u : dt < 60u ? 2u : 1u;
    return s * (int32_t)m;
}

/* oscillator B's tuning, in words: no numbers here either */
static void say_interval(int st)
{
    static const char *const IN[12] = {"", "A SEMITONE", "A TONE", "A MINOR THIRD", "A MAJOR THIRD", "A FOURTH",
                                       "A TRITONE", "A FIFTH", "A MINOR SIXTH", "A SIXTH", "A MINOR SEVENTH", "A MAJOR SEVENTH"};
    static char b[32];
    const int a = st < 0 ? -st : st, oct = a / 12, rest = a % 12;
    int i = 0;
    const char *p;
    if (!a) {
        ui_caption("UNISON", 1100u);
        return;
    }
    p = oct == 2 ? "TWO OCTAVES" : oct == 1 ? "AN OCTAVE" : "";
    for (; *p && i < 30; p++)
        b[i++] = *p;
    if (oct && rest)
        for (p = " AND "; *p && i < 30; p++)
            b[i++] = *p;
    for (p = IN[rest]; *p && i < 30; p++)
        b[i++] = *p;
    for (p = st < 0 ? " DOWN" : " UP"; *p && i < 31; p++)
        b[i++] = *p;
    b[i] = 0;
    ui_caption(b, 1300u);
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
    case P_LFOSHAPE: case P_LFO2SHAPE: ui_caption(LFO_SHAPE_NAME[proj.par[k]], 1200u); break;
    case P_LFO2DEST: ui_caption(LFO_DEST_NAME[proj.par[k]], 1200u); break;
    case P_BMEET: ui_caption(MEET_NAME[proj.par[k]], 1200u); break;
    case P_FTYPE: ui_caption(FTYPE_NAME[proj.par[k]], 1200u); break;
    case P_MATTACK: if (!proj.par[k]) ui_caption("WITH THE MOUTH", 1000u); break;
    case P_BTUNE: say_interval(proj.par[k]); break;
    case P_BTABLE: {
        static char b[20];
        const char *n = SY_TABLE_NAME[proj.par[k]];
        int i = 0;
        for (; "SHADOW: "[i]; i++)
            b[i] = "SHADOW: "[i];
        for (; *n && i < 19; n++, i++)
            b[i] = *n;
        b[i] = 0;
        ui_caption(b, 1300u);
        break;
    }
    case P_MIDIOUT: {
        static const char *const W[MO_N] = {"MIDI OUT: NO", "MIDI OUT: THE KEYS", "OUT: KEYS, ARP, LOOP", "OUT: EVERYTHING. DRUMS ON 10"};
        ui_caption(W[proj.par[k]], 1400u);
        break;
    }
    case P_CLOCK: {
        static const char *const W[CK_N] = {"MY OWN TEMPO.", "FOLLOWING THEIR CLOCK.", "THEY FOLLOW ME."};
        ui_caption(W[proj.par[k]], 1400u);
        break;
    }
    case P_PRESS: {
        static const char *const W[AT_N] = {"PRESSING: NOTHING.", "PRESS: IT TREMBLES", "PRESS: EYES OPEN", "PRESS: IT MORPHS", "PRESS: MORE LFO"};
        ui_caption(W[proj.par[k]], 1400u);
        break;
    }
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

/* TEMPO: one for every beat (the setting's default) or each beat its own. One tempo: landing on the beat's
 * own speed, it says so; a new beat far from its speed, it says that too. */
static int tempo_home(void) { const int d = proj.par[P_TEMPO] - sy_style_tempo(proj.par[P_STYLE]); return d >= -1 && d <= 1; }
static void say_tempo_home(void)
{
    static char b[40];
    const char *q, *n = sy_style_name(proj.par[P_STYLE]);
    int i = 0;
    for (q = "FINE. THIS IS "; *q; q++)
        b[i++] = *q;
    for (; *n && i < 30; n++)
        b[i++] = *n;
    for (q = " SPEED."; *q && i < 39; q++)
        b[i++] = *q;
    b[i] = 0;
    ui_caption(b, 1800u);
}
static void beat_changed(void)                           /* a new tape */
{
    if (proj.par[P_TEMPOMODE] == TM_BEAT)
        knob_set(P_TEMPO, sy_style_tempo(proj.par[P_STYLE]));   /* (its own tempo) */
    say_choice(P_STYLE);
    if (proj.par[P_TEMPOMODE] == TM_GLOBAL) {
        const int d = proj.par[P_TEMPO] - sy_style_tempo(proj.par[P_STYLE]);
        static char b[40];
        const char *q, *n = sy_style_name(proj.par[P_STYLE]);
        int i = 0;
        if (d >= -20 && d <= 20)
            return;
        for (; *n && i < 24; n++)
            b[i++] = *n;
        for (q = d > 0 ? ". TOO FAST." : ". TOO SLOW."; *q; q++)
            b[i++] = *q;
        b[i] = 0;
        ui_caption(b, 1600u);
    }
}
static void turn(int role, int k, int32_t e)
{
    const sy_param_t *p = sy_param_info(k);
    const int was_home = k == P_TEMPO && tempo_home();
    if (!e || k == K_NONE)
        return;
    knob_set(k, proj.par[k] + accel(role, e, p->hi - p->lo));
    if (k == P_STYLE) {
        beat_changed();
        return;
    }
    if (k == P_TEMPO && proj.par[P_TEMPOMODE] == TM_GLOBAL && !was_home && tempo_home()) {
        say_tempo_home();                                /* (on the beat's own speed) */
        return;
    }
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

/* ------------------------------------------------------ your presets --- */
/* what a preset is: the sound's settings (the table .. the tape, the octave and glide, the shadow .. its
 * level, the effects' newer knobs), not the mixer, the tempo, the band or the MIDI */
static int is_sound(int i)
{
    return i <= P_TAPE || i == P_GLIDE || i == P_OCTAVE || (i >= P_BTABLE && i <= P_LEVEL) || (i >= P_RTONE && i <= P_VMODE);
}
/* the next preset that way (factory, then yours; empty slots skipped) */
static int mood_step(int from, int dir)
{
    int m = from, k;
    for (k = 0; k < mood_n(); k++) {
        m = (m + (dir > 0 ? 1 : mood_n() - 1)) % mood_n();
        if (mood_used(m))
            return m;
    }
    return from;
}
/* a name, in its voice: two words */
static const char *const NAME_A[] = {"TIRED", "DAMP", "LATE", "GREY", "SOFT", "LOST", "COLD", "NUMB", "STALE", "PALE", "QUIET",
                                     "WET", "SLOW", "LAST", "SPARE", "FLAT", "DULL", "UNSENT"};
static const char *const NAME_B[] = {"MONDAY", "SOCK", "TOAST", "RAIN", "CHAIR", "BUS", "TEA", "SOUP", "WINDOW", "PLANT", "MEMO",
                                     "EMAIL", "TUESDAY", "SHRUG", "NAP", "REGRET", "LAMP", "SIGH"};
static void sad_name(char *out)
{
    const char *a, *b;
    int n = 0;
    ui.name_dice = ui.name_dice * 1664525u + 1013904223u + plat_ms();
    a = NAME_A[(ui.name_dice >> 8) % (sizeof NAME_A / sizeof NAME_A[0])];
    b = NAME_B[(ui.name_dice >> 20) % (sizeof NAME_B / sizeof NAME_B[0])];
    while (*a && n < PRESET_NAME)
        out[n++] = *a++;
    if (n < PRESET_NAME)
        out[n++] = ' ';
    while (*b && n < PRESET_NAME)
        out[n++] = *b++;
    out[n] = 0;
}
static const char NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.!?-:',/+";
static int name_len(void)
{
    int n = PRESET_NAME;
    while (n > 0 && (ui.name[n - 1] == ' ' || ui.name[n - 1] == 0))
        n--;
    return n;
}
static void name_slot(int slot)                          /* a slot chosen: its name, or a new one */
{
    int i;
    ui.save_slot = (uint8_t)slot;
    for (i = 0; i <= PRESET_NAME; i++)
        ui.name[i] = 0;
    if (presets.used >> slot & 1u)
        for (i = 0; i < PRESET_NAME; i++)
            ui.name[i] = presets.slot[slot].name[i];
    else
        sad_name(ui.name);
    for (i = 0; i < PRESET_NAME; i++)                    /* (spaces to the end: the cursor can walk there) */
        if (!ui.name[i])
            ui.name[i] = ' ';
    ui.name[PRESET_NAME] = 0;
    ui.cursor = (uint8_t)(name_len() < PRESET_NAME ? name_len() : PRESET_NAME - 1);
}
static void set_page(int pg);
static void save_open(void)                              /* SAVE tapped: SAVE AS, a slot and a name */
{
    int slot = ui.mood >= NMOOD ? ui.mood - NMOOD : -1, k;
    for (k = 0; slot < 0 && k < PRESET_SLOTS; k++)
        if (!(presets.used >> k & 1u))
            slot = k;                                    /* (yours if it is one of yours, else the first empty) */
    name_slot(slot < 0 ? 0 : slot);
    ui.naming = 1;
    set_page(PG_SAVE);
    ui_caption("NAME IT. SAVE: KEEP.", 1500u);
}
static void save_close(int keep)
{
    ui.naming = 0;
    ui.page_until = plat_ms() + 900u;
    if (!keep) {
        ui_caption("NOT SAVED.", 1000u);
        return;
    }
    {
        preset_slot_t *sl = &presets.slot[ui.save_slot];
        int i;
        for (i = 0; i < PRESET_NAME; i++)
            sl->name[i] = i < name_len() ? ui.name[i] : 0;
        if (!name_len())
            sl->name[0] = '?';                           /* (a name of nothing: still something to find it by) */
        for (i = 0; i < PRESET_NPAR; i++)
            sl->par[i] = i < P_NPARAMS ? proj.par[i] : 0;
        presets.used |= (uint16_t)(1u << ui.save_slot);
        ui.mood = (uint8_t)(NMOOD + ui.save_slot);
        if (presets_save() == 0 && project_save() == 0) {
            static char said[32];
            int n = 0;
            ui.dirty = 0;
            for (const char *q = "KEPT AS "; *q; q++)
                said[n++] = *q;
            for (i = 0; i < name_len() && n < 29; i++)
                said[n++] = ui.name[i];
            said[n++] = '.';
            said[n] = 0;
            ui_caption(said, 1600u);
        } else
            ui_caption("COULD NOT SAVE.", 1500u);
    }
}
static void name_type(int c)                             /* a typed character, or a key that edits */
{
    if (c == 1)
        ui.cursor = ui.cursor ? ui.cursor - 1 : 0;
    else if (c == 2)
        ui.cursor = ui.cursor < PRESET_NAME - 1 ? ui.cursor + 1 : PRESET_NAME - 1;
    else if (c == 8) {
        int i;
        if (ui.cursor)
            ui.cursor--;
        for (i = ui.cursor; i < PRESET_NAME - 1; i++)    /* (the rest moves back) */
            ui.name[i] = ui.name[i + 1];
        ui.name[PRESET_NAME - 1] = ' ';
    } else if (c == 13)
        save_close(1);
    else if (c == 27)
        save_close(0);
    else if (c >= 32 && c < 127) {
        const char u = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
        const char *q;
        for (q = NAME_CHARS; *q && *q != u; q++)
            ;
        if (*q) {                                        /* (only what the screen's font can draw) */
            ui.name[ui.cursor] = u;
            if (ui.cursor < PRESET_NAME - 1)
                ui.cursor++;
        }
    }
    ui.page_until = plat_ms() + STAY_MS;
}
static void name_knob(int i, int32_t e)                  /* SAVE AS: 1 the cursor, 2 its letter, 3 a new name, 4 the slot */
{
    if (i == 0)
        name_type(e > 0 ? 2 : 1);
    else if (i == 1) {
        const int n = (int)sizeof NAME_CHARS - 1;
        int at = 0;
        while (at < n && NAME_CHARS[at] != ui.name[ui.cursor])
            at++;
        at = ((at + (e > 0 ? 1 : -1)) % n + n) % n;
        ui.name[ui.cursor] = NAME_CHARS[at];
    } else if (i == 2) {
        int k;
        sad_name(ui.name);
        for (k = 0; k < PRESET_NAME; k++)
            if (!ui.name[k])
                ui.name[k] = ' ';
        ui.name[PRESET_NAME] = 0;
        ui.cursor = (uint8_t)(name_len() < PRESET_NAME ? name_len() : PRESET_NAME - 1);
    } else
        name_slot((ui.save_slot + (e > 0 ? 1 : PRESET_SLOTS - 1)) % PRESET_SLOTS);
}
const char *ui_name(void) { return ui.naming ? ui.name : 0; }   /* SAVE AS open: the name being made */
int ui_naming(void) { return ui.naming; }

static void load_mood(int m)
{
    int i;
    ui.mood = (uint8_t)m;
    if (m >= NMOOD) {                                    /* one of yours: its sound, as you kept it */
        for (i = 0; i < P_NPARAMS; i++)
            if (is_sound(i))
                knob_set(i, presets.slot[m - NMOOD].par[i]);
        ui.preset_t = plat_ms() | 1u;
        return;
    }
    for (i = 0; i < MOOD_N; i++)
        knob_set(i, MOODS[m].v[i]);
    for (i = 0; i < MOOD_X; i++)
        knob_set(P_BTABLE + i, MOODS[m].x[i]);
    for (i = P_RTONE; i <= P_VMODE; i++)                 /* (the effects' newer knobs and the voices: as they come, unless it says) */
        knob_set(i, MOODS[m].hn ? MOODS[m].n[i - P_RTONE] : sy_param_info(i)->def);
    knob_set(P_OCTAVE, MOODS[m].oct);                    /* (basses sit an octave down, some leads glide) */
    knob_set(P_GLIDE, MOODS[m].glide);
    ui.preset_t = plat_ms() | 1u;                        /* its card: the name, its kind, and what it brought */
}

/* -------------------------------------------------------------- notes --- */
static int midi_ch(void) { return proj.par[P_MIDICH] ? proj.par[P_MIDICH] - 1 : 0; }
static void note_out(int on, int n, int vel)
{
    uint32_t st;
    if (proj.par[P_MIDIOUT] == MO_OFF || (ui.arp && proj.par[P_MIDIOUT] >= MO_NOTES))
        return;                                          /* (the arpeggiator's notes go out instead: the engine's) */
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

static const uint8_t WHITE_K[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* (the white keys, F3 up) */
/* SEQ's black keys: the first four the patterns, the three from F#4 the mutes (drums, bass, loop) */
static const uint8_t PAT_K[SY_PATS] = {1, 3, 5, 8}, MUTE_K[3] = {13, 15, 17};
static int white_of(int k)
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE_K[i] == k)
            return i;
    return -1;
}

/* the punches are on the black keys (FX held, or FX pressed twice: it stays), PU_* each; STUTTER is
 * REPEAT with MORE. The white key below a punching black key: MORE (stronger); above: TWIST (another way). */
static const int8_t PUNCH_KEY[NKEYS] = {-1, PU_HALF, -1, PU_DOUBLE, -1, PU_STOP, -1, -1, PU_REVERSE, -1, PU_REPEAT, -1, -1,
                                        PU_GATE, -1, PU_GRAIN, -1, PU_CRUSH, -1, -1, PU_SWEEP, -1, PU_WASH, -1, -1, PU_WOBBLE, -1};
static int punch_mode(void) { return (ui.btn >> B_FX & 1u) || (ui.latch && ui.page == PG_PUNCH); }
static int punch_mods(int k)                             /* a punching key: its whites beside, held */
{
    return (k > 0 && (ui.modkeys >> (k - 1) & 1u) ? PM_MORE : 0) | (k < NKEYS - 1 && (ui.modkeys >> (k + 1) & 1u) ? PM_TWIST : 0);
}
static void punch_resend(void)                           /* the whites beside changed: every punch hears it */
{
    int k;
    for (k = 0; k < NKEYS; k++)
        if (ui.punched >> k & 1u)
            sy_punch(PUNCH_KEY[k], 1 + punch_mods(k));
}
static const char *const TWIST_SAID[PU_N] = {"A FIFTH DOWN.", "A FIFTH UP.", "AND BACK.", "LONGER.", "IN THREES.", "IN THREES.",
                                             "A PATTERN.", "FROZEN.", "WOBBLY BITS.", "UPWARD.", "BREATHING.", "SEASICK."};
/* what each punch says when it starts, in its voice (PU_* order) */
static const char *const PUNCH_SAID[PU_N] = {"HALF.", "TWICE. WHY.", "STOP.", "BACKWARDS.", "AGAIN AGAIN.", "S-S-STUTTER.",
                                             "ON OFF ON OFF.", "FALLING APART.", "CRUSHED.", "MUFFLED.", "WASHED OUT.",
                                             "WARPED."};
/* ------------------------------------------------------------ the loop --- */
/* SEQ, full screen: a cursor on the roll. Knob 1 its step, knob 2 its note, knob 3 the note's length; a step's
 * white key: there (on its nearest note), and again: that note goes, or one comes. */
static void seq_preview(int note)                       /* a short taste of it */
{
    if (ui.prev_on)
        sy_note_off(ui.preview);
    sy_note_on(note, 90);
    ui.preview = (uint8_t)note;
    ui.prev_on = 1;
    ui.prev_until = plat_ms() + 220u;
}
static void seq_knob(int k, int32_t e)
{
    const int n = loop_steps();
    if (k == 0) {                                        /* the step: along the bars (the bar on show follows) */
        ui.cur_step = (int16_t)(((ui.cur_step + (int)e) % n + n) % n);
        ui.loop_bar = (uint8_t)(ui.cur_step / 16);
        if (loop_nearest(ui.cur_step, ui.cur_note) == ui.cur_note)
            seq_preview(ui.cur_note);
    } else if (k == 1) {                                 /* the note: up and down, a semitone a click */
        const int v = ui.cur_note + (int)e;
        ui.cur_note = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
        seq_preview(ui.cur_note);
    } else if (loop_nearest(ui.cur_step, ui.cur_note) == ui.cur_note)
        sy_loop_len(ui.cur_step, ui.cur_note, e > 0 ? 1 : -1);   /* its length */
    else
        ui_caption("NO NOTE HERE.", 800u);
}

/* the punches' knobs, what each is called (said when turned; the screen shows them as symbols) */
static void punch_knob(int k, int32_t e)
{
    static char b[32];
    const char *nm;
    int v, n = 0;
    if (ui.pk_fx < 0) {
        ui_caption("PUNCH SOMETHING FIRST.", 1200u);
        return;
    }
    v = project_pk(ui.pk_fx, k) + (int)e;
    v = v < 0 ? 0 : v > PU_KMAX ? PU_KMAX : v;
    if (v != project_pk(ui.pk_fx, k)) {
        project_pk_set(ui.pk_fx, k, v);
        sy_punch_knob(ui.pk_fx, k, v);
        mark_dirty();
    }
    nm = PK_NAME[PK_KIND[ui.pk_fx][k]];
    for (; *nm && n < 20; nm++)
        b[n++] = *nm;
    for (nm = v == 0 ? ": NONE." : v == PU_KMAX ? ": ALL OF IT." : e > 0 ? ": MORE." : ": LESS."; *nm && n < 31; nm++)
        b[n++] = *nm;
    b[n] = 0;
    ui_caption(b, 900u);
}

/* PATTERNS: one of four, on the next bar (the one playing goes back to its slot as it is) */
static void pattern_pick(int p)
{
    static char cap[28];
    const int cur = proj.pat;
    int bars, i = 0;
    const char *q;
    proj.pat_bars[cur] = (uint8_t)(proj.par[P_LOOPBARS] + 1);
    bars = proj.pat_bars[p] ? proj.pat_bars[p] - 1 : proj.par[P_LOOPBARS];
    sy_loop_swap(cur, p, bars);
    ui.pat_next = (int8_t)p;
    for (q = "PATTERN "; *q; q++)
        cap[i++] = *q;
    cap[i++] = (char)('1' + p);
    for (q = p == cur ? ": STAYS." : sy_tel.drums_on ? " NEXT BAR" : proj.pat_n[p] ? "." : ": EMPTY."; *q; q++)
        cap[i++] = *q;
    cap[i] = 0;
    ui_caption(cap, 1200u);
    ui.page_until = plat_ms() + STAY_MS;
}
static void mute_toggle(int j)
{
    static const char *const NAME[3][2] = {{"DRUMS: BACK.", "DRUMS: QUIET."}, {"BASS: BACK.", "BASS: QUIET."},
                                           {"LOOP: BACK.", "LOOP: QUIET."}};
    ui.mute ^= (uint8_t)(1u << j);
    sy_mute(ui.mute);
    ui_caption(NAME[j][ui.mute >> j & 1u], 900u);
    ui.page_until = plat_ms() + STAY_MS;
}
static void key_down(int k)
{
    int n = KEY0_NOTE + k + 12 * proj.par[P_OCTAVE];
    n = n < 0 ? 0 : n > 127 ? 127 : n;
    if (punch_mode() && PUNCH_KEY[k] >= 0) {             /* a black key punches (the white ones still play) */
        const int fx = PUNCH_KEY[k];
        ui.punched |= 1u << k;
        ui.pk_fx = (int8_t)fx;                           /* (the knobs turn this one now) */
        sy_punch(fx, 1 + punch_mods(k));
        ui.btn_used |= 1u << B_FX;
        ui_caption(PUNCH_SAID[fx], 900u);
        ui.page_until = plat_ms() + STAY_MS;
        return;
    }
    if (PUNCH_KEY[k] < 0 && ((k > 0 && (ui.punched >> (k - 1) & 1u)) || (k < NKEYS - 1 && (ui.punched >> (k + 1) & 1u)))) {
        const int above = k > 0 && (ui.punched >> (k - 1) & 1u);   /* a white beside a punch: it changes it */
        ui.modkeys |= 1u << k;
        punch_resend();
        if (above)
            ui_caption(TWIST_SAID[PUNCH_KEY[k - 1]], 900u);
        else
            ui_caption("MORE.", 700u);
        ui.page_until = plat_ms() + STAY_MS;
        return;
    }
    if (step_mode()) {   /* a white key: that 1/16 step of the bar on show. Another step: the cursor goes there (on its
                          * nearest note; none, one comes). The cursor's step again: its note goes, or one comes. */
        const int w = white_of(k);
        int j;
        for (j = 0; j < SY_PATS; j++)
            if (k == PAT_K[j])
                pattern_pick(j);
        for (j = 0; j < 3; j++)
            if (k == MUTE_K[j])
                mute_toggle(j);
        if (w >= 0) {
            const int st = ui.loop_bar * 16 + w, near = loop_nearest(st, ui.cur_note);
            if (st != ui.cur_step && near >= 0) {
                ui.cur_step = (int16_t)st;
                ui.cur_note = (uint8_t)near;
            } else {
                const int had = loop_nearest(st, ui.cur_note) == ui.cur_note;
                ui.cur_step = (int16_t)st;
                sy_loop_note(st, ui.cur_note);
                ui_caption(had ? "THAT NOTE: GONE." : "A NOTE.", 700u);
            }
            ui.focus_t = plat_ms();
            ui.page_until = plat_ms() + STAY_MS;
            seq_preview(ui.cur_note);                    /* (you hear it) */
        }
        return;
    }
    ui.key_note[k] = (uint8_t)n;
    ui.step_note = ui.cur_note = (uint8_t)n;             /* (a new step gets the last note you played: SEQ's cursor on it) */
    play(n, 100);
}
static void key_up(int k)
{
    if (ui.punched >> k & 1u) {                          /* a punch lasts until its key is up (FX let go or not) */
        ui.punched &= ~(1u << k);
        sy_punch(PUNCH_KEY[k], 0);
        return;
    }
    if (ui.modkeys >> k & 1u) {
        ui.modkeys &= ~(1u << k);
        punch_resend();
        return;
    }
    if (step_mode() && white_of(k) >= 0)
        return;
    stop(ui.key_note[k]);
}

/* ------------------------------------------------------------ buttons --- */
static void reset_defaults(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)                      /* the settings back as they come (the levels and */
        proj.par[i] = sy_param_info(i)->def;             /* the patterns stay: it remembers) */
    proj.seen = 0;
    memset(proj.pk, 0, sizeof proj.pk);
    memset(proj.pat_bars, 0, sizeof proj.pat_bars);
    project_send_knobs();
    for (i = 0; i < P_NPARAMS; i++)
        sy_set(i, proj.par[i]);
    mark_dirty();
    ui_caption("FORGOT EVERYTHING.", 1800u);
}

/* TAP TEMPO: HOME tapped in time on the tape deck or the mixer: the last few gaps, averaged */
#define TAP_GAP_MS 1300u                                 /* (50 bpm is 1200 ms a beat) */
static void tap_tempo(void)
{
    const uint32_t now = plat_ms();
    if (ui.tap_n && now - ui.tap_last <= TAP_GAP_MS)
        ui.tap_iv[(ui.tap_n - 1) % 3] = now - ui.tap_last;
    else
        ui.tap_n = 0;
    ui.tap_n = (uint8_t)(ui.tap_n < 200 ? ui.tap_n + 1 : ui.tap_n);
    ui.tap_last = now;
    ui.page_until = now + STAY_MS;
    ui.focus_t = ui.act_t = now;
    if (ui.tap_n >= 2) {
        const int k = ui.tap_n - 1 < 3 ? ui.tap_n - 1 : 3;
        uint32_t sum = 0;
        int i, t;
        static char cap[16];
        for (i = 0; i < k; i++)
            sum += ui.tap_iv[i];
        t = (int)((60000u * (uint32_t)k + sum / 2u) / sum);
        if (proj.par[P_CLOCK] == CK_FOLLOW && sy_tel.ext_bpm) {
            ui_caption("THEIR CLOCK DECIDES.", 1200u);
            return;
        }
        knob_set(P_TEMPO, t);
        t = proj.par[P_TEMPO];
        i = 0;
        if (t >= 100)
            cap[i++] = (char)('0' + t / 100);
        cap[i++] = (char)('0' + t / 10 % 10);
        cap[i++] = (char)('0' + t % 10);
        for (const char *q = " BPM"; *q; q++)
            cap[i++] = *q;
        cap[i] = 0;
        ui_caption(cap, 1200u);
    }
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
    if (ui.naming) {                                     /* SAVE AS open: SAVE keeps it, anything else: never mind */
        save_close(b == B_SAVE);
        return;
    }
    if (b == B_HOME && !ui.tap_skip && ui.panel && (ui.page == PG_DECK || ui.page == PG_MIX)) {
        tap_tempo();                                     /* the tempo pages: HOME in time sets it (one tap: HOME) */
        return;
    }
    if (b == B_FX && ui.latch && ui.page == PG_PUNCH && !(plat_ms() - ui.last_up_t < DOUBLE_MS && ui.last_up_btn == B_FX)) {
        ui.latch = 0;                                    /* the punches staying: FX again lets them go */
        ui.held_btn = -1;
        ui.panel = 0;
        ui.page = PG_BODY;
        ui_caption("PUNCHES: AWAY.", 900u);
        return;
    }
    if (b == B_HOME && ui.latch) {                       /* HOME ends a drawer that stays: the blob alone */
        ui.latch = 0;
        ui.held_btn = -1;
        ui.panel = 0;
        ui.page = PG_BODY;
        ui_caption("HOME.", 800u);
        return;
    }
    {
        const uint32_t now = plat_ms();
        const int dbl = b != B_ARP && b == ui.last_up_btn && now - ui.last_up_t < DOUBLE_MS;
        int pg = -1;
        /* A part's button opens its first page; pressed again (its page open, and the last page button let
         * go: REC opening the loop's drawer doesn't count) its second page, and back. Twice fast: it stays. */
        if (dbl)
            pg = ui.page;
        else if (ui.panel && PAGE_BTN[ui.page] == b && ui.last_up_btn == b && PAGE_AGAIN[ui.page] >= 0)
            pg = PAGE_AGAIN[ui.page];
        else
            for (i = 0; pg < 0 && i < NPAGES; i++)
                if (PAGE_BTN[i] == b && !PAGE_NTH[i])
                    pg = i;
        if (pg == PG_LOOP && !(ui.panel && ui.page == PG_LOOP)) {   /* (a fresh look: the cursor on the last note played) */
            sy_loop_select(-1);
            ui.cur_note = ui.step_note;
            if (ui.cur_step >= loop_steps())
                ui.cur_step = 0;
            ui.loop_bar = (uint8_t)(ui.cur_step / 16);
        }
        if (pg >= 0) {                                   /* a part's button: its page while held, and 3 s on */
            const int again = PAGE_AGAIN[pg];
            set_page(pg);
            if (PAGE_NTH[pg] && !(proj.seen & PAGE_SEEN_BIT[pg])) {
                proj.seen |= PAGE_SEEN_BIT[pg];          /* found: no more nudging */
                mark_dirty();
            } else if (!dbl && again >= 0 && PAGE_NTH[again] && !(proj.seen & PAGE_SEEN_BIT[again])) {
                static char nudge[32];                  /* not found yet: a nudge, in its voice */
                const char *p = PAGE_NAME[again];
                int n = 0;
                for (const char *q = "AGAIN: "; *q; q++)
                    nudge[n++] = *q;
                for (; *p && n < 30; p++)
                    nudge[n++] = *p;
                nudge[n++] = '.';
                nudge[n] = 0;
                ui_caption(nudge, 1600u);
            }
            ui.held_btn = (int8_t)b;
            ui.held_t = now;
            if (pg == PG_SETUP || pg == PG_LOOP)
                ui.latch = 1;                            /* (the settings and the loop, full screen: they stay until HOME) */
            if (dbl && !ui.latch) {
                ui.latch = 1;                            /* twice: it stays (other pages too) until HOME */
                ui_caption(pg == PG_PUNCH ? "PUNCHES STAY. FX: AWAY." : "STAYS OPEN.", 1200u);
            }
            return;
        }
    }
    switch (b) {
    case B_PLAY:
        sy_drums(!sy_tel.drums_on);
        ui_caption(sy_tel.drums_on ? "STOP." : sy_style_name(proj.par[P_STYLE]), 1200u);
        break;
    case B_REC:
        if (ui.btn >> B_HOME & 1u) {                     /* HOME + REC: the take undone */
            sy_loop_undo();
            ui_caption("UNDONE.", 1000u);
        } else if (ui.btn >> B_SAVE & 1u) {              /* SAVE + REC: the loop gone (and no save) */
            ui.btn_used |= 1u << B_SAVE;
            sy_loop_clear();
            ui_caption("LOOP: GONE.", 1200u);
        } else if (sy_tel.rec) {
            sy_rec(0);
            ui_caption("GOT IT.", 1000u);
        } else {
            sy_rec(1);
            ui_caption(sy_tel.drums_on ? "REC." : "READY...", 1200u);
            if (!ui.latch) {                             /* the loop's drawer: the count-in, the take going in */
                set_page(PG_LOOP);
                ui.page_until = plat_ms() + (sy_tel.drums_on ? STAY_MS : STAY_MS + 3000u);
            }
        }
        break;
    case B_OCTDN:
    case B_OCTUP:
        if (step_mode()) {                               /* the steps: the next bar, or the one before */
            const int bars = proj.par[P_LOOPBARS] == 2 ? 4 : proj.par[P_LOOPBARS] + 1;
            ui.loop_bar = (uint8_t)((ui.loop_bar + (b == B_OCTUP ? 1 : bars - 1)) % bars);
            ui.cur_step = (int16_t)(ui.loop_bar * 16 + ui.cur_step % 16);   /* (the cursor goes along) */
            ui.focus_t = plat_ms();
            break;
        }
        knob_set(P_OCTAVE, proj.par[P_OCTAVE] + (b == B_OCTUP ? 1 : -1));
        ui_caption(b == B_OCTUP ? "UP. WHY." : "DOWN.", 900u);
        break;
    case B_SAVE:                                         /* SAVE tapped: SAVE AS (the project saves itself, quietly) */
        save_open();
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
    ui.held_btn = -1;
    ui.last_up_btn = (int8_t)b;
    ui.last_up_t = now;
    ui.page_until = now + STAY_MS;                       /* the drawer stays 3 s after the last touch */
    if (b == B_ARP && now - ui.held_t < TAP_MS) {
        ui.arp = !ui.arp;
        sy_arp(ui.arp);
        ui_caption(ui.arp ? ARP_MODE_NAME[proj.par[P_ARPMODE]] : "ARP: OFF", 1200u);
    }
}

static int cc_param;                                    /* CC 102 picks a parameter, CC 103 sets it (0..127 over its range) */
static void midi_cc(uint32_t cc, uint32_t v)
{
    switch (cc) {
    case 104: sy_drums(v >= 64u); break;               /* (the deck, for testing over USB) */
    case 105: sy_punch((int)(v & 63u), v >= 64u); break;   /* (a punch: 64 + n on, n off; for testing over USB) */
    case 102: cc_param = (int)v < P_NPARAMS ? (int)v : P_NPARAMS - 1; break;
    case 103: {
        const sy_param_t *pi = sy_param_info(cc_param);
        knob_set(cc_param, pi->lo + (int)(((int32_t)v * (pi->hi - pi->lo) + 63) / 127));
        break;
    }
    case 1: sy_mod((int)v); break;
    case 64: sy_sustain(v >= 64u); break;               /* the pedal */
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

/* ---------------------------------------------------- a MIDI clock in --- */
/* CLOCK FOLLOW: its tempo from the times of the last clocks (stamped as they arrived), where it is from the
 * count since its Start; both to the engine, which follows. Two seconds without one: its own tempo again. */
#define CK_HIST 25
static struct {
    uint16_t t[CK_HIST];               /* the last clocks' arrival (ms, low 16 bits) */
    uint8_t n, w, started, start_pend;
    uint32_t clocks;                   /* since the first after Start */
    uint32_t seen;                     /* plat_ms of the newest */
    float est, sent;                   /* the tempo: as measured (smoothed), as last sent (0 none) */
} ck;
static void clock_in(uint32_t st, uint32_t stamp)
{
    if (proj.par[P_CLOCK] != CK_FOLLOW)
        return;
    if (st == 0xF8u) {
        ck.t[ck.w] = (uint16_t)stamp;
        ck.w = (uint8_t)((ck.w + 1) % CK_HIST);
        ck.n += ck.n < CK_HIST;
        ck.seen = plat_ms();
        if (ck.start_pend) {                             /* (the first clock after Start: beat one) */
            ck.start_pend = 0;
            ck.started = 1;
            ck.clocks = 0;
            sy_clock_start();
        }
        ck.clocks++;
    } else if (st == 0xFAu) {
        ck.start_pend = 1;
    } else if (st == 0xFBu) {
        sy_drums(1);
    } else if (st == 0xFCu) {
        ck.started = ck.start_pend = 0;
        sy_drums(0);
    }
}
static void clock_frame(void)
{
    const uint32_t now = plat_ms();
    uint16_t newest, span;
    float b, per;
    if (proj.par[P_CLOCK] != CK_FOLLOW || (ck.n && now - ck.seen > 2000u)) {
        if (ck.sent > 0.0f) {
            sy_clock_bpm(0);
            if (proj.par[P_CLOCK] == CK_FOLLOW)
                ui_caption("THEIR CLOCK STOPPED. MINE, THEN.", 1600u);
        }
        memset(&ck, 0, sizeof ck);
        return;
    }
    if (ck.n < 7)
        return;
    newest = ck.t[(ck.w + CK_HIST - 1) % CK_HIST];
    span = (uint16_t)(newest - ck.t[(ck.w + CK_HIST - ck.n) % CK_HIST]);
    if (!span)
        return;
    per = (float)span / (float)(ck.n - 1);               /* ms a clock */
    b = 60000.0f / (24.0f * per);
    if (b < 20.0f || b > 300.0f)
        return;
    ck.est = ck.est > 0.0f ? ck.est + (b - ck.est) * 0.15f : b;
    if (ck.est - ck.sent > 0.05f || ck.sent - ck.est > 0.05f) {
        static char cap[24];
        const int t = (int)(ck.est + 0.5f);
        if (ck.sent <= 0.0f) {                           /* (found one) */
            int i = 0;
            const char *q;
            for (q = "FOLLOWING: "; *q; q++)
                cap[i++] = *q;
            if (t >= 100)
                cap[i++] = (char)('0' + t / 100);
            cap[i++] = (char)('0' + t / 10 % 10);
            cap[i++] = (char)('0' + t % 10);
            for (q = " BPM"; *q; q++)
                cap[i++] = *q;
            cap[i] = 0;
            ui_caption(cap, 1600u);
        }
        ck.sent = ck.est;
        sy_clock_bpm((int)(ck.est * 100.0f + 0.5f));
    }
    if (ck.started) {                                    /* where it is now: its newest clock, and the time since */
        const float d = (float)(uint16_t)((uint16_t)now - newest) / per, since = d < 0.0f ? 0.0f : d > 1.0f ? 1.0f : d;
        const float at = ((float)(ck.clocks - 1u) + since) * (1.0f / 24.0f);
        const int whole = (int)at;
        sy_clock_pos(whole, (int)((at - (float)whole) * 10000.0f));
    }
}

static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu, type = st & 0xF0u;
        if (st >= 0xF0u) {                               /* real time: the clock (its arrival time in the top bytes) */
            clock_in(st, pkt >> 16);
            continue;
        }
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
        } else if (type == 0xD0u) {                      /* channel pressure */
            sy_press((int)d1);
        } else if (type == 0xA0u) {                      /* a key's own pressure: taken as everyone's */
            sy_press((int)d2);
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
        uint8_t p = (keys & both) == both && !punch_mode();   /* (not while punching) */
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
        if (!ui.latch) {                                 /* (a page that stays, stays) */
            set_page(PG_BODY);
            ui.touched = 0;
        }
        ui.panel = 1;
        ui.page_until = plat_ms() + STAY_MS;
        ui.act_t = plat_ms();
    }
    if ((e = plat_enc(EN_SELECT)) != 0) {                /* the drum tape */
        knob_set(P_STYLE, (proj.par[P_STYLE] + (e > 0 ? 1 : ST_N - 1)) % ST_N);
        beat_changed();
        if (!ui.latch) {
            set_page(PG_DECK);
            ui.touched = 0;
        }
        ui.panel = 1;
        ui.page_until = plat_ms() + STAY_MS;
        ui.act_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {                /* the preset: its mood (factory, then yours); SAVE AS: the slot */
        if (ui.naming)
            name_knob(3, e);
        else
            load_mood(mood_step(ui.mood, e));
        ui.act_t = plat_ms();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            if (ui.naming) {
                name_knob((int)i, e);
                ui.page_until = plat_ms() + STAY_MS;
                ui.focus_t = ui.act_t = plat_ms();
                continue;
            }
            if (ui.panel && ui.page == PG_SETUP) {       /* SETTINGS: knob 1 the row, knob 2 its value */
                if (i == 0)
                    ui.set_row = (uint8_t)(((ui.set_row + (int)e) % NSET + NSET) % NSET);
                else if (i == 1) {
                    const set_row_t *w = &SET_ROWS[ui.set_row];
                    const sy_param_t *p = sy_param_info(w->par);
                    const int n = p->hi - p->lo + 1;
                    if (w->words)                        /* (words go round) */
                        knob_set(w->par, p->lo + ((proj.par[w->par] - p->lo + (int)e) % n + n) % n);
                    else
                        knob_set(w->par, proj.par[w->par] + accel(EN_K2, e, p->hi - p->lo));
                }
                ui.focus_t = ui.act_t = plat_ms();
                continue;
            }
            if (ui.panel && ui.page == PG_PUNCH) {       /* PUNCH: the last one punched, its two knobs (3 and 4: nothing) */
                if (i >= PU_KN)
                    continue;
                punch_knob((int)i, e);
                ui.touched = (int8_t)i;
                if (ui.held_btn < 0)
                    ui.page_until = plat_ms() + STAY_MS;
                ui.focus_t = ui.act_t = plat_ms();
                continue;
            }
            if (ui.page == PG_LOOP && i < 3)             /* the cursor: step, note; the note's length */
                seq_knob((int)i, e);
            else
                turn(EN_K1 + (int)i, PAGE_KNOB[ui.page][i], e);
            ui.touched = (int8_t)i;
            ui.panel = 1;
            if (ui.held_btn < 0)
                ui.page_until = plat_ms() + STAY_MS;
            ui.preset_t = 0;
            ui.focus_t = ui.act_t = plat_ms();
        }
}

/* ------------------------------------------------------------- lights --- */
static void leds(void)
{
    uint32_t b = PAGE_BTN[ui.page] < NB ? 1u << PAGE_BTN[ui.page] : 0u, dim = 0u;
    if (sy_tel.drums_on && sy_tel.step % (sy_tel.steps / 4u ? sy_tel.steps / 4u : 4u) != 0u)
        b |= 1u << B_PLAY;                               /* lit, dark on each beat */
    if (sy_tel.rec == 2 || (sy_tel.rec == 1 && (plat_ms() / 125u) & 1u))
        b |= 1u << B_REC;                                /* recording: lit; counting in: blinking */
    if (ui.arp)
        b |= 1u << B_ARP;
    if (step_mode() && proj.par[P_LOOPBARS] > 0)        /* the steps: OCT- / OCT+ go through the bars */
        b |= 1u << B_OCTDN | 1u << B_OCTUP;
    else if (proj.par[P_OCTAVE] != 0)                    /* up or down an octave or more: that way lit */
        b |= 1u << (proj.par[P_OCTAVE] > 0 ? B_OCTUP : B_OCTDN);
    if (punch_mode() || ui.punched) {                    /* punching: the black pads dim, the whites beside a punch dim too */
        int k;
        for (k = 0; k < NKEYS; k++) {
            if (punch_mode() && PUNCH_KEY[k] >= 0)
                dim |= 1u << k;
            if (ui.punched >> k & 1u)
                dim |= (k > 0 ? 1u << (k - 1) : 0u) | (k < NKEYS - 1 ? 1u << (k + 1) : 0u);
        }
    }
    if (step_mode()) {   /* SEQ: the cursor's step lit (where a tap lands), the bar's steps with notes dim */
        int i, n;
        uint32_t b2 = 0u;
        const sy_ev_t *ev = sy_loop_events(&n);
        for (i = 0; i < n; i++)
            if (ev[i].t / 192 == ui.loop_bar)
                dim |= 1u << WHITE_K[ev[i].t / 12 % 16];
        for (i = 0; i < SY_PATS; i++) {                  /* the patterns: playing lit, waiting blinking, with notes dim */
            if (i == proj.pat && ui.pat_next < 0)
                b2 |= 1u << PAT_K[i];
            else if (i == ui.pat_next)
                b2 |= (plat_ms() / 150u) & 1u ? 1u << PAT_K[i] : 0u;
            else if (proj.pat_n[i])
                dim |= 1u << PAT_K[i];
        }
        for (i = 0; i < 3; i++)                          /* the mutes: lit while it plays */
            if (!(ui.mute >> i & 1u))
                b2 |= 1u << MUTE_K[i];
        if (ui.cur_step / 16 == ui.loop_bar)
            b2 |= 1u << WHITE_K[ui.cur_step % 16];
        plat_leds(b, ui.keys | b2, dim & ~b2);
        return;
    }
    plat_leds(b, ui.keys | ui.punched, dim & ~ui.punched);
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
    memset(&ck, 0, sizeof ck);
    sy_midi_out = plat_midi_out;                         /* (the band's notes and the clock, from the engine) */
    ui.touched = -1;
    ui.pk_fx = -1;
    ui.page = PG_BODY;
    ui.act_t = ui.focus_t = plat_ms();
    ui.panel = 0;
    ui.held_btn = -1;
    ui.last_up_btn = -1;
    ui.step_note = 60;
    ui.step_held = -1;
    ui.cur_note = 60;
    ui.pat_next = -1;
    px_init();
    blob_init();
}

static void button(int b);
static void button_up(int b);
void ui_frame(void)
{
    int c;
    input();
    clock_frame();
    if (ui.tap_n && plat_ms() - ui.tap_last > TAP_GAP_MS) {   /* one HOME tap on a tempo page, no more: HOME after all */
        const int one = ui.tap_n == 1;
        ui.tap_n = 0;
        if (one) {
            ui.tap_skip = 1;
            button(B_HOME);
            if (!(ui.btn >> B_HOME & 1u))
                button_up(B_HOME);
            ui.tap_skip = 0;
        }
    }
    {   /* a flood of MIDI filled the engine's queue and some values were lost: all of them again, once there is room */
        static uint32_t seen;
        if (sy_tel.dropped != seen && sy_queue_room() > P_NPARAMS + PU_N * PU_KN + 16) {
            int i;
            seen = sy_tel.dropped;
            for (i = 0; i < P_NPARAMS; i++)
                sy_set(i, proj.par[i]);
            project_send_knobs();
        }
    }
    if (ui.prev_on && (int32_t)(plat_ms() - ui.prev_until) >= 0) {   /* (SEQ's taste of a note: over) */
        sy_note_off(ui.preview);
        ui.prev_on = 0;
    }
    while ((c = plat_text()) != 0)                       /* typing: into the name, while SAVE AS is open */
        if (ui.naming)
            name_type(c);
    if (sy_tel.pattern < SY_PATS && sy_tel.pattern != proj.pat && sy_tel.pattern_next < 0) {   /* a new pattern came in */
        proj.pat = sy_tel.pattern;
        knob_set(P_LOOPBARS, proj.pat_bars[proj.pat] ? proj.pat_bars[proj.pat] - 1 : proj.par[P_LOOPBARS]);
        ui.loop_bar = 0;
        ui.cur_step = 0;
        mark_dirty();
    }
    if (sy_tel.pattern_next < 0)
        ui.pat_next = -1;
    if (sy_tel.loop_ver != ui.loop_ver) {                /* the loop changed: it wants saving */
        ui.loop_ver = sy_tel.loop_ver;
        if (plat_ms() > 1500u)
            mark_dirty();
    }
    autosave();
    if (ui.panel && ui.held_btn < 0 && !ui.latch && !ui.naming && (int32_t)(plat_ms() - ui.page_until) > 0)
        ui.panel = 0;                                    /* nothing held, nothing turned: the drawer hides, the page stays */
    if (plat_ms() - ui.last_draw >= 32u) {               /* ~30 pictures a second */
        ui.last_draw = plat_ms();
        blob_draw();
    }
    leds();
}

void ui_input_only(void) { input(); }
