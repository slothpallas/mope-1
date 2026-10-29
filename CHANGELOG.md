# Changelog

A release is a push to `main` that changes `VERSION` to a version with no tag yet. CI then makes the
release build, tags the commit `vX.Y.Z` and publishes the GitHub Release with that version's notes,
taken from its section below. Add the section in the same pull request as the `VERSION` change.

## 0.3.0

More sound from the same blob: a second oscillator, more filters, a second envelope and a second LFO,
each on a second page; presets that are all different and all as loud; and a crash fixed.

- **Second pages.** Press a part's button again while its page shows (not twice fast: that keeps the
  drawer open). Until you've found one, the caption nudges you: AGAIN: SHADOW.
  - **Shadow** (HOME again): a second oscillator in every voice, with its own wavetable, up to two
    octaves away, mixed in, frequency modulating the first, ring modulating it or synced to it.
  - **Sigh** (ENV again): the mod envelope gets its own attack, and it can sweep the wavetable on every
    note, up or down, as well as open the eyes.
  - **Tremble** (LFO again): a second LFO, any of the 9 shapes, to the shape, the eyes, the pitch, the
    volume or the shadow.
  - Each page has its own drawing in the drawer.
- **The Eyes page** now turns the filter's kind (low-pass, band-pass, high-pass, notch, or a comb)
  and drive. Env amount and mod decay moved to Sigh.
- **Every preset is its own sound** and sets all of the new pages, so nothing carries over from the
  last one (that was the slow pitch wobble on every preset). Fewer of them wobble the pitch or the tape.
- **Presets at one level:** each is trimmed to the same loudness, measured on the FM-1 itself.
- **No crackle on chords:** a heavy sound (the shadow, drive, a band-pass) plays fewer voices, and if
  a moment still runs hot the oldest voice lets go quickly. Plain sounds cost what they did before.
- **Fixed a crash** when the LFO's destination was the shadow and its page was drawn.
- **The puddle:** the tears collect in a dark puddle on the floor in front of the blob, inside one soft
  shadow, a blurred mirror of the blob and the room that ripples when a tear lands. No more dotted
  floor reflection, and no row of voice lights over its head.
- The side walls are a touch more visible.
- Under the hood: random sessions are run under the sanitizers in CI, which found two places drawing
  just past the edge of their memory (both fixed); the FM-1 answers a USB query with its audio load
  and its last crash.

## 0.2.0

MOPE-1's own sound and its own player: the last parts that came from FiMba-1 are rewritten.

- **A new room** (the reverb): eight walls handing the sound to each other, four of them slowly
  breathing so the tail never rings. Size still runs from a dull one-second closet to the bright,
  half-minute void, as long and as loud as before at every setting, for the same processor time.
- **New tears** (the delay): each echo crosses from left to right and back, darker and thinner every
  time, softly clipped when the feedback is high, wobbling with Tape. Same times and levels as before.
- **MOPE-1's own maths** under the sound and the screen, held to the computer's own to a millionth.
- **A new player in the browser:** the FM-1 drawn in thin lines on black, so the screen is the
  only thing that glows; real buttons you can tab to; the right hints when you hover (the old page
  still described a kalimba). In another site's page it drops its title and installer, and it works
  on sites with a strict security policy.
