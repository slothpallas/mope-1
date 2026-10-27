// SPDX-License-Identifier: GPL-3.0-only
// MOPE-1's WebAssembly (web/play/bridge.c) in Node: start it, run a scale up the white keys, then a
// MIDI note. It must sound, stay under full scale, draw its screen and light the keys it plays.
//   node tests/host/play_test.mjs build/play/mope.wasm
import { readFileSync } from "node:fs";
const quiet = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(readFileSync(process.argv[2]), { wasi_snapshot_preview1: quiet, env: quiet });
const app = instance.exports;
if (app._initialize) app._initialize();
app.play_volume(2800);
app.play_start();
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const loudest = (pulls) => {
  let most = 0;
  for (let i = 0; i < pulls; i++) for (const v of new Float32Array(app.memory.buffer, app.play_pull(128), 256)) most = Math.max(most, Math.abs(v));
  return most;
};
let scale = 0, litSeen = 0;
for (const k of WHITE) {
  app.play_hold(0, 1 << k);
  scale = Math.max(scale, loudest(20));
  litSeen += !!(new Uint32Array(app.memory.buffer, app.play_status(), 7)[2] & (1 << k));
}
app.play_hold(0, 0);
loudest(1500);                                       // let it ring out
app.play_midi(0x90, 72, 110);
const midi = loudest(100);
const st = new Uint32Array(app.memory.buffer, app.play_status(), 7);
const ok = scale > 0.05 && scale < 1 && midi > 0.05 && st[0] > 10 && litSeen >= 12;
console.log(`scale peak ${scale.toFixed(3)}, midi peak ${midi.toFixed(3)}, screens ${st[0]}, keys lit ${litSeen}/16: ${ok ? "ok" : "FAIL"}`);
process.exit(ok ? 0 : 1);
