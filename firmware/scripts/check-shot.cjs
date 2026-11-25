const assert = require('node:assert/strict');
const fs = require('node:fs');

function checkShot(serial, zone) {
  assert.ok(['upper_left', 'upper_right', 'lower_left', 'lower_right', 'none'].includes(zone));
  const messages = serial.split(/\r?\n/).filter(line => line.trim().startsWith('{')).map(JSON.parse);
  assert.equal(messages.filter(m => m.level === 'error' || m.level === 'warning').length, 0,
    'unexpected firmware error/warning');
  const shots = messages.filter(m => m.event_type === 'shot');
  assert.equal(shots.length, 1, 'expected exactly one JSON shot');
  const shot = shots[0];
  assert.equal(shot.target_zone, zone);
  assert.equal(typeof shot.ball_speed_kmh, 'number');
  assert.ok(Math.abs(shot.ball_speed_kmh - 72) <= 0.5, 'speed must be close to 72 km/h');
  assert.ok(Number.isInteger(shot.impact_intensity));
  assert.ok(shot.impact_intensity <= 4095);
  if (zone === 'none') assert.equal(shot.impact_intensity, 0);
  else assert.ok(shot.impact_intensity > 0, 'impact intensity must be positive');
  return shot;
}

module.exports = { checkShot };
if (require.main === module) {
  try {
    const [, , log, zone] = process.argv;
    if (!log || !zone) throw new Error('Usage: node scripts/check-shot.cjs serial.log upper_left');
    console.log('PASS', checkShot(fs.readFileSync(log, 'utf8'), zone));
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
