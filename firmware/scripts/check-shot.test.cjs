const { test } = require('node:test');
const assert = require('node:assert/strict');
const { checkShot } = require('./check-shot.cjs');
const shot = { event_type: 'shot', ball_speed_kmh: 72, target_zone: 'upper_left', impact_intensity: 3500 };
const line = changes => JSON.stringify({ ...shot, ...changes });

test('accepts positive intensity in each zone and explicit no-impact', () => {
  for (const zone of ['upper_left', 'upper_right', 'lower_left', 'lower_right']) {
    checkShot('[BOOT]\n[SHOT] log\n' + line({ target_zone: zone }), zone);
  }
  checkShot(line({ target_zone: 'none', impact_intensity: 0 }), 'none');
});
test('rejects absent/duplicate shots, wrong zone, zero intensity and invalid speed', () => {
  for (const log of ['', line({}) + '\n' + line({}), line({ target_zone: 'none' }),
    line({ impact_intensity: 0 }), line({ ball_speed_kmh: 0 }), line({ impact_intensity: -1 })]) {
    assert.throws(() => checkShot(log, 'upper_left'));
  }
});
test('rejects malformed JSON and firmware errors', () => {
  assert.throws(() => checkShot('{broken', 'upper_left'));
  assert.throws(() => checkShot(line({}) + '\n' + JSON.stringify({ level: 'error' }), 'upper_left'));
});
