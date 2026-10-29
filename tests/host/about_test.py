#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The about page as built (web/about/build.py -> SITE/about): every picture is there, 120 x 120 and
described; every link inside the site and every #anchor lands; it stays light.
  about_test.py SITE          (e.g. build/site)"""
import pathlib
import struct
import sys
from html.parser import HTMLParser
from urllib.parse import urlsplit

BUDGET = 600 * 1024                     # the page and all its pictures (og.png apart: only for link previews)


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.imgs, self.srcsets, self.links, self.ids = [], [], [], set()

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "img":
            self.imgs.append(a)
        elif tag == "source" and "srcset" in a:
            self.srcsets.append(a["srcset"])
        elif tag == "a" and "href" in a:
            self.links.append(a["href"])


def png_size(path):
    b = path.read_bytes()[:24]
    if b[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", b[16:24])


def main():
    site = pathlib.Path(sys.argv[1])
    about = site / "about"
    page = Page()
    page.feed((about / "index.html").read_text())
    fails = []

    def check(ok, what):
        print(("  ok   " if ok else "  FAIL ") + what)
        if not ok:
            fails.append(what)

    for a in page.imgs:
        p = about / a["src"]
        check(p.is_file(), f"picture {a['src']} is there")
        check(bool(a.get("alt", "").strip()), f"picture {a['src']} is described")
        if p.is_file():
            check(png_size(p) == (120, 120), f"picture {a['src']} is the screen's 120 x 120")
    for s in page.srcsets:
        check((about / s).is_file(), f"still {s} is there")
    for href in page.links:
        u = urlsplit(href)
        if u.scheme in ("http", "https"):
            continue
        if href.startswith("#"):
            check(href[1:] in page.ids, f"anchor {href} lands")
            continue
        target = (about / u.path).resolve()
        if target.is_dir():
            target = target / "index.html"
        if not target.is_relative_to(site.resolve()):
            check(False, f"link {href} stays in the site")
        elif target == (site / "index.html").resolve() and not target.exists():
            print(f"  skip {href} (the player isn't in {site})")
        else:
            check(target.exists(), f"link {href} lands")
            if u.fragment and target.suffix == ".html":
                check(f'id="{u.fragment}"' in target.read_text(), f"link {href}: its anchor is there")
    size = sum(f.stat().st_size for f in about.rglob("*") if f.is_file() and f.name != "og.png")
    check(size <= BUDGET, f"the page weighs {size // 1024} KB (at most {BUDGET // 1024})")
    print("about page: " + ("all passed" if not fails else f"{len(fails)} FAILED"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
