// SPDX-License-Identifier: GPL-3.0-only
// MOPE-1 to play (build/play) on http://localhost:8137 for local testing: node tools/serve_play.mjs
import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = fileURLToPath(new URL("../build/play/", import.meta.url));
const PORT = Number(process.env.PORT || 8137);
const TYPES = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".wasm": "application/wasm",
                ".json": "application/json", ".png": "image/png" };
createServer(async (req, res) => {
  try {
    let url = decodeURIComponent(req.url.split("?")[0]);
    if (url.endsWith("/")) url += "index.html";
    const path = join(ROOT, normalize(url).replace(/^(\.\.[/\\])+/, ""));
    const body = await readFile(path);
    res.writeHead(200, { "Content-Type": TYPES[extname(path)] || "application/octet-stream", "Cache-Control": "no-store" });
    res.end(body);
  } catch {
    res.writeHead(404, { "Content-Type": "text/plain" });
    res.end("not found");
  }
}).listen(PORT, "127.0.0.1", () => console.log(`emu: http://localhost:${PORT}/`));
