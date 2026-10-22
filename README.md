# MOPE-1

A sad wavetable synth and drum machine for the M-VAVE FM-1. It plays anyway.

There are no menus and no numbers. The screen shows one blob, a lit, glossy jelly in chunky pixels,
floating in a small sad room with a rainy window behind it, and the blob *is* the sound. Everything you turn happens to it. Under it, the
part you are working on opens as a schematic: the envelope, the LFO, the filter curve, drawn the
classic way, live.

![the blob, through its parts](docs/img/sheet.png)

MOPE-1 is a fork of [FiMba-1](https://github.com/jadamsowers/fm1-fimba). The boot code, safe mode,
update loader, installer, plate reverb and browser emulator are FiMba-1's, inherited from FoMni, X0X
and Felucca. The instrument and the screen are new.

> **Status: untested on hardware.** Everything below runs in the host simulator and the browser
> emulator, built from the same C code. The device build has not been compiled or flashed yet; see
> [Before it goes on an FM-1](#before-it-goes-on-an-fm-1).

## The blob is the synth

| You turn | What happens to the blob |
| --- | --- |
| **Table, Position** | Its outline is the wavetable wrapped round it, and it wobbles with the live output. Morphing the position reshapes its body. |
| **Detune** | A ghost of itself drifts apart from it, beating. |
| **Sub** | It gets heavier, and its bottom spreads. |
| **Cutoff** | Its big eyes: wide open, then half-lidded, then shut (stoned), all the way. |
| **Resonance** | Its pupils shrink to pinpoints and start to glow, and at the top they become laser eyes. |
| **Env amount / Mod decay** | Its eyebrows rise with every note. |
| **Attack / Decay / Sustain / Release** | Its big mouth opens wide with the note, stays open at the sustain and closes with the release. A long release is a yawn. |
| **LFO** | Its whole body dances at the rate, as far as the depth: a sway (side to side, leaning into it), a bob (stretching as it rises), a bounce (squashed when it lands), a nod, or a twitch; its lumps wobble along. It dances big while notes sound (riding their envelope), shows you a preview while you turn the Vibe knobs, and only murmurs when idle. |
| **Chorus** | See-through copies of itself peek out from behind it, a beat out of step: one a side, then more, further out and fainter. |
| **Delay** | It cries one tear per echo. Each tear is smaller than the one before, and they fall as far apart as the echoes. They pool on the ground. |
| **Reverb, Size** | The walls throw cold light back onto its edges as the reverb rings. A bigger room pushes the walls out and the blob gets smaller in it. Past the walls is the void, with stars. |
| **Fry** | It gets deep-fried in burnt patches: brown, crusty, sweating, then steaming. |
| **Volume** | Its dirty orange aura. |
| **Voices** | Six lights over its head, one per voice: bright while its note is held, glowing while it rings out. A stolen voice's light gets yeeted. |
| **Clipping** | It catches fire and looks calm (this is fine). |
| **The drums** | A smoky see-through cassette in the top-left corner, the tape winding between its spools, the style on its label, its reels turning at the tempo. Kicks squash the blob and make the light flinch, snares make it blink, hats are rain on the window. Gabber makes it shake. |
| **Nothing** | Its mouth rests at meh. It falls asleep; much later it wakes up and stares at you. |

| | | | |
| --- | --- | --- | --- |
| ![](docs/img/02b_env_schematic.png) envelope | ![](docs/img/02c_lfo_schematic.png) LFO | ![](docs/img/04_lasers.png) resonance | ![](docs/img/05_tears_chorus.png) delay + chorus |
| ![](docs/img/04b_preset_card.png) a preset's card | ![](docs/img/08_deep_fried.png) fry | ![](docs/img/09_drums.png) drums | ![](docs/img/12b_blob_alone.png) the blob alone |
| ![](docs/img/10b_mixer.png) mixer | ![](docs/img/10c_arp.png) arpeggiator | ![](docs/img/07_void.png) the void | ![](docs/img/13_asleep.png) asleep |

## The look

- **Pixels, lit wrong:** 120 x 120 pixels shown at 2x. The blob and its copies are shaded per pixel through a 4 x 4 dither, by a light that comes from below like a torch under a chin. Each note swings that light round the blob by its pitch, the envelope brightens it, and a kick makes it flinch. There is a soft dull sheen (none in close-ups), a one-pixel dark edge, and the streetlamp's orange rim that grows with the volume. The reverb shows as cold light thrown back by the walls onto its edges. It reflects in the wet floor.
- **Never a ball:** it sags at the bottom like a slime, has lumps that slowly drift, and leans a little; the wavetable shapes the rest.
- **The room is tones, not lines:** a back wall lit in the middle and fading at its edges (no frame), a window that fades into it, the window's light falling on the floor. The side walls are lit differently: cold plaster on the left, the lamp's faint warmth on the right, and tiny broken pieces of the font drip down both, barely there, like rain made of text.
- **Colour:** sad blues for the world. Warm reds, oranges and amber only for what is happening: the lamp's rim, the kick's edge, resonance, fry, the playheads, the meter you are turning.
- **Its own font:** a droopy 4 x 6 face. Crossbars sag, tops are rounded, the S slides, the T leans and the V is lopsided.

## Controls

Hold a part's button (in the browser emulator, click it: the page buttons latch there, since one mouse can't hold a button and turn a knob): its schematic opens in a dark glass drawer over the bottom of the screen and the four knobs turn that part, each knob's name over its meter (the one you turn lights up). Let go and the drawer slides away and the knobs go back to the Body. Turning a knob with nothing held shows the Body's drawer for a moment. The blob stays big and shows faintly through the glass. The drawer is drawn in the room's cold colours, like the drum grid.

| Page | Its schematic |
| --- | --- |
| Body | the wavetable's frames stacked, the one playing in front, the detuned copy sliding, the position on a frame ladder |
| Eyes | the low-pass curve and its resonance peak, the cutoff marked, and where the note's envelope opens it now (red) |
| Mouth | the ADSR, a red dot riding it where the loudest note is |
| Vibe | the LFO's wave (more cycles: faster), a red dot at its phase, where it goes |
| Tears | the delay's taps, ping (blue) and pong (coral), each smaller; the chorus as a wobbling ribbon |
| Walls | the reverb's tail (its length: Size), tape's wobble on it, the fry's transfer curve |
| Tape deck | the drum grid as round steps in the room's cold colours (kick pale blue, snare tear-blue, hats rain-grey, bass violet; soft hits dimmer, empty steps rings), the fill on the fourth bar, a pale glint for the playhead |
| Mixer | four channel strips: each a fader and a live meter (voice, drums, bass, gunk) |
| Arp | the next 16 steps as a piano roll, the playing one lit, each as long as the gate |
| Setup | a note gliding to the next, MIDI out and channel |

| Button | Part | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- | --- |
| HOME | Body | Table | Position | Detune | Sub |
| EDIT | Eyes | Cutoff | Resonance | Env amount | Mod decay |
| ENV | Mouth | Attack | Decay | Sustain | Release |
| LFO | Vibe | Rate | Depth | Shape (sway, bob, bounce, nod, twitch) | Destination (shape, eyes, voice, volume) |
| FX | Tears | Chorus | Delay | Time | Feedback |
| SEL | Walls | Reverb | Size | Fry | Tape |
| SEQ | Tape deck | Style | Tempo | Swing | Human (looser timing and velocity) |
| GLO | Mixer | Voice | Drums | Bass (0: no bass) | Gunk |
| GLO tap, then hold | Setup | Glide | MIDI channel | MIDI out | Tune |
| ARP | Arp | Mode (up, down, up and down, wander, stab) | Rate (1/4 .. 1/32, triplets) | Octaves (1-3) | Gate |

- **ALGORITHM** changes only the instrument's sound source, its wavetable (the blob's body shape). The caption says which ("WAVE: SIGH"); everything else stays.
- **PRESETS** loads a whole saved sound at once: wave, filter, envelope, LFO and effects. Its card opens in the drawer: the name, which of the eight it is, and thumbnails of the wave, filter and envelope it brought.
- **SELECT**: the drum tape (the blob swaps the cassette and shrugs).
- **Close-ups:** while you hold EDIT (Eyes) the view moves in on its eyes; while you hold ENV (Mouth), on its mouth.
- **PLAY**: drums on and off. **ARP**: tap for the arpeggiator on or off; hold for its page: the keys you hold become its pattern, at the tempo, starting each bar with the drums. **REC**: hold (the keys stay down, so the arpeggiator keeps going).
- **OCT− / OCT+**: octave. The 27 keys play F3 to G5.
- **SAVE**: saves (it also saves by itself when quiet). Hold for 1.5 s, then press SAVE again, to reset everything. The blob keeps its level.
- The lowest and highest keys together: it gives up.

