// Execute the actual Wokwi artifact with a deterministic, shared-module API host.
// This checks chip behavior, not ESP32 interrupt latency or Wokwi integration.
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

function simulator(order = ['beam1', 'beam2']) {
  const memory = new WebAssembly.Memory({ initial: 16 });
  const attrs = [], pins = [], timers = [], edges = [];
  let owner, now = 0, instance;
  const u32 = p => new DataView(memory.buffer).getUint32(p, true);
  const string = p => {
    const bytes = new Uint8Array(memory.buffer);
    let end = p;
    while (bytes[end]) end++;
    return Buffer.from(bytes.slice(p, end)).toString();
  };
  const call = (callback, ...args) => instance.exports.__indirect_function_table.get(callback)(...args);
  const env = {
    memory,
    attrInit(p, value) {
      const name = string(p);
      attrs.push({ owner, name, value: name === 'role' ? (owner === 'beam1' ? 1 : 2) : value });
      return attrs.length - 1;
    },
    attrReadFloat: id => attrs[id].value,
    pinInit(p, mode) {
      pins.push({ owner, name: string(p), mode, value: mode === 17 ? 1 : 0 });
      return pins.length - 1;
    },
    timerInit(p) {
      timers.push({ user: u32(p), callback: u32(p + 4), due: Infinity });
      return timers.length - 1;
    },
    timerStart(id, delay, repeat) {
      Object.assign(timers[id], { due: now + delay, delay, repeat });
    },
    pinWatch(id, p) {
      pins[id].watch = { user: u32(p), edge: u32(p + 4), callback: u32(p + 8) };
      return 1;
    },
    pinWrite(id, value) {
      const pin = pins[id];
      assert.ok(pin.mode === 16 || pin.mode === 17, 'writes must target an output');
      if (pin.value === value) return;
      pin.value = value;
      if (pin.name === 'OUT') edges.push({ beam: pin.owner, value, t: now });
      if (pin.name === 'SYNC') {
        for (const peer of pins.filter(p => p !== pin && p.name === 'SYNC')) {
          const old = peer.value;
          peer.value = value;
          if (old !== value && peer.watch && (peer.watch.edge & (value ? 1 : 2))) {
            call(peer.watch.callback, peer.watch.user, pins.indexOf(peer), value);
          }
        }
      }
    },
  };
  const file = path.resolve(__dirname, '../../firmware/chips/ir-break-beam.chip.wasm');
  instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(file)), { env });
  for (owner of order) instance.exports.chipInit();
  return {
    edges,
    set(beam, name, value) { attrs.find(a => a.owner === beam && a.name === name).value = value; },
    advance(duration) {
      const end = now + duration;
      for (;;) {
        const timer = timers.reduce((a, b) => !a || b.due < a.due ? b : a, null);
        if (!timer || timer.due > end) break;
        now = timer.due;
        timer.due = timer.repeat ? now + timer.delay : Infinity;
        call(timer.callback, timer.user);
      }
      now = end;
    },
  };
}

for (const order of [['beam1', 'beam2'], ['beam2', 'beam1']]) {
  test(`72 km/h: separate instances, initialization ${order.join(', ')}`, () => {
    const sim = simulator(order);
    sim.advance(5000);
    assert.deepEqual(sim.edges, [], 'no startup pulse');
    sim.set('beam1', 'shoot', 1);
    sim.advance(50000);
    assert.deepEqual(sim.edges, [
      { beam: 'beam1', value: 0, t: 6000 },
      { beam: 'beam1', value: 1, t: 16000 },
      { beam: 'beam2', value: 0, t: 16000 },
      { beam: 'beam2', value: 1, t: 26000 },
    ]);
  });
}

test('slave controls cannot trigger or change master speed', () => {
  const sim = simulator();
  sim.set('beam2', 'shoot', 1);
  sim.set('beam2', 'ballSpeedKmh', 144);
  sim.advance(5000);
  assert.deepEqual(sim.edges, []);
  sim.set('beam1', 'shoot', 1);
  sim.advance(30000);
  const cuts = sim.edges.filter(e => e.value === 0);
  assert.deepEqual(cuts.map(e => e.beam), ['beam1', 'beam2']);
  assert.equal(cuts[1].t - cuts[0].t, 10000);
});

test('held Shoot emits once; sampled zero then one permits the next shot', () => {
  const sim = simulator();
  sim.set('beam1', 'shoot', 1);
  sim.advance(1000000);
  assert.equal(sim.edges.filter(e => e.value === 0).length, 2);
  sim.set('beam1', 'shoot', 0);
  sim.advance(2000);
  sim.set('beam1', 'shoot', 1);
  sim.advance(30000);
  assert.deepEqual(sim.edges.filter(e => e.value === 0).map(e => e.beam),
    ['beam1', 'beam2', 'beam1', 'beam2']);
});
