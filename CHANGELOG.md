# Changelog

A release is a push to `main` that changes `VERSION` to a version with no tag yet. CI then makes the
release build, tags the commit `vX.Y.Z` and publishes the GitHub Release with that version's notes,
taken from its section below. Add the section in the same pull request as the `VERSION` change.

## 0.4.1

Punches stay clean with the whole band playing.

- **No more grinding under punches.** With the arpeggiator, the drums and the bass all going, HALF,
  DOUBLE, STOP and GRAIN could push the FM-1 past what it can do in time, and the sound broke up into
  something like a bit crusher. The sound engine now runs from the FM-1's fast memory instead of its
  flash, and the drums, the bass, the echoes and the tape do fewer sums per sample: a punch costs a
  few percent now instead of a tenth of the FM-1's time. The sound is the same.
- **STOP ends smoothly.** The last moment of a tape stop, when it is barely turning, no longer grinds.
- With the FM-1 very busy, the buffer punches switch to a lighter way of reading the sound until things
  calm down; you shouldn't hear the difference.

## 0.4.0

Punches, more effects, presets of your own, a band that keeps time with others, and a sequencer you
can perform with.

- **Punches.** Hold **SEL** (or press it twice to keep them) and the black keys punch the whole sound
  while held: half speed, double speed, a tape stop, reverse, repeats, a stutter, a gate, grains, a
  crush, a closing filter, a frozen wash and a warped record. With the band playing they land on the
  step. The two white keys beside a held punch light up: the left one makes it stronger, the right one
  twists it. Each punch has two knobs, shown as big named dials at the bottom of the screen, and the
  blob acts each one out (flat on the floor, up at the ceiling, greying to a stop, mirrored...).
- **FX goes round four effect pages:** walls (reverb, size, tone, rain), tears (the delay), copies (the
  chorus: rate, depth, width) and dust (tape, crackle, dust, fry).
- **A richer delay.** Its time follows the band's tempo (sixteenths, triplets, eighths, dotted eighths,
  the beat, dotted quarters). Every echo wears a little more, like tape: it saturates, more with more
  feedback, and the time wanders and flutters (Tape: much more). Dark past halfway smears the echoes
  into a cloud. Presets with a delay keep their sound but their echo now snaps to the tempo.
- **Your own presets.** Tap **SAVE**: SAVE AS opens with a sad name (DAMP MONDAY) in your first empty
  place of sixteen. Type over it in the web player, or spell it with the knobs (knob 1 the cursor,
  knob 2 the letter, knob 3 another name, knob 4 the place); SAVE again keeps it, any other button
  doesn't. Your presets come after the factory ones on PRESETS.
- **16 new presets, 54 in all:** aggressive basses (Rage quit, Passive aggressive, Overdraft, Landlord,
  Burnout), screaming leads (Scream into pillow, Noise complaint, All caps, Reply all), a new kind, VOX,
  sung by a vowel filter (Choir of one, Humming alone, Aah well, Says nothing), and more. All measured
  on the FM-1 to one loudness.
- **A vowel filter:** the Eyes page's sixth kind; Cutoff moves it from A to U.
- **Table 0 is PLAIN:** a sine, then a square in the middle, then a saw at the end.
- **The LFOs go further:** across the wavetables, to the detune, and to each effect.
- **One tempo for every beat,** on the mixer and the tape deck. Tap **HOME** in time on either to set
  it; the number shows big for a moment.
- **MIDI.** Settings → Clock: its own tempo, follow an incoming clock (and Start / Stop), or lead (it
  sends clock). Settings → MIDI out: the keys, plus the arpeggiator and the loop, plus the bass and the
  drums on channel 10. The sustain pedal works, and aftertouch does what Settings → Aftertouch says.
- **The sequencer.** Four patterns on the SEQ screen's first black keys, switching on the next bar
  while the band plays; mutes for the drums, the bass and the loop on the next three. The roll fits
  your notes, and the cursor edits single notes.
- **Settings** is a page of its own (GLO again): a list of rows on the whole screen.
- **The screen:** captions in their own band at the top, never on the blob; the cassette counts the
  beats on its label; the Sigh page draws the wave a note starts on and where its sweep takes it.
- **The web player** plays like a DAW from the computer's keys (A S D F... the white keys, W E R... the
  black ones, Z / X an octave), the letters drawn on the keys, the buttons on the number row. On the
  front page it is wider.
- **Faster where it counts:** the clocks no longer redo the tempo's maths every sample, and the gate
  punch no longer costs a tenth of the chip (it had been running the tape buffer it doesn't use). Heavy
  punches leave fewer voices free so the audio never runs late.
- **Fixes:** no hanging notes when the band's notes and yours overlap; the loop stays steady through
  tempo changes; the balloon at level 10 is gone (it crowded the screen).

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
