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
 *   SEQ   LOOP       the stepped step's Note  Length  Velocity, and the loop's Bars
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
       PG_SHADOW, PG_SIGH, PG_TREMBLE, NPAGES };
static const char *const PAGE_NAME[NPAGES] = {"BODY", "EYES", "MOUTH", "VIBE", "TEARS", "WALLS", "TAPE DECK", "SETUP",
                                              "MIXER", "ARP", "LOOP", "SHADOW", "SIGH", "TREMBLE"};
static const uint8_t PAGE_BTN[NPAGES] = {B_HOME, B_EDIT, B_ENV, B_LFO, B_FX, B_SEL, B_SEQ, B_GLO, B_GLO, B_ARP, B_SEQ,
                                         B_HOME, B_ENV, B_LFO};
/* a button's second page (pressed again, its first page open), and back: -1 none */
static const int8_t PAGE_AGAIN[NPAGES] = {PG_SHADOW, -1, PG_SIGH, PG_TREMBLE, -1, -1, PG_LOOP, PG_MIX, PG_SETUP, -1, PG_DECK,
                                          PG_BODY, PG_MOUTH, PG_VIBE};
/* which of its button's pages it is (0 first, 1 second; the dots in the drawer), and its bit in proj.seen */
static const uint8_t PAGE_SECOND[NPAGES] = {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1};
static const uint8_t PAGE_SEEN_BIT[NPAGES] = {0, 0, 0, 0, 0, 0, 16, 8, 0, 0, 0, 1, 2, 4};
/* the knobs' names, under their meters in the schematic */
static const char *const PAGE_LABEL[NPAGES][4] = {
    {"WAVE", "POS", "DTUN", "SUB"}, {"CUT", "RES", "TYPE", "DRIV"}, {"ATK", "DEC", "SUS", "REL"},
    {"RATE", "DPTH", "SHPE", "DEST"}, {"CHOR", "DLY", "TIME", "FDBK"}, {"VERB", "SIZE", "FRY", "TAPE"},
    {"STYL", "BPM", "SWNG", "HUMN"}, {"GLID", "CH", "OUT", "TUNE"}, {"VOX", "DRUM", "BASS", "GUNK"},
    {"MODE", "RATE", "OCT", "GATE"}, {"NOTE", "LEN", "VEL", "BARS"},
    {"WAVE", "TUNE", "MEET", "LVL"}, {"ATK", "DEC", "WAVE", "EYES"}, {"RATE", "DPTH", "SHPE", "DEST"}};
#define STAY_MS 3000u                  /* a drawer stays this long after the last touch (a press, a turn) */
#define DOUBLE_MS 350u                 /* a part's button pressed twice this fast: its page stays until HOME */
#define TAP_MS 300u                    /* ARP let go sooner: a tap (on / off), not a look at its page */
#define K_NONE 255
static const uint8_t PAGE_KNOB[NPAGES][4] = {
    {P_TABLE, P_POS, P_DETUNE, P_SUB},
    {P_CUTOFF, P_RESO, P_FTYPE, P_DRIVE},
    {P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE},
    {P_LFORATE, P_LFODEPTH, P_LFOSHAPE, P_LFODEST},
    {P_CHORUS, P_DELAY, P_TIME, P_FEEDBACK},
    {P_REVERB, P_SIZE, P_FRY, P_TAPE},
    {P_STYLE, P_TEMPO, P_SWING, P_HUMAN},
    {P_GLIDE, P_MIDICH, P_MIDIOUT, P_TUNE},
    {P_SYNTH, P_DRUMS, P_BASS, P_GUNK},
    {P_ARPMODE, P_ARPRATE, P_ARPOCT, P_ARPGATE},
    {K_NONE, K_NONE, K_NONE, P_LOOPBARS},            /* the loop: the stepped step's note, length, velocity */
    {P_BTABLE, P_BTUNE, P_BMEET, P_BLEVEL},          /* the shadow: oscillator B */
    {P_MATTACK, P_MODDEC, P_MWAVE, P_ENVAMT},        /* the sigh: the mod envelope, to the table and the eyes */
    {P_LFO2RATE, P_LFO2DEPTH, P_LFO2SHAPE, P_LFO2DEST},   /* the tremble: the second LFO */
};
#define SAVE_HOLD_MS 1500u
#define RESET_ASK_MS 5000u
#define KEY0_NOTE 53                   /* the lowest key, F3 */

