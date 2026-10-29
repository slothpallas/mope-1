#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MOPE-1's about page (web/about/index.html): the blob in its states, a short manual, how to try and
install it. The pictures are the firmware's own screen, from the host simulator
(tests/scenarios/site.mope, run by tests/run_tests.sh): each 240 x 240 shot halved back to its 120 x 120
pixels, shown large and crisp by the page. The manual's knob tables come from README.md's Button
table, so the two can't drift apart. Needs Pillow.

  build.py SCENARIOS_DIR OUT_DIR [VERSION]   (e.g. build/scenarios/site build/site/about 0.3.0)"""
import html
import pathlib
import re
import sys

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent

# the states: name, caption (what it is, flatly), the control that does it, what the picture shows
STATES = [
    ("awake", "Filter open. It is regrettably alert.", "EDIT · knob 1 up",
     "The blob with its eyes wide open and its mouth open on a chord, in its rainy room."),
    ("stoned", "Filter down. Best not to talk to it.", "EDIT · knob 1 down",
     "The blob with half-closed, heavy eyelids."),
    ("lasers", "Resonance up. It has seen something.", "EDIT · knob 2 up",
     "Red laser beams coming out of the blob's eyes."),
    ("dance", "The LFO makes it dance, not that it enjoys it.", "LFO · knobs 2 and 3",
     "The blob bouncing, squashed as it lands."),
    ("yawn", "With a long release it takes a while to stop.", "ENV · knob 4 up",
     "The blob's mouth slowly closing after the note has ended."),
    ("cry", "Delay: one tear per echo, and they collect on the floor.", "FX · knobs 2 and 4",
     "Tears falling from the blob into a dark puddle on the floor that mirrors it."),
    ("void", "Reverb. The room got bigger and it didn't.", "SEL · knobs 1 and 2 up",
     "The walls gone: a small blob floating in the dark, among stars."),
    ("fire", "Too loud, so it catches fire. It's fine.", "everything up at once",
     "Embers flying off the blob, which looks calm."),
    ("asleep", "Leave it alone for a while and it falls asleep.", "leave it",
     "The blob asleep, eyes shut."),
    ("stare", "Later it wakes up and looks at you.", "leave it longer",
     "The blob awake again, staring straight out of the screen."),
]

# the manual: each button's part, what it does to the blob, its drawer's picture (README has the knobs)
PARTS = [
    ("HOME", "Body", "page_body", "Its outline is the wavetable. Position reshapes it, detune gives it a ghost that drifts "
     "apart, sub makes it heavier.", ("Shadow", "page_shadow", "a second oscillator in every voice. It can sit a few semitones "
                                      "or two octaves away, mixed in or modulating the first.")),
    ("EDIT", "Eyes", "page_eyes", "Its eyes are the filter: wide awake when it's open, half-lidded and then shut as you close "
     "it. Resonance shrinks the pupils until they turn into lasers.", None),
    ("ENV", "Mouth", "page_mouth", "Its mouth opens with the note, stays open at the sustain and closes with the release. A "
     "long release is a yawn.", ("Sigh", "page_sigh", "a second envelope that can sweep the wavetable up or down on every "
                                 "note, and open the eyes.")),
    ("LFO", "Vibe", "page_vibe", "It dances at the rate, as far as the depth: sway, bob, bounce, nod, twitch, heave, "
     "wander, stagger, throb.", ("Tremble", "page_tremble", "a second LFO, for the things the first one is busy with.")),
    ("FX", "Tears", "page_tears", "One tear per echo, each smaller than the last. Chorus: see-through copies of it peek "
     "out from behind.", None),
    ("SEL", "Walls", "page_walls", "The reverb is the room: the bigger it gets, the smaller the blob looks, and past the "
     "walls there's only the void. Fry deep-fries it, and Tape makes everything wobble a little.", None),
    ("GLO", "Mixer", "page_mixer", "How loud each part is: the blob, the drums, the bass and the rain.",
     ("Setup", None, "glide, the MIDI channel, MIDI out and the tuning.")),
    ("SEQ", "Loop", None, "Record what you play (REC), then fix it one step at a time.",
     ("Tape deck", "page_deck", "the backing band: a cassette in the corner. Kicks squash it, snares make it blink.")),
    ("ARP", "Arp", None, "Tap it to turn the arpeggiator on or off, or hold it for its page. The keys you hold become its pattern.", None),
]


def shrink(path):
    """a 240 x 240 shot back to its 120 x 120 pixels (each is 2 x 2 in the shot)"""
    im = Image.open(path).convert("RGB")
    if im.size != (240, 240):
        raise SystemExit(f"{path}: {im.size}, not 240 x 240")
    return im.resize((120, 120), Image.NEAREST)


def indexed(im):
    return im.convert("P", palette=Image.ADAPTIVE, colors=255)   # (the screen has ~50 colours: exact)


def film(frames, path, ms):
    """an animated PNG, every frame on one palette (the screen's: fewer than 256 colours, exact)"""
    strip = Image.new("RGB", (120, 120 * len(frames)))
    for i, f in enumerate(frames):
        strip.paste(f, (0, 120 * i))
    pal = strip.convert("P", palette=Image.ADAPTIVE, colors=255)
    fr = [f.quantize(palette=pal, dither=Image.Dither.NONE) for f in frames]
    fr[0].save(path, format="PNG", save_all=True, append_images=fr[1:], duration=ms, loop=0, optimize=True)


def readme_rows():
    """README's Button table: {(button, part): [knob 1..4]}"""
    text = (ROOT / "README.md").read_text()
    m = re.search(r"^\| Button \| Part \|.*?\n\| ---.*?\n((?:\|.*\n)+)", text, re.M)
    if not m:
        raise SystemExit("README.md: no Button table")
    rows = {}
    for line in m.group(1).splitlines():
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) != 6:
            raise SystemExit(f"README.md: a Button row without 6 cells: {line}")
        rows[cells[1]] = (cells[0], cells[2:])
    return rows


