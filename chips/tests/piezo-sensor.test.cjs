// Real WASM, simulated API host. No ESP32 ADC conversion is assumed here.
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

function simulator(order = [1, 2, 3, 4]) {
  const memory = new WebAssembly.Memory({ initial: 16 });
  const attrs = [], pins = [], timers = [], writes = [];
  let owner, now = 0, instance;
  const u32 = p => new DataView(memory.buffer).getUint32(p, true);
  const string = p => {
    const bytes = new Uint8Array(memory.buffer);
    let end = p;
    while (bytes[end]) end++;
    return Buffer.from(bytes.slice(p, end)).toString();
  };
  const env = {
    memory,
    attrInit(p, value) { attrs.push({ owner, name: string(p), value }); return attrs.length - 1; },
    attrReadFloat: id => attrs[id].value,
    pinInit(p, mode) { assert.equal(mode, 4); pins.push({ owner, value: 0 }); return pins.length - 1; },
    pinDACWrite(id, value) { pins[id].value = value; writes.push({ owner: pins[id].owner, t: now, value }); },
    timerInit(p) { timers.push({ user: u32(p), callback: u32(p + 4), due: Infinity }); return timers.length - 1; },
    timerStart(id, delay, repeat) { Object.assign(timers[id], { due: now + delay, delay, repeat }); },
    timerStop(id) { timers[id].due = Infinity; },
  };
  instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(
    path.resolve(__dirname, '../../firmware/chips/piezo-sensor.chip.wasm'))), { env });
  for (owner of order) instance.exports.chipInit();
  return {
    writes,
    set(owner, name, value) { attrs.find(a => a.owner === owner && a.name === name).value = value; },
    level(owner) { return pins.find(p => p.owner === owner).value; },
    advance(duration) {
      const end = now + duration;
      for (;;) {
        const timer = timers.reduce((a, b) => !a || b.due < a.due ? b : a, null);
        if (timer.due > end) break;
        now = timer.due;
        timer.due = timer.repeat ? now + timer.delay : Infinity;
        instance.exports.__indirect_function_table.get(timer.callback)(timer.user);
      }
      now = end;
    },
  };
}

for (const order of [[1, 2, 3, 4], [4, 3, 2, 1]]) {
  for (const zone of [1, 2, 3, 4]) {
    test(`piezo${zone} isolated, init ${order}`, () => {
      const sim = simulator(order);
      sim.advance(12000);
      assert.ok(sim.writes.every(w => w.value === 0));
      sim.set(zone, 'peakVolts', 3.3);
      sim.set(zone, 'trigger', 1);
      sim.advance(1000);
      assert.ok(Math.abs(sim.level(zone) - 3.3) < 0.00001);
      for (const other of [1, 2, 3, 4].filter(n => n !== zone)) assert.equal(sim.level(other), 0);
      sim.advance(5000);
      assert.ok(Math.abs(sim.level(zone) - 3.3 / Math.E) < 0.001, 'tau is 5 ms');
      sim.advance(100000);
      assert.equal(sim.level(zone), 0);
      assert.equal(sim.writes.filter(w => w.value > 3.29).length, 1, 'held trigger emits once');
      sim.set(zone, 'trigger', 0);
      sim.advance(2000);
      sim.set(zone, 'trigger', 1);
      sim.advance(1000);
      assert.ok(sim.level(zone) > 3.29);
    });
  }
}

test('simultaneous impacts retain independent amplitudes and decay timers', () => {
  const sim = simulator();
  sim.set(1, 'peakVolts', 1);
  sim.set(4, 'peakVolts', 3.3);
  sim.set(1, 'trigger', 1);
  sim.set(4, 'trigger', 1);
  sim.advance(1000);
  assert.equal(sim.level(1), 1);
  assert.ok(sim.level(4) > 3.29);
  sim.advance(5000);
  assert.ok(Math.abs(sim.level(1) - 1 / Math.E) < 0.001);
  assert.ok(Math.abs(sim.level(4) - 3.3 / Math.E) < 0.001);
  assert.equal(sim.level(2), 0);
  assert.equal(sim.level(3), 0);
});