/* moods: the presets, Table .. Tape (P_TABLE .. P_TAPE) */
/* The presets: whole sounds (Table .. Tape, P_TABLE .. P_TAPE), plus an octave and a glide, in six
 * kinds. Columns: table pos detune sub | cutoff reso env mdecay | A D S R | lfo rate depth shape dest |
 * chorus delay time feedback | reverb size fry tape; then B table, B tune, B meets, B level | filter, drive |
 * mod attack, env wave | lfo2 rate depth shape dest | level (each preset's loudness, evened out) */
enum { CAT_PAD, CAT_KEYS, CAT_PLUCK, CAT_LEAD, CAT_BASS, CAT_FX, NCAT };
static const char *const CAT_NAME[NCAT] = {"PAD", "KEYS", "PLUCK", "LEAD", "BASS", "FX"};
#define MOOD_N (P_TAPE + 1)
#define MOOD_X (P_LEVEL - P_BTABLE + 1)
typedef struct {
    const char *name;
    uint8_t cat;
    int8_t oct, glide;
    int8_t v[MOOD_N];
    int8_t x[MOOD_X];                    /* the second pages and the level: B table .. LFO2 dest, Level */
} mood_t;
static const mood_t MOODS[] = {
    {"MONDAY", CAT_PAD, 0, 0, {0, 20, 25, 30, 58, 20, 35, 45, 10, 50, 60, 50, 30, 12, 0, 0, 30, 22, 50, 45, 38, 62, 0, 20},
     {9, 12, 0, 20, 0, 0, 0, 0, 55, 0, 1, 2, -10}},
    {"3AM", CAT_PAD, 0, 0, {1, 40, 15, 30, 52, 35, 50, 55, 35, 60, 75, 65, 15, 30, 0, 0, 45, 35, 65, 55, 60, 85, 0, 25},
     {0, 7, 0, 25, 0, 0, 60, 30, 20, 15, 0, 3, 14}},
    {"RAIN AGAIN", CAT_PAD, 0, 0, {4, 60, 35, 25, 50, 25, 40, 50, 20, 55, 50, 70, 20, 25, 1, 1, 50, 45, 55, 65, 55, 80, 0, 30},
     {0, -12, 0, 0, 0, 0, 0, -40, 70, 20, 4, 1, -7}},
    {"CHURCH OF NOTHING", CAT_PAD, 0, 0, {6, 80, 40, 10, 60, 15, 20, 60, 60, 70, 90, 85, 10, 15, 0, 0, 60, 30, 70, 50, 85, 98, 0, 10},
     {6, -12, 0, 35, 0, 0, 0, 0, 10, 15, 0, 0, -8}},
    {"SUNDAY SCARIES", CAT_PAD, 0, 0, {1, 70, 45, 25, 40, 20, 30, 70, 75, 80, 80, 90, 8, 40, 1, 0, 70, 30, 75, 55, 75, 90, 0, 35},
     {10, 1, 0, 15, 0, 0, 0, 0, 15, 20, 6, 4, 15}},
    {"SEEN 2:14", CAT_PAD, 0, 0, {9, 45, 30, 10, 42, 15, 25, 55, 45, 60, 75, 75, 12, 30, 6, 0, 55, 30, 60, 50, 55, 85, 0, 15},
     {0, -12, 0, 0, 3, 0, 70, 50, 55, 0, 1, 2, 5}},
    {"WEIGHTED BLANKET", CAT_PAD, 0, 0, {15, 50, 20, 15, 36, 10, 20, 60, 50, 70, 75, 80, 8, 25, 5, 1, 40, 20, 55, 40, 50, 78, 0, 40},
     {1, -12, 0, 30, 0, 25, 0, 0, 55, 0, 1, 2, -16}},
    {"THE VOID STARES", CAT_PAD, 0, 0, {14, 30, 25, 0, 42, 25, 30, 70, 65, 80, 72, 95, 10, 35, 6, 0, 60, 35, 80, 60, 45, 100, 0, 0},
     {14, -24, 2, 30, 0, 0, 0, 0, 8, 30, 0, 4, -10}},
    {"SLOW HEART", CAT_PAD, 0, 0, {11, 40, 15, 15, 42, 10, 15, 50, 30, 60, 80, 70, 15, 55, 8, 3, 40, 20, 50, 40, 50, 75, 0, 10},
     {0, -12, 0, 0, 0, 0, 0, 0, 55, 0, 1, 2, -16}},
    {"LEFT ON READ", CAT_KEYS, 0, 0, {6, 30, 10, 0, 70, 10, 10, 30, 5, 40, 80, 40, 40, 12, 0, 3, 20, 40, 40, 60, 45, 70, 0, 10},
     {8, 12, 0, 15, 0, 15, 0, -30, 55, 0, 1, 2, -14}},
    {"HOLD MUSIC", CAT_KEYS, 0, 0, {0, 10, 20, 10, 55, 10, 20, 40, 15, 50, 70, 55, 55, 12, 0, 2, 70, 20, 45, 30, 30, 40, 0, 70},
     {0, -12, 0, 0, 1, 0, 0, 0, 55, 0, 1, 2, 3}},
    {"OPEN PLAN", CAT_KEYS, 0, 0, {0, 30, 20, 25, 58, 15, 45, 35, 1, 55, 30, 40, 45, 15, 0, 0, 40, 20, 40, 30, 30, 45, 5, 15},
     {0, 12, 1, 25, 0, 0, 0, 0, 55, 0, 1, 2, -1}},
    {"MUSIC BOX", CAT_KEYS, 0, 0, {13, 20, 5, 0, 65, 15, 25, 30, 1, 45, 40, 50, 30, 0, 0, 2, 20, 35, 45, 40, 45, 65, 0, 20},
     {13, 24, 0, 30, 0, 0, 0, 0, 55, 0, 1, 2, 21}},
    {"GLASS OFFICE", CAT_KEYS, 0, 0, {8, 35, 12, 10, 68, 15, 30, 35, 1, 55, 45, 55, 40, 10, 1, 1, 45, 25, 40, 35, 40, 55, 0, 5},
     {8, 19, 1, 20, 0, 0, 0, 0, 55, 0, 1, 2, 16}},
    {"ELEVATOR", CAT_KEYS, 0, 0, {14, 25, 10, 15, 64, 10, 20, 30, 1, 50, 55, 45, 50, 20, 0, 3, 50, 20, 35, 30, 35, 50, 0, 30},
     {11, 12, 0, 20, 0, 0, 0, 0, 55, 0, 1, 2, -7}},
    {"NPC", CAT_PLUCK, 0, 0, {2, 55, 0, 45, 72, 30, 45, 25, 2, 40, 62, 20, 60, 10, 2, 0, 0, 15, 30, 30, 15, 30, 0, 10},
     {2, 7, 3, 40, 0, 0, 0, 0, 55, 0, 1, 2, 10}},
    {"READ RECEIPT", CAT_PLUCK, 0, 0, {3, 50, 10, 30, 62, 35, 70, 25, 1, 35, 10, 30, 0, 0, 0, 0, 20, 40, 45, 50, 30, 55, 0, 10},
     {0, -12, 0, 0, 4, 0, 0, 0, 55, 0, 1, 2, 0}},
    {"BUBBLE WRAP", CAT_PLUCK, 1, 0, {1, 70, 5, 30, 72, 40, 80, 30, 1, 42, 10, 25, 70, 20, 4, 0, 10, 30, 25, 35, 25, 40, 0, 0},
     {0, -12, 0, 0, 0, 20, 0, 60, 55, 0, 1, 2, 50}},
    {"TYPING...", CAT_PLUCK, 0, 0, {13, 70, 5, 25, 74, 25, 60, 24, 0, 34, 5, 25, 0, 0, 0, 0, 10, 40, 30, 45, 25, 40, 0, 20},
     {3, 12, 2, 25, 0, 0, 0, 0, 55, 0, 1, 2, 50}},
    {"ICICLE", CAT_PLUCK, 0, 0, {14, 60, 8, 30, 78, 30, 62, 25, 0, 38, 8, 30, 0, 0, 0, 0, 15, 45, 50, 50, 40, 65, 0, 5},
     {14, 24, 0, 30, 0, 0, 0, 0, 55, 0, 1, 2, 36}},
    {"MAIN CHARACTER", CAT_LEAD, 0, 25, {7, 55, 25, 20, 70, 35, 40, 50, 3, 50, 75, 35, 45, 8, 0, 2, 25, 35, 45, 40, 30, 50, 10, 10},
     {7, -12, 0, 35, 0, 30, 0, 0, 55, 0, 1, 2, 0}},
    {"LOW BATTERY", CAT_LEAD, 0, 10, {4, 60, 15, 10, 60, 20, 20, 40, 5, 40, 80, 40, 55, 20, 0, 2, 20, 30, 55, 45, 35, 60, 0, 70},
     {0, -12, 0, 0, 0, 0, 0, 0, 25, 20, 7, 3, -32}},
    {"DEEP FRIED", CAT_LEAD, 0, 0, {5, 70, 30, 50, 80, 55, 40, 40, 2, 45, 70, 30, 70, 40, 3, 1, 20, 10, 20, 20, 20, 40, 62, 20},
     {5, 7, 1, 35, 0, 70, 0, 0, 55, 0, 1, 2, 18}},
    {"CRY ON THE FLOOR", CAT_LEAD, 0, 0, {10, 50, 30, 15, 66, 45, 40, 45, 3, 50, 80, 40, 45, 40, 5, 0, 30, 35, 45, 45, 35, 55, 15, 15},
     {10, 12, 0, 25, 0, 0, 40, 40, 60, 6, 0, 2, 16}},
    {"BENT OUT OF SHAPE", CAT_LEAD, 0, 0, {12, 40, 20, 15, 70, 30, 45, 40, 2, 45, 75, 35, 50, 35, 7, 0, 25, 30, 40, 35, 30, 50, 20, 30},
     {12, 5, 3, 50, 0, 20, 0, 0, 55, 0, 1, 2, -20}},
    {"DRAMA", CAT_LEAD, 0, 30, {15, 70, 25, 10, 72, 40, 35, 45, 4, 50, 80, 40, 55, 20, 1, 1, 30, 40, 50, 50, 40, 60, 30, 10},
     {15, -12, 0, 40, 0, 25, 0, 0, 55, 0, 1, 2, -29}},
    {"SUB-PAR", CAT_BASS, -1, 0, {0, 0, 5, 90, 35, 10, 20, 30, 2, 40, 85, 15, 20, 0, 0, 0, 0, 0, 40, 0, 8, 30, 0, 5},
     {0, -12, 0, 0, 0, 10, 0, 0, 55, 0, 1, 2, -19}},
    {"BOTTOM TEXT", CAT_BASS, -1, 0, {2, 50, 60, 50, 40, 25, 30, 35, 3, 50, 80, 20, 15, 30, 0, 0, 10, 0, 40, 0, 10, 30, 15, 20},
     {2, 12, 3, 30, 0, 30, 0, 0, 55, 0, 1, 2, -16}},
    {"DOOMSCROLL", CAT_BASS, -1, 35, {7, 30, 10, 50, 33, 75, 75, 35, 1, 40, 55, 15, 0, 0, 0, 0, 0, 20, 30, 30, 10, 30, 25, 10},
     {0, -12, 0, 0, 0, 45, 0, -50, 55, 0, 1, 2, 0}},
    {"BASEMENT", CAT_BASS, -1, 0, {5, 40, 15, 60, 45, 30, 40, 40, 2, 45, 70, 20, 10, 20, 1, 1, 0, 0, 40, 0, 12, 25, 40, 25},
     {3, -12, 0, 20, 0, 40, 0, 0, 55, 0, 1, 2, -7}},
    {"HOLLOW INSIDE", CAT_BASS, -1, 0, {11, 70, 5, 50, 40, 25, 45, 30, 1, 45, 70, 20, 0, 0, 0, 0, 0, 0, 30, 0, 10, 30, 10, 10},
     {11, 12, 2, 20, 3, 0, 0, 0, 55, 0, 1, 2, -6}},
    {"UNDERSLEPT", CAT_BASS, -1, 0, {15, 60, 15, 60, 38, 35, 40, 35, 1, 45, 80, 20, 20, 30, 6, 1, 0, 0, 30, 0, 10, 25, 25, 30},
     {15, 7, 1, 30, 0, 0, 30, 30, 55, 0, 1, 2, -37}},
    {"TINNITUS", CAT_FX, 1, 0, {7, 100, 70, 0, 90, 70, 0, 50, 70, 80, 100, 90, 30, 40, 4, 1, 50, 60, 60, 70, 80, 95, 20, 0},
     {8, 24, 2, 30, 1, 0, 0, 0, 5, 10, 6, 2, 28}},
    {"DIAL TONE", CAT_FX, 0, 0, {3, 90, 5, 30, 75, 40, 10, 30, 1, 50, 100, 40, 3, 50, 3, 1, 0, 50, 70, 60, 40, 60, 35, 40},
     {0, 4, 0, 50, 0, 0, 0, 0, 55, 0, 1, 2, -47}},
    {"GHOSTED", CAT_FX, 0, 0, {1, 20, 80, 20, 45, 30, 20, 60, 78, 90, 80, 100, 5, 70, 0, 0, 100, 70, 85, 75, 95, 100, 0, 30},
     {0, -12, 0, 0, 4, 0, 0, 0, 10, 25, 6, 1, 24}},
    {"HEAVY BREATHING", CAT_FX, 0, 0, {9, 100, 40, 0, 70, 40, 20, 60, 50, 70, 90, 85, 8, 70, 5, 3, 50, 40, 60, 55, 70, 85, 0, 20},
     {0, -12, 0, 0, 1, 0, 80, 40, 55, 0, 1, 2, 50}},
    {"BROKEN RECORD", CAT_FX, 0, 0, {13, 80, 5, 0, 70, 30, 30, 30, 1, 50, 70, 40, 55, 40, 7, 2, 20, 50, 30, 60, 40, 60, 40, 80},
     {0, -12, 0, 0, 0, 0, 0, 0, 40, 50, 2, 3, 0}},
    {"PHANTOM VIBRATE", CAT_FX, 0, 0, {14, 90, 20, 0, 60, 50, 20, 40, 5, 50, 80, 50, 60, 60, 8, 3, 30, 30, 40, 40, 45, 70, 10, 0},
     {14, -12, 2, 50, 0, 0, 0, 0, 85, 30, 2, 1, 20}},
};
#define NMOOD ((int)(sizeof MOODS / sizeof MOODS[0]))
/* what a level-up feels like */
static const char *const LEVEL_UP[] = {"OK.", "COOL. I GUESS.", "LEVEL UP. NOTHING CHANGED.", "+1 SADNESS",
                                       "ACHIEVEMENT: STILL HERE", "GG", "IT LISTENED.", "NEW SKILL: NONE"};