def esc(s):
    return html.escape(s, quote=True)


def knob_table(knobs):
    cells = "".join(f"<td><span class=k>Knob {i + 1}</span>{esc(k)}</td>" for i, k in enumerate(knobs))
    return f'<table class=knobs><tr>{cells}</tr></table>'


def screen(img, part, cls=""):
    if not img:
        return '<div class="px none" aria-hidden=true></div>'
    return (f'<img class="px{cls}" src="img/{img}.png" width=120 height=120 loading=lazy alt="The {part} page: its '
            f'drawer under the blob, the four knobs named.">')


def manual(rows):
    """each button's part, then (when it has one) its second page as a block of its own"""
    out = []
    for btn, part, img, blob, again in PARTS:
        if part not in rows or rows[part][0] != btn:
            raise SystemExit(f"README.md's Button table has no '{btn} | {part}' row")
        out.append("\n".join([
            f'<article class=part id="{part.lower()}">', screen(img, part), '<div>',
            f'<h3>The {part.lower()}</h3>',
            f'<p class=press>Press <span class=btn>{btn}</span></p>',
            f'<p>{esc(blob)}</p>', knob_table(rows[part][1]), '</div>', '</article>']))
        if again:
            name, aimg, what = again
            if name not in rows or rows[name][0] != btn + " again":
                raise SystemExit(f"README.md's Button table has no '{btn} again | {name}' row")
            out.append("\n".join([
                f'<article class="part second" id="{name.lower().replace(" ", "-")}">', screen(aimg, name), '<div>',
                f'<p class=eyebrow>The {part.lower()}\'s second page</p>',
                f'<h3>The {name.lower()}</h3>',
                f'<p class=press>Press <span class=btn>{btn}</span> again while the {part.lower()} is showing</p>',
                f'<p>{esc(what[0].upper() + what[1:])}</p>', knob_table(rows[name][1]), '</div>', '</article>']))
    return "\n".join(out)


def states():
    out = []
    for name, cap, how, alt in STATES:
        wake = ' data-wake="stare"' if name == "asleep" else ""
        out.append(f'''<figure class=state id="{name}"{wake}>
<img class=px src="img/{name}.png" data-film="img/{name}.apng" width=120 height=120 loading=lazy alt="{esc(alt)}">
<figcaption>{esc(cap)}<span>{esc(how)}</span></figcaption>
</figure>''')
    return "\n".join(out)


def main():
    scen, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    version = sys.argv[3] if len(sys.argv) > 3 else (ROOT / "VERSION").read_text().strip()
    img = out / "img"
    img.mkdir(parents=True, exist_ok=True)
    for name, *_ in STATES:
        indexed(shrink(scen / f"{name}.png")).save(img / f"{name}.png", optimize=True)
        frames = sorted(scen.glob(f"{name}_[0-9][0-9].png"))
        if not frames:
            raise SystemExit(f"{scen}: no film of {name}")
        film([shrink(f) for f in frames], img / f"{name}.apng",
             120 if name in ("asleep", "stare", "yawn", "void") else 90)
    for name in ("idle",):                              # (the top of the page: just it, in the rain)
        film([shrink(f) for f in sorted(scen.glob(f"{name}_[0-9][0-9].png"))], img / f"{name}.apng", 120)
        indexed(shrink(scen / f"{name}.png")).save(img / f"{name}.png", optimize=True)
    for _, _, pg, _, again in PARTS:
        for p in (pg, again and again[1]):
            if p:
                indexed(shrink(scen / f"{p}.png")).save(img / f"{p}.png", optimize=True)
    shrink(scen / "awake.png").resize((960, 960), Image.NEAREST).save(img / "og.png", optimize=True)   # (link previews)
    page = (HERE / "index.html").read_text()
    for slot, text in (("<!--STATES-->", states()), ("<!--MANUAL-->", manual(readme_rows())),
                       ("{{VERSION}}", esc(version))):
        if slot not in page:
            raise SystemExit(f"web/about/index.html: no {slot}")
        page = page.replace(slot, text)
    (out / "index.html").write_text(page)
    size = sum(f.stat().st_size for f in out.rglob("*") if f.is_file() and f.name != "og.png")
    print(f"about: {out}/index.html, {len(list(img.iterdir()))} pictures, {size // 1024} KB (without og.png)")


if __name__ == "__main__":
    main()