**Moods:** Monday, 3AM, Left on read, Rain again, NPC, Deep fried, Church of nothing, Hold music.

**Drum styles:** each named after its genre, sadly: Grief drill, Empty city pop, Gloom gabber, Missed phonk call, Jersey cry club, Heartbreakcore, Amapia-no, Cold footwork, Dembow out, Vaporweep, Hyperflop, Funeral bossa, Office ambivalent. Each style has a main bar, a fill every fourth bar, its own tempo and swing, and a synthesized kit (no samples). In Amapia-no the bass is a log drum.

**Gunk** is the room's texture: rain on the window, a record's crackle, a low rumble. It swells when nothing plays and steps back when something does, and the rain on the window follows it.

**The bass is always sad:** the auto-bass plays natural minor. Hold something that sounds major (or one note) and it plays the relative minor, a minor third down: hold C and it plays A minor, every note of which is also in C major. Hold a minor chord and it plays that minor. Any note in a pattern outside the minor scale is pulled onto it.

**MIDI in:** notes with velocity, and pitch bend. The mod wheel moves the position and program change
picks a mood. CCs: 74 cutoff, 71 resonance, 73 attack, 72 release, 70 position, 91 reverb, 93 chorus,
94 delay, 7 volume.

**Levels:** every note is XP. Levels change nothing, but the captions say so ("LEVEL UP. NOTHING
CHANGED."). Level 10 gives it a balloon, mostly deflated.

## The sound

- **8 wavetables:** Sigh, Moan, Pulse, Crush, Warble, Fold, Church, Sync. Each has 8 frames, built at
  boot from harmonic recipes, so they take no flash space. Each frame is mipmapped at five sizes, so
  high notes don't alias (the test checks C7 on Sync: aliases at −68 dB).
- **6 voices:** two detuned oscillators panned apart, a sine sub, slow analog drift, a resonant
  low-pass (Simper's SVF) with key tracking, an amp ADSR and a mod envelope on the cutoff. A stolen
  voice restarts from its current level, so stealing doesn't click.
- **One LFO** with 5 shapes and 4 destinations.
- **Effects:** fry (drive, then bitcrush and sample-rate crush), a three-tap chorus, a ping-pong delay
  (a half-rate int16 line, its time wobbling with Tape), Dattorro's plate reverb, tape (wow, flutter,
  saturation, a darker top) and a limiter.
- **Fry is level-matched:** more fry is more grit, not more loud (full fry stays within about 2 dB of clean), and it glides, so a preset change doesn't slam it in.
- **Drums:** synthesized kick (808 to gabber), snare, clap, closed and open hats, perc (rim, cowbell,
  log drum, squeak), a sliding 808 bass and record crackle. They run sample-accurate with swing.

The test suite renders every table and every drum style to WAV in `build/host/synth/`.

## Try it

```bash
tests/run_tests.sh
```

It runs the engine tests and writes the demo WAVs, then the app scenario (screenshots land in
`build/scenarios/blob/`), then the browser build if Emscripten is installed. To play it in a browser:

```bash
web/emu/build.sh && node tools/serve_emu.mjs
```

Then open <http://localhost:8137>, or open `build/emu/MOPE-1.html` straight from disk. Computer keys:
`Q`–`I` and `A`–`K` play the white keys and `1`–`0`, `-` the black ones. `Space` starts the drums,
`B` the bass and `Enter` hold. `↑` `↓` turn ALGORITHM (the instrument) and `←` `→` SELECT (the drum tape).

## Before it goes on an FM-1

The device build needs Docker and the JieLi toolchain and SDK ([BUILDING.md](BUILDING.md)). Neither has
been run for MOPE-1 yet. Before the first install:

1. Run `./build.sh`. It checks the image size and the RAM and pool budget. The pool should fit: the
   simulator asserts the same sum.
2. Download M-VAVE's stock FM-1 V15 ("PC Firmware") so you can always go back. FiMba-1's safe mode and
   rescue tools are unchanged: after two failed starts it comes up in safe mode with USB on.
3. Measure the audio load: hold six notes with the drums and every effect on. Compared on the same Mac
   against FiMba-1's measured 48% for 12 voices, the estimate is about 40% of the FM-1's budget. If it
   is too high, lower `SY_NVOICE` in `firmware/src/dsp/synth.h`.
4. Building the tables at boot takes about 12 ms on a Mac, so expect well under a second on the FM-1.
   The watchdog is fed while they build.

The package identity is `FM-1_7…` (FiMba-1 uses `FM-1_8…`), so the two are never confused.

## Licence

GPL-3.0-only, like everything it is built on: see [LICENSING.md](LICENSING.md). Not affiliated with
M-VAVE.
