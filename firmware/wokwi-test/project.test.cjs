// Static project checks plus an API-host test of the exact Wokwi FSR artifact.
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const projectRoot = __dirname;
const diagram = JSON.parse(fs.readFileSync(path.join(projectRoot, 'diagram.json'), 'utf8'));
const config = fs.readFileSync(path.join(projectRoot, 'wokwi.toml'), 'utf8');
const declarations = [...config.matchAll(/\[\[chip\]\]\s*\r?\nname\s*=\s*'([^']+)'\s*\r?\nbinary\s*=\s*'([^']+)'/g)]
  .map(([, name, binary]) => ({ name, binary }));

test('standalone Wokwi project declares every local custom chip and firmware artifact', () => {
  const declared = new Map(declarations.map(chip => [chip.name, chip]));
  const customParts = diagram.parts.filter(part => part.type.startsWith('chip-'));
  assert.deepEqual([...declared.keys()].sort(), ['fsr-sensor', 'shot-test-sequencer']);
  assert.deepEqual(customParts.map(part => part.type.slice('chip-'.length)).sort(),
    [...declared.keys()].sort());

  for (const chip of declarations) {
    const binary = path.resolve(projectRoot, chip.binary);
    const definition = binary.replace(/\.wasm$/, '.json');
    assert.ok(fs.existsSync(binary), `missing chip binary ${chip.binary}`);
    assert.ok(fs.existsSync(definition), `missing paired chip definition ${definition}`);
    const definitionJson = JSON.parse(fs.readFileSync(definition, 'utf8'));
    assert.ok(definitionJson.pins.length > 0);
    for (const part of customParts.filter(item => item.type === `chip-${chip.name}`)) {
      for (const connection of diagram.connections.filter(row => row[0].startsWith(`${part.id}:`) || row[1].startsWith(`${part.id}:`))) {
        for (const endpoint of connection.slice(0, 2)) {
          if (endpoint.startsWith(`${part.id}:`)) {
            assert.ok(definitionJson.pins.includes(endpoint.split(':')[1]),
              `undefined ${chip.name} pin ${endpoint}`);
          }
        }
      }
    }
  }
  for (const key of ['firmware', 'elf']) {
    const match = config.match(new RegExp(`^${key}\\s*=\\s*'([^']+)'`, 'm'));
    assert.ok(match, `missing ${key} path in wokwi.toml`);
    assert.ok(fs.existsSync(path.resolve(projectRoot, match[1])), `missing ${match[1]}`);
  }
});

test('diagram has valid visible connection arrays, unique IDs, and expected GPIO mapping', () => {
  const ids = diagram.parts.map(part => part.id);
  assert.equal(new Set(ids).size, ids.length, 'part IDs must be unique');
  const idSet = new Set(ids);
  for (const part of diagram.parts) {
    assert.ok(part.top >= 0 && part.left >= 0, `${part.id} is outside the positive canvas`);
  }
  assert.equal(diagram.connections.length, 25);
  for (const row of diagram.connections) {
    assert.ok(Array.isArray(row), 'each Wokwi wire must be an array');
    assert.equal(row.length, 4);
    for (const endpoint of row.slice(0, 2)) {
      const partId = endpoint.split(':')[0];
      assert.ok(partId === '$serialMonitor' || idSet.has(partId), `unknown connected part ${partId}`);
    }
  }
  for (const [gpio, pin] of [[25, 'IR1'], [26, 'IR2'], [32, 'PZ1'], [33, 'PZ2'], [34, 'PZ3'], [35, 'PZ4']]) {
    assert.ok(diagram.connections.some(row => row[0] === `esp:${gpio}` && row[1] === `shotTest:${pin}`),
      `wrong or missing ESP GPIO ${gpio} -> ${pin}`);
  }
  assert.ok(diagram.connections.some(row => row[0] === 'esp:VP' && row[1] === 'fsr1:OUT'));
});

test('local FSR WASM initializes and changes analog output with pressure control', () => {
  const memory = new WebAssembly.Memory({ initial: 16 });
  const attrs = [], pins = [], timers = [];
  let now = 0, instance;
  const u32 = p => new DataView(memory.buffer).getUint32(p, true);
  const readString = p => {
    const bytes = new Uint8Array(memory.buffer);
    let end = p;
    while (bytes[end]) end++;
    return Buffer.from(bytes.slice(p, end)).toString();
  };
  const env = {
    memory,
    attrInit(p, value) { attrs.push({ name: readString(p), value }); return attrs.length - 1; },
    attrReadFloat: id => attrs[id].value,
    pinInit(p, mode) { assert.equal(mode, 4); pins.push({ name: readString(p), value: 0 }); return pins.length - 1; },
    pinDACWrite(id, value) { pins[id].value = value; return value; },
    timerInit(p) { timers.push({ user: u32(p), callback: u32(p + 4), due: Infinity }); return timers.length - 1; },
    timerStart(id, delay, repeat) { Object.assign(timers[id], { due: now + delay, delay, repeat }); },
  };
  const wasm = path.join(projectRoot, 'chips/fsr-sensor.chip.wasm');
  instance = new WebAssembly.Instance(new WebAssembly.Module(fs.readFileSync(wasm)), { env });
  instance.exports.chipInit();
  assert.equal(pins[0].name, 'OUT');
  attrs.find(attr => attr.name === 'pressure').value = 60;
  now = 1000;
  const timer = timers[0];
  instance.exports.__indirect_function_table.get(timer.callback)(timer.user);
  assert.ok(pins[0].value > 0 && pins[0].value < 3.3);
});
