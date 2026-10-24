# Changelog

A release is a push to `main` that changes `VERSION` to a version with no tag yet. CI then makes the
release build, tags the commit `vX.Y.Z` and publishes the GitHub Release with that version's notes,
taken from its section below. Add the section in the same pull request as the `VERSION` change.

## 0.1.0

The first MOPE-1. It forks FiMba-1 and replaces the kalimba with a different instrument. FiMba-1's history is below.

- **A wavetable synth:** 8 tables (Sigh, Moan, Pulse, Crush, Warble, Fold, Church, Sync) and 6 voices.
  Each voice has two detuned oscillators, a sub, analog drift and a resonant low-pass with its own
  envelope. There is one LFO. The effects are fry, chorus, a ping-pong delay, a plate reverb and tape.
- **A drum machine with styles:** 13 of them, each named after its genre, sadly: Grief drill, Empty city pop, Gloom gabber, Missed phonk call, Jersey cry club, Heartbreakcore, Amapia-no, Cold footwork, Dembow out, Vaporweep, Hyperflop, Funeral bossa, Office ambivalent. Each has a main
  bar, a fill every fourth bar, its own tempo and swing, and a synthesized kit. An optional auto-bass
  plays the style's bass line, always in a minor key: the relative minor of what you hold (C held: A
  minor), or the minor you hold.
- **No menus, no numbers: the blob.** The screen shows one sad pixel creature in a rainy alley, and it
  is the sound. Its outline is the wavetable. Its eyes are the filter (stoned when it is closed, laser
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
  blob. Drawers stay 3 s after the last touch; a part's button pressed twice keeps its page until
  HOME. **21 presets** in six kinds (pad, keys, pluck, lead, bass, FX).
- **A loop recorder**, the way other FM-1 firmwares do it: PLAY runs the band, REC records what you
  play into a 1, 2 or 4-bar loop (a bar's count-in when stopped, overdub while playing, notes on the
  1/16 grid), HOME + REC undoes, SAVE + REC clears. Holding SEQ shows the loop as a piano roll with
  the white keys as its 16 steps. The loop is saved with your sound.
- **Clearer cues:** chorus copies are see-through copies of the blob peeking from behind it; the
  voices are six lights over its head; the Eyes and Mouth pages zoom in on the eyes and the mouth;
  the cassette sits in the top-left corner.
- **Moods (presets):** Monday, 3AM, Left on read, Rain again, NPC, Deep fried, Church of nothing, Hold
  music.
- **Story:** every note is XP. Levels change nothing, though level 10 brings a sad balloon. Left alone, the blob
  falls asleep, and much later it stares at you.

## 0.6.1

- **Less work with Reverb at 0:** once the reverb's tail has died away, the reverb stops running until
  you turn it up again (on the FM-1, about 24% → 15% CPU when nothing is playing). Nothing changes in
  the sound.

## 0.6.0

- **Reset to the defaults:** hold SAVE for 1.5 s, then press SAVE again to confirm (anything else, or
  waiting 5 s, keeps everything). Every sound setting goes back to its default; the Setup page (Tune,
  MIDI, Theme) stays as it is.
- **MIDI out is off by default.** Turn it on on the Setup page (GLO, KNOB 3). Settings you've already
  saved keep what they had.
- **Wah needs a sound hole:** on the Board and None bodies, Wah and Wah rate are greyed out and don't
  turn ("NO HOLE: BOX OR GOURD").
- **The middle of the kalimba glows:** the key under the lowest tine (white key 9 in Tine, 8 in
  Mirror; in Keyboard, the key that plays middle C) is dimly lit, and lights fully while it rings.

## 0.5.2

- **Wah is how much wah.** With Wah rate on, the hand now flutters from open to the Wah setting and
  back, so Wah 100 is the full sweep and low settings a gentle one. In 0.5.1 it was the other way
  round: the flutter filled what Wah left open, so Wah 1 swept the whole way and Wah 100 didn't move.
- The fastest Wah rates (up to ~8 Hz) now sweep fully instead of being smoothed away.

## 0.5.1

- **The wah is now heard.** Covering the sound hole (ENV → Body: Wah, the mod wheel, pressure, the Hole
  key, or Wah rate) muffles the sound like a vowel closing: bright with the hole open, dark and a little
  quieter covered, with a vowel-like peak sweeping through the tines on the way. Before, it only moved a
  resonance below the lowest tine, and barely changed the sound. Box and Gourd have a hole; Board and
  None don't.
- The hand glides over the hole in about 20 ms, so the Hole key covers and uncovers it without a click.
- **Wah rate**'s lowest setting reads **Off** (it was "Hand"): the hole moves only when you move it.

## 0.5.0

- **Theme**, a new knob on the Setup page (GLO, KNOB 4): **Light**, the pale wood and dark ink as
  before, or **Dark**, near-black with cream and amber. In Dark the knob bars and the chips turn cream
  and brown, so amber only means a tine sounding or a knob touched. The tines keep their metal in both.
  The screen changes at once, and the theme is saved with your settings.
- The Setup page's header shows the firmware's version, beside the CPU load.
- Saved settings carry over; Theme starts on Light.

## 0.4.1

- **Tine is now left-first**, as on a 17-key kalimba in C. C4 sits on white key 9, the next note up
  (D4) is on its left, and the scale alternates outward: D6 on the far left, C6 on the far right
  ([#1](https://github.com/jadamsowers/fm1-fimba/issues/1)).
- **Mirror**, a new Layout value (Keys page, KNOB 1), keeps the old right-first arrangement: C4 on
  key 8, D4 on its right.
- Saved settings carry over. A project saved on Tine plays the new way; pick Mirror for the old one.

## 0.4.0

- **16 voices** (up from 12): fewer tines cut short when long-ringing steel tines and patterns overlap.
  Upper overtones that have died away are skipped, so ordinary playing costs less.
- **Worn**: an old, played-in kalimba. Each tine gets its own quirks: a little out of tune (up to ±12
  cents), a longer or shorter sustain, a faster or slower beat, uneven brightness and level.
- **Pages by button**: each page button owns its pages; press it again for the next (dots in the
  header show which). HOME: Tine → **Character** (Worn, Glide, Tuning, Release). ENV: Body.
  FX: Space → Color. LFO: Grain → Spread. SEL: Keys → Play. SEQ: Pattern. GLO: Setup.
- **ARP**: tap to start or stop the pattern; **hold** to open its settings.
- Settings saved by earlier versions carry over; new settings start at their defaults.

## 0.3.0

- The web page installs FiMba-1 on the FM-1 over USB, and offers the highest release.
- It runs on the FM-1: the first install on real hardware boots and plays.

## 0.2.0

- CI builds the firmware package and checks it against FoMni's; a version tag makes a release.

## 0.1.0

- The first release.