static const char *const LFO_DEST_NAME[LD_N] = {"TO: SHAPE", "TO: EYES", "TO: VOICE", "TO: VOLUME", "TO: SHADOW"};
static const char *const MEET_NAME[MEET_N] = {"MIX: BESIDE IT", "FM: INSIDE IT", "RING: AROUND IT", "SYNC: TIED TO IT"};
static const char *const FTYPE_NAME[FT_N] = {"LOW-PASS", "BAND-PASS: A SQUINT", "HIGH-PASS: WIDE AWAKE", "NOTCH: A WINK",
                                             "COMB: THE BLINDS"};
static const char *const LFO_SHAPE_NAME[LFO_NSHAPE] = {"SWAY", "BOB", "BOUNCE", "NOD", "TWITCH", "HEAVE", "WANDER", "STAGGER",
                                                          "THROB"};
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
    uint32_t held_t, page_until;
    uint8_t latch;                     /* a page pressed twice: it stays until HOME */
    int8_t last_up_btn;                /* the page button last let go, and when (a double press) */
    uint32_t last_up_t;
    uint32_t preset_t;                 /* when PRESETS last turned: its card shows a moment (0: never) */
    uint8_t key_note[NKEYS];           /* the note each key started (its note off) */
    uint32_t stepped;                  /* keys pressed as the loop's steps (their preview to stop) */
    uint8_t loop_bar, step_note;       /* the bar the steps show; the note a new step gets */
    int16_t step_held;                 /* a step key down on a step with notes: let go untouched, they go */
    uint8_t preview;                   /* the note a step key is sounding */
    uint8_t step_turned;               /* ...a knob turned meanwhile: they stay (edited) */
    uint32_t loop_ver;
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