- **Eight new wavetables** (16 in all): Glass (a few far-apart partials ringing), Breath (air rising
  round a sine), Cry (a peak climbing the harmonics), Hollow (a clarinet's softness to a square),
  Bend (a phase-bent sine), Toy (a four-bit triangle to a thin pulse), Ice (a sine modulating its
  octave) and Sag (a sine that can't hold itself up).
- **Four new LFO shapes** (9 in all), each with its own dance: heave (a slow rise and a drop: the blob
  fills up and lets it out), wander (smooth random: it drifts off and back), stagger (four steps: it
  lurches sideways) and throb (a heartbeat: two swells and a rest).
- **Seventeen new presets** (38 in all), in their kinds: Seen 2:14, Weighted blanket, The void stares,
  Slow heart; Music box, Glass office, Elevator; Typing..., Icicle; Cry on the floor, Bent out of
  shape, Drama; Hollow inside, Underslept; Heavy breathing, Broken record, Phantom vibrate.
- The reverb runs at half rate now, the same tail in half the memory: room for the new wavetables.
  They are built when it switches on, so it takes a little longer to wake.
- The help, the README and the licence notes say what is MOPE-1's and what it runs on.

## 0.1.0

The first MOPE-1: a new instrument and screen on the FM-1 platform it started from (FiMba-1, Felucca's family).

- **A wavetable synth:** 8 tables (Sigh, Moan, Pulse, Crush, Warble, Fold, Church, Sync) and 6 voices.
  Each voice has two detuned oscillators, a sub, analog drift and a resonant low-pass with its own
  envelope. There is one LFO. The effects are fry, chorus, a ping-pong delay, a plate reverb and tape.
- **A toy-keyboard backing band:** 13 depressing beats, each named after its genre, sadly: Grief drill, Empty city pop, Gloom gabber, Missed phonk call, Jersey cry club, Heartbreakcore, Amapia-no, Cold footwork, Dembow out, Vaporweep, Hyperflop, Funeral bossa, Office ambivalent. Each has a main
  bar, a fill every fourth bar, its own tempo and swing, and a synthesized kit. An optional auto-bass
  plays the style's bass line, always in a minor key: the relative minor of what you hold (C held: A
  minor), or the minor you hold.
- **The blob, the main character.** The screen shows one sad pixel creature in a rainy alley, and it
  shows the sound. Its outline is the wavetable. Its eyes are the filter (stoned when it is closed, laser
  eyes at full resonance) and its mouth is the envelope. Its tears are the delay, the alley's walls are
  the reverb, chorus adds more of it, and fry deep-fries it. The drums squash it, make it blink and
  rain on it. It floats in a small room in perspective whose walls move with the reverb size, and warm
  reds and oranges mark what is happening. One button per body part, four knobs per part: each part
  opens its schematic under the blob (wavetable stack, filter curve, ADSR, LFO, delay taps, reverb
  tail, drum grid) with the knobs' names over their meters.
- **Pixels, lit:** the blob is shaded per pixel as a glossy jelly: window light, floor bounce, a wet
  highlight, the lamp's orange rim and its reflection in the wet floor. The room is drawn in tones,
  the reverb's waves are ripples on the floor, and the text uses MOPE's own droopy 4 x 6 font.
- **Encoders as on other synths:** ALGORITHM picks the instrument's wave alone, PRESETS a whole saved
  sound (its card shows the wave, filter and envelope it brought), SELECT the drum tape.
- **Fry is level-matched** (more grit, not more loud) and glides, so presets don't slam it in.
- **The drawer is dark glass** over the bottom of the screen, so the blob stays big. The cassette is
  smoked see-through plastic with the tape winding between its spools.
- **A bigger face:** larger eyes and mouth, a thicker frown and brows; the filter shuts the eyes all
  the way, the envelope opens the mouth much wider, resonance shrinks the pupils to pinpoints.
- **The drum grid is round and cold:** circular steps in the room's blues and violet, no warm colours.
- **An arpeggiator on ARP:** up, down, up and down, wander, stab; 1/4 to 1/32 with triplets; 1-3
  octaves; gate. Synced to the tempo and to the drums' bar. Its page draws the pattern as a roll.
- **A mixer on GLO** (Setup on a second press): voice, drums, bass and **gunk**, the room's texture
  (rain, crackle, rumble) that swells when idle. The auto-bass follows the Bass fader.
- **Swing and Human** on the tape deck; the grid shows both. The drawer is all cold colours.
- **Lit wrong, on purpose:** a low light from below that swings round the blob with each note's pitch
  (instead of rings on the floor), brightens with the envelope and flinches on kicks; a soft sheen in
  place of the sharp highlight; the reverb as cold light from the walls. The blob sags, has drifting
  lumps and leans, so it never looks like a ball; fry burns it in patches.
- **More vibe:** the LFO moves its whole body, two and a half times as far, leaning, stretching and
  squashing with the shape: fully while notes sound, as a preview while you set it, barely when idle. At rest its mouth is meh, not a frown. No party hat.
- **Pages while held:** a part's button shows its drawer and gives it the knobs only while you hold
  it; let go and the knobs are the body's again. ARP: a tap toggles it, holding shows its page. GLO:
  hold for the mixer, tap-then-hold for the setup. In the browser emulator (one mouse) the page
  buttons latch instead: click to open, click again to close. A slimmer chin.
- **The walls:** text drips down them in tiny broken pieces (texture, barely visible); the left wall
  is cold, the right faintly warm; the back wall and the window fade at their edges instead of
  framing the room.
- **Seen from above:** the camera looks down into the room, as from a table; a darker room lifts the
  blob. Drawers hide 3 s after the last touch, but the page stays (its button lit, the knobs still
  its own) until another button or HOME; pressed twice, a drawer stays open. OCT+ / OCT− light while
  the keys are shifted. **21 presets** in six kinds (pad, keys, pluck, lead, bass, FX).
- **A loop recorder**, the way other FM-1 firmwares do it: PLAY runs the band, REC records what you
  play into a 1, 2 or 4-bar loop (a bar's count-in when stopped, overdub while playing, notes on the
  1/16 grid), HOME + REC undoes, SAVE + REC clears. SEQ shows the loop as a piano roll, and while it
  is open the white keys are its 16 steps: press one to choose it (or add a note there) and turn
  NOTE, LEN and VEL to edit it; tap it to remove it. With no step chosen the knobs edit the whole
  loop. The loop is saved with your sound.
- **Clearer cues:** chorus copies are see-through copies of the blob peeking from behind it; the
  voices are six lights over its head; the Eyes and Mouth pages zoom in on the eyes and the mouth;
  the cassette sits in the top-left corner.
- **Moods (presets):** Monday, 3AM, Left on read, Rain again, NPC, Deep fried, Church of nothing, Hold
  music.
- **Story:** every note is XP. Levels change nothing, though level 10 brings a sad balloon. Left alone, the blob
  falls asleep, and much later it stares at you.
