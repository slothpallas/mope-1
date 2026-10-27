// SPDX-License-Identifier: GPL-3.0-only
// MOPE-1's voice in the browser: an AudioWorklet that owns mope.wasm (web/play/bridge.c), the whole
// FM-1 app. Every render quantum pulls 128 frames from it, which moves the simulated FM-1 on. The page
// sends what is held and turned; about 30 times a second this sends back the lights, a changed
// screen and anything saved.
//
// Messages in:  {is: "start", wasm, saves, volume}  {is: "hold", buttons, keys}  {is: "turn", knob, by}
//               {is: "midi", bytes: [st, d1, d2]}   {is: "volume", to}
// Messages out: {is: "on"}  {is: "frame", buttons, keys, dim, screen?, saves?, cpu?}

const TAP = 1536;                 // frames (35 ms): a press shorter than this still reaches the firmware's scan
const FRAME = 44100 / 30;         // frames between two frames for the page
// (a worklet may have no performance clock: then milliseconds, averaged over a second)
const msNow = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();

class MopeVoice extends AudioWorkletProcessor {
  constructor() {
    super();
    this.app = null;
    this.held = { buttons: 0, keys: 0 };
    this.pressedAt = { buttons: new Array(32).fill(-1e9), keys: new Array(32).fill(-1e9) };
    this.frames = 0;
    this.nextFrame = 0;
    this.seen = { screen: -1, saves: -1 };
    this.work = { ms: 0, frames: 0 };
    this.port.onmessage = (e) => this.receive(e.data);
  }

  async receive(m) {
    if (m.is === "start") return this.start(m);
    if (!this.app) return;
    if (m.is === "hold") {
      for (const part of ["buttons", "keys"]) {
        const was = this.held[part], now = m[part] >>> 0;
        for (let i = 0; i < 32; i++) if ((now >>> i & 1) && !(was >>> i & 1)) this.pressedAt[part][i] = this.frames;
        this.held[part] = now;
      }
      this.applyHold();
    } else if (m.is === "turn") this.app.play_turn(m.knob, m.by | 0);
    else if (m.is === "midi") this.app.play_midi(m.bytes[0], m.bytes[1] | 0, m.bytes[2] | 0);
    else if (m.is === "volume") this.app.play_volume(m.to | 0);
  }

  async start(m) {
    // the app was built for WASI but never uses a file or the clock: every import answers 0
    const quiet = new Proxy({}, { get: () => () => 0 });
    const { instance } = await WebAssembly.instantiate(m.wasm, { wasi_snapshot_preview1: quiet, env: quiet });
    const app = instance.exports;
    if (app._initialize) app._initialize();
    this.memory = app.memory;
    const room = app.play_save_room();
    for (const [obj, bytes] of Object.entries(m.saves || {})) {
      const b = new Uint8Array(bytes).subarray(0, room);
      new Uint8Array(this.memory.buffer, app.play_save(+obj), b.length).set(b);
      app.play_save_sized(+obj, b.length);
    }
    app.play_volume(m.volume ?? 2800);
    app.play_start();
    this.app = app;
    this.port.postMessage({ is: "on" });
  }

  // a press keeps its bit for at least TAP frames, so a quick tap is never lost between two scans
  applyHold() {
    const mask = (part) => {
      let m = this.held[part];
      this.pressedAt[part].forEach((t, i) => { if (this.frames - t < TAP) m |= 1 << i; });
      return m >>> 0;
    };
    this.app.play_hold(mask("buttons"), mask("keys"));
  }

  report() {
    const app = this.app;
    const st = new Uint32Array(this.memory.buffer, app.play_status(), 7);
    const msg = { is: "frame", buttons: st[1], keys: st[2], dim: st[3] };
    const moving = [];
    if (st[0] !== this.seen.screen) {
      this.seen.screen = st[0];
      msg.screen = new Uint16Array(this.memory.buffer, app.play_screen(), 240 * 240).slice();
      moving.push(msg.screen.buffer);
    }
    if (st[5] !== this.seen.saves) {
      this.seen.saves = st[5];
      msg.saves = {};
      for (let obj = 0; obj < app.play_saves(); obj++) {
        const n = app.play_save_size(obj);
        if (n >= 0) msg.saves[obj] = new Uint8Array(this.memory.buffer, app.play_save(obj), n).slice();
      }
    }
    if (this.work.frames >= 44100) {                 // about once a second: the share of real time the app takes
      msg.cpu = Math.round(100 * this.work.ms / (this.work.frames / 44.1));
      this.work.ms = this.work.frames = 0;
    }
    this.port.postMessage(msg, moving);
  }

  process(_in, outputs) {
    if (!this.app) return true;
    const [left, right] = outputs[0], n = left.length;
    if (this.pressedAt.buttons.some((t) => this.frames - t < TAP + n) || this.pressedAt.keys.some((t) => this.frames - t < TAP + n))
      this.applyHold();
    const t0 = msNow();
    const at = this.app.play_pull(n);
    this.work.ms += msNow() - t0;
    this.work.frames += n;
    const lr = new Float32Array(this.memory.buffer, at, n * 2);
    for (let i = 0; i < n; i++) {
      left[i] = lr[2 * i];
      if (right) right[i] = lr[2 * i + 1];
    }
    this.frames += n;
    if (this.frames >= this.nextFrame) {
      this.nextFrame = this.frames + FRAME;
      this.report();
    }
    return true;
  }
}

registerProcessor("mope-voice", MopeVoice);