/* the loop's steps: while its drawer is open (not while recording: then the keys play) */
static int step_mode(void) { return ui.page == PG_LOOP && ui.panel && !sy_tel.rec; }

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
        knob_set(i, MOODS[m].v[i]);
    for (i = 0; i < MOOD_X; i++)
        knob_set(P_BTABLE + i, MOODS[m].x[i]);
    knob_set(P_OCTAVE, MOODS[m].oct);                    /* (basses sit an octave down, some leads glide) */
    knob_set(P_GLIDE, MOODS[m].glide);
    ui.preset_t = plat_ms() | 1u;                        /* its card: the name, its kind, and what it brought */
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

static int white_of(int k)
{
    static const uint8_t W[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    int i;
    for (i = 0; i < 16; i++)
        if (W[i] == k)
            return i;
    return -1;
}

static void key_down(int k)
{
    int n = KEY0_NOTE + k + 12 * proj.par[P_OCTAVE];
    n = n < 0 ? 0 : n > 127 ? 127 : n;
    if (step_mode()) {                                   /* a white key: that 1/16 step of the bar on show */
        const int w = white_of(k);
        if (w >= 0) {
            const int st = ui.loop_bar * 16 + w;
            int i, cnt, has = 0, pitch = ui.step_note;
            const sy_ev_t *ev = sy_loop_events(&cnt);
            for (i = 0; i < cnt; i++)
                if (ev[i].t / 12 == (uint16_t)st) {
                    has = 1;
                    pitch = ev[i].note;
                }
            if (has) {                                   /* notes there: chosen (the knobs edit them) */
                sy_loop_select(st);
                ui.step_held = (int16_t)st;
                ui.step_turned = 0;
            } else {                                     /* none: one comes, chosen */
                sy_loop_step(st, ui.step_note);
                ui.step_held = -1;
            }
            ui.focus_t = plat_ms();
            ui.page_until = plat_ms() + STAY_MS;
            sy_note_on(pitch, 90);                       /* (you hear the step) */
            ui.preview = (uint8_t)pitch;
            ui.stepped |= 1u << k;
        }
        return;
    }
    ui.key_note[k] = (uint8_t)n;
    ui.step_note = (uint8_t)n;                           /* (a new step gets the last note you played) */
    play(n, 100);
}
static void key_up(int k)
{
    if (ui.stepped >> k & 1u) {
        ui.stepped &= ~(1u << k);
        sy_note_off(ui.preview);
        if (ui.step_held >= 0 && !ui.step_turned) {      /* tapped, not edited: its notes go */
            sy_loop_step(ui.step_held, ui.step_note);
            sy_loop_select(-1);
        }
        ui.step_held = -1;
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
                if (PAGE_BTN[i] == b && !PAGE_SECOND[i])
                    pg = i;
        if (pg == PG_LOOP && !(ui.panel && ui.page == PG_LOOP))
            sy_loop_select(-1);                          /* (a fresh look: the knobs edit the whole loop) */
        if (pg >= 0) {                                   /* a part's button: its page while held, and 3 s on */
            const int again = PAGE_AGAIN[pg];
            set_page(pg);
            if (PAGE_SECOND[pg] && !(proj.seen & PAGE_SEEN_BIT[pg])) {
                proj.seen |= PAGE_SEEN_BIT[pg];          /* found: no more nudging */
                mark_dirty();
            } else if (!dbl && again >= 0 && PAGE_SECOND[again] && !(proj.seen & PAGE_SEEN_BIT[again])) {
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
            if (dbl && !ui.latch) {
                ui.latch = 1;                            /* twice: it stays (other pages too) until HOME */
                ui_caption("STAYS OPEN.", 1200u);
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
            ui.focus_t = plat_ms();
            break;
        }
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
    case 102: cc_param = (int)v < P_NPARAMS ? (int)v : P_NPARAMS - 1; break;
    case 103: {
        const sy_param_t *pi = sy_param_info(cc_param);
        knob_set(cc_param, pi->lo + (int)(((int32_t)v * (pi->hi - pi->lo) + 63) / 127));
        break;
    }
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
        knob_set(P_TEMPO, sy_style_tempo(proj.par[P_STYLE]));
        say_choice(P_STYLE);
        if (!ui.latch) {
            set_page(PG_DECK);
            ui.touched = 0;
        }
        ui.panel = 1;
        ui.page_until = plat_ms() + STAY_MS;
        ui.act_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {                /* the preset: its mood */
        load_mood((ui.mood + (e > 0 ? 1 : NMOOD - 1)) % NMOOD);
        ui.act_t = plat_ms();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            if (ui.page == PG_LOOP && i < 3) {           /* the chosen step (or the whole loop): note, length, velocity */
                sy_loop_nudge((int)i, e > 0 ? 1 : -1);
                ui.step_turned = 1;
            }
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
    uint32_t b = 1u << PAGE_BTN[ui.page];
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
    plat_leds(b, ui.keys, 0u);
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
    ui.last_up_btn = -1;
    ui.step_note = 60;
    ui.step_held = -1;
    px_init();
    blob_init();
}

void ui_frame(void)
{
    input();
    if (sy_tel.loop_ver != ui.loop_ver) {                /* the loop changed: it wants saving */
        ui.loop_ver = sy_tel.loop_ver;
        if (plat_ms() > 1500u)
            mark_dirty();
    }
    autosave();
    if (ui.panel && ui.held_btn < 0 && !ui.latch && (int32_t)(plat_ms() - ui.page_until) > 0)
        ui.panel = 0;                                    /* nothing held, nothing turned: the drawer hides, the page stays */
    if (plat_ms() - ui.last_draw >= 32u) {               /* ~30 pictures a second */
        ui.last_draw = plat_ms();
        blob_draw();
    }
    leds();
}

void ui_input_only(void) { input(); }
