// SPDX-License-Identifier: GPL-3.0-only
// MOPE-1's page in headless Chrome, twice: the one-file build opened from disk (file://), and the
// served page under a strict content policy like a portfolio site's (no data: scripts, WebAssembly
// allowed). Each time it must switch on, sound for a held key and for a MIDI note, and draw.
//   node tests/host/page_test.mjs CHROME
import { spawn } from "node:child_process";
import { createServer } from "node:http";
import { readFileSync, rmSync } from "node:fs";
import { resolve, extname } from "node:path";

const PROBE = `(async () => {
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const AC = window.AudioContext; let ear = null;
  window.AudioContext = function (o) {              // listen to what reaches the speakers
    const ac = new AC(o); ear = ac.createAnalyser(); ear.fftSize = 2048;
    const connect = AudioNode.prototype.connect;
    AudioNode.prototype.connect = function (to, ...r) { if (to === ac.destination && this !== ear) connect.call(this, ear); return connect.call(this, to, ...r); };
    return ac;
  };
  const loudest = async (n) => { let most = 0; const b = new Float32Array(2048);
    for (let i = 0; i < n; i++) { await wait(30); ear.getFloatTimeDomainData(b); for (const v of b) most = Math.max(most, Math.abs(v)); } return most; };
  document.getElementById("on").click();
  for (let i = 0; i < 100 && !document.getElementById("power").hidden; i++) await wait(100);
  if (!document.getElementById("power").hidden) return "did not switch on: " + document.getElementById("said").textContent;
  await wait(800);
  voice.port.postMessage({ is: "hold", buttons: 0, keys: 1 << 12 });
  const key = await loudest(30);
  voice.port.postMessage({ is: "hold", buttons: 0, keys: 0 });
  voice.port.postMessage({ is: "midi", bytes: [0x90, 76, 110] });
  await wait(400);
  const midi = await loudest(10);
  const px = document.getElementById("screen").getContext("2d").getImageData(0, 0, 240, 240).data;
  let lit = 0; for (let i = 0; i < px.length; i += 4) lit += px[i] + px[i + 1] + px[i + 2] > 60;
  return \`key=\${key.toFixed(3)} midi=\${midi.toFixed(3)} screen=\${lit}\`;
})()`;

const chrome = process.argv[2], profile = resolve("build/play/chrome_profile");
rmSync(profile, { recursive: true, force: true });
const CSP = "default-src 'self'; script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval'; style-src 'self' 'unsafe-inline'; connect-src 'self'";
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm" };
const server = createServer((q, r) => {
  const f = q.url === "/" ? "index.html" : q.url.slice(1).split("?")[0];
  let body;
  try { body = readFileSync("build/play/" + f); } catch { r.writeHead(404); r.end(); return; }
  r.writeHead(200, { "Content-Type": TYPES[extname(f)] || "application/octet-stream", "Content-Security-Policy": CSP });
  r.end(body);
}).listen(0, "127.0.0.1");
await new Promise((r) => server.on("listening", r));

const browser = spawn(chrome, ["--headless=new", "--disable-gpu", "--autoplay-policy=no-user-gesture-required", "--no-first-run",
  `--user-data-dir=${profile}`, "--remote-debugging-port=9334", "about:blank"], { stdio: "ignore" });
const end = (code) => { browser.kill("SIGKILL"); server.close(); process.exit(code); };
setTimeout(() => { console.log("TIMEOUT"); end(2); }, 90000);
let tab;
for (let i = 0; i < 50 && !tab; i++) {
  await new Promise((r) => setTimeout(r, 200));
  try { tab = (await (await fetch("http://127.0.0.1:9334/json")).json()).find((t) => t.type === "page"); } catch {}
}
const ws = new WebSocket(tab.webSocketDebuggerUrl);
let n = 0; const waiting = new Map(), errors = [];
const cdp = (method, params = {}) => new Promise((res) => { waiting.set(++n, res); ws.send(JSON.stringify({ id: n, method, params })); });
ws.onmessage = (e) => {
  const m = JSON.parse(e.data);
  if (m.id && waiting.has(m.id)) { waiting.get(m.id)(m.result); waiting.delete(m.id); }
  if (m.method === "Runtime.exceptionThrown") errors.push(m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text);
  if (m.method === "Log.entryAdded" && /Content Security Policy/.test(m.params.entry.text)) errors.push(m.params.entry.text);
};
await new Promise((r) => (ws.onopen = r));
await cdp("Runtime.enable");
await cdp("Log.enable");

let failed = 0;
for (const [what, url] of [["from disk", "file://" + resolve("build/play/MOPE-1.html")],
                           ["served, strict policy", `http://127.0.0.1:${server.address().port}/`]]) {
  errors.length = 0;
  await cdp("Page.navigate", { url });
  await new Promise((r) => setTimeout(r, 1500));
  const r = await cdp("Runtime.evaluate", { expression: PROBE, awaitPromise: true });
  const said = r?.result?.value || "no answer";
  const m = /key=([\d.]+) midi=([\d.]+) screen=(\d+)/.exec(said);
  const ok = m && +m[1] > 0.05 && +m[2] > 0.02 && +m[3] > 10000 && !errors.length;
  failed += !ok;
  console.log(`${ok ? "ok  " : "FAIL"} ${what}: ${said}${errors.length ? " | " + errors.join(" | ") : ""}`);
}
end(failed ? 1 : 0);
