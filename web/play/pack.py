#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""MOPE-1's page, ready to serve and as one file that also opens from disk.

  web/play/pack.py PAGE.html BUILD_DIR

writes BUILD_DIR/index.html (the page with the installer's modules in it, beside mope.wasm and
voice.js) and BUILD_DIR/MOPE-1.html (the same with both files inside it, as base64, where the page
says <!--MOPE-EMBED-->). The installer's modules are Felucca's update protocol, web/fm1pkg.js and
web/fm1ota.js: they go where the page says /*FM1LIB*/, as plain declarations."""
import base64
import re
import sys
from pathlib import Path

WEB = Path(__file__).resolve().parents[1]
BEGIN = "/* ---- the update protocol: web/fm1pkg.js, web/fm1ota.js ---- */"
END = "/* ---- end of the update protocol ---- */"


def once(page, mark):
    if page.count(mark) != 1:
        raise SystemExit(f"pack: the page needs exactly one {mark}")


def installer():
    parts = []
    for name in ("fm1pkg.js", "fm1ota.js"):
        src = (WEB / name).read_text()
        if re.search(r"^\s*import\b", src, re.M):
            raise SystemExit(f"pack: {name} imports something; this only inlines plain modules")
        parts.append(f"/* web/{name} */\n" + re.sub(r"^export\s+", "", src, flags=re.M))
    body = "\n".join(parts).replace("</script", "<\\/script")
    return f"{BEGIN}\n{body}\n{END}"


def main(page_path, out):
    out = Path(out)
    page = Path(page_path).read_text()
    for mark in ("/*FM1LIB*/", "<!--MOPE-EMBED-->"):
        once(page, mark)
    page = page.replace("/*FM1LIB*/", installer())
    once(page, "<!--SITE-NAV-->")
    page = page.replace("<!--SITE-NAV-->", (WEB / "nav.html").read_text())   # (the site's bar, as on every page)
    (out / "index.html").write_text(page.replace("<!--MOPE-EMBED-->\n", ""))
    wasm = base64.b64encode((out / "mope.wasm").read_bytes()).decode()
    voice = base64.b64encode((out / "voice.js").read_bytes()).decode()
    embed = f'<script>\nconst MOPE_EMBED = {{ wasm: "{wasm}", voice: "{voice}" }};\n</script>'
    (out / "MOPE-1.html").write_text(page.replace("<!--MOPE-EMBED-->", embed))
    print(f"pack: {out}/index.html, {out}/MOPE-1.html ({(out / 'MOPE-1.html').stat().st_size // 1024} KiB, opens from disk)")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
