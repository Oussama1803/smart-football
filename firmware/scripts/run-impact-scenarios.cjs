// Run each scenario in a fresh Wokwi simulation and validate complete JSON lines.
const { spawnSync } = require('node:child_process');
const path = require('node:path');
const fs = require('node:fs');
const os = require('node:os');
const { checkShot } = require('./check-shot.cjs');
const cases = [
  ['upper-left', 'upper_left'], ['upper-right', 'upper_right'],
  ['lower-left', 'lower_left'], ['lower-right', 'lower_right'],
  ['no-impact', 'none'], ['simultaneous', 'lower_right'],
];
try {
  for (const [name, zone] of cases) {
    // Scenario progress messages can split a JSON line on stdout. Validate only
    // the dedicated serial stream, stored outside the repository.
    const logDir = fs.mkdtempSync(path.join(os.tmpdir(), 'smart-football-wokwi-'));
    const serialLog = path.join(logDir, 'serial.log');
    try {
      const result = spawnSync('wokwi-cli', ['.', '--scenario', `scenarios/impact-${name}.test.yaml`,
        '--timeout', '3000', '--serial-log-file', serialLog],
        { cwd: path.resolve(__dirname, '..'), encoding: 'utf8', timeout: 120000 });
      if (result.error) throw result.error;
      if (result.status !== 0) throw new Error(`${name}: Wokwi exit ${result.status}\n${result.stderr}\n${result.stdout}`);
      const shot = checkShot(fs.readFileSync(serialLog, 'utf8'), zone);
      console.log(`PASS ${name}: ${shot.ball_speed_kmh} km/h, ${shot.target_zone}, intensity ${shot.impact_intensity}`);
    } finally {
      if (fs.existsSync(serialLog)) fs.unlinkSync(serialLog);
      fs.rmdirSync(logDir);
    }
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
