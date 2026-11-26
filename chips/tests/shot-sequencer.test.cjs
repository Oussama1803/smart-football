// Test the same WASM artifact loaded by firmware/wokwi-test/wokwi.toml.
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

function simulator() {
  const memory = new WebAssembly.Memory({ initial: 16 });
  const attrs = [], pins = [], timers = [], writes = [];
  let now = 0, instance;
  const u32 = p => new DataView(memory.buffer).getUint32(p, true);
  const string = p => {
    const bytes = new Uint8Array(memory.buffer);
    let end = p;
    while (bytes[end]) end++;
    return Buffer.from(bytes.slice(p, end)).toString();
  };
  const env = {
    memory,
    attrInit(p, value) { attrs.push({ name: string(p), value }); return attrs.length - 1; },
    attrReadFloat: id => attrs[id].value,
    pinInit(p, mode) {
      pins.push({ name: string(p), mode, value: mode === 17 ? 1 : 0 });
      return pins.length - 1;
    },
    pinWrite(id, value) {
      const pin = pins[id];
      assert.equal(pin.mode, 17, `${pin.name} is a high-initialized output`);
      if (pin.value !== value) {
        pin.value = value;
        writes.push({ pin: pin.name, value, t: now });
      }
    },
    pinDACWrite(id, value) {
      pins[id].value = value;
      writes.push({ pin: pins[id].name, value, t: now });
      return value;
    },
    timerInit(p) { timers.push({ user: u32(p), callback: u32(p + 4), due: Infinity }); return timers.length - 1; },
    timerStart(id, delay, repeat) { Object.assign(timers[id], { due: now + delay, delay, repeat }); },
    timerStop(id) { timers[id].due = Infinity; },
  };
  const wasm = path.resolve(__dirname, '../../firmware/wokwi-test/chips/shot-sequencer.chip.wasm');
  instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(wasm)), { env });
  instance.exports.chipInit();
  return {
    writes,
    set(name, value) { attrs.find(attr => attr.name === name).value = value; },
    advance(duration) {
      const end = now + duration;
      for (;;) {
        const timer = timers.reduce((soonest, candidate) =>
          !soonest || candidate.due < soonest.due ? candidate : soonest, null);
        if (!timer || timer.due > end) break;
        now = timer.due;
        timer.due = timer.repeat ? now + timer.delay : Infinity;
        instance.exports.__indirect_function_table.get(timer.callback)(timer.user);
      }
      now = end;
    },
  };
}

const targets = [
  ['upper_left', 0], ['upper_right', 1], ['lower_left', 2], ['lower_right', 3],
];

for (const [zone, pinIndex] of targets) {
  test(`${zone}: 72 km/h beam sequence and impact 12 ms after beam 2`, () => {
    const sim = simulator();
    sim.advance(5000);
    sim.set('scenario', pinIndex);
    sim.set('run', 1);
    sim.advance(23000);
    const falling = sim.writes.filter(w => (w.pin === 'IR1' || w.pin === 'IR2') && w.value === 0);
    assert.deepEqual(falling, [{ pin: 'IR1', value: 0, t: 6000 }, { pin: 'IR2', value: 0, t: 16000 }]);
    const impact = sim.writes.find(w => w.pin === `PZ${pinIndex + 1}` && w.value > 0);
    assert.equal(impact.pin, `PZ${pinIndex + 1}`);
    assert.equal(impact.t, 28000);
    assert.ok(Math.abs(impact.value - 3.3) < 0.00001);
    for (let i = 0; i < 4; i++) {
      if (i !== pinIndex) assert.equal(sim.writes.some(w => w.pin === `PZ${i + 1}` && w.value > 0), false);
    }
  });
}

test('no impact: beam sequence still occurs and all piezos stay silent', () => {
  const sim = simulator();
  sim.advance(5000);
  sim.set('scenario', 4);
  sim.set('run', 1);
  sim.advance(30000);
  assert.deepEqual(sim.writes.filter(w => (w.pin === 'IR1' || w.pin === 'IR2') && w.value === 0)
    .map(w => [w.pin, w.t]), [['IR1', 6000], ['IR2', 16000]]);
  assert.equal(sim.writes.some(w => w.pin.startsWith('PZ') && w.value > 0), false);
});

test('simultaneous: same timestamp, separate amplitudes, strongest is lower-right', () => {
  const sim = simulator();
  sim.advance(5000);
  sim.set('scenario', 5);
  sim.set('run', 1);
  sim.advance(23000);
  const impacts = sim.writes.filter(w => w.t === 28000 && w.value > 0);
  assert.equal(impacts.length, 2);
  assert.deepEqual(impacts.map(w => [w.pin, w.t]), [['PZ1', 28000], ['PZ4', 28000]]);
  assert.equal(impacts[0].value, 1);
  assert.ok(Math.abs(impacts[1].value - 3.3) < 0.00001);
  assert.ok(impacts[1].value > impacts[0].value);
});

test('held run control generates only one pair of beam edges', () => {
  const sim = simulator();
  sim.set('run', 1);
  sim.advance(1000000);
  assert.equal(sim.writes.filter(w => (w.pin === 'IR1' || w.pin === 'IR2') && w.value === 0).length, 2);
});
