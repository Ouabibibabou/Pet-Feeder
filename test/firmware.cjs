// Run compiled Uno firmware; the fixtures emulate peripherals, not controller logic.
'use strict';
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const { join } = require('node:path');
const { homedir } = require('node:os');
const { execFileSync } = require('node:child_process');
const avr = require('avr8js');

const env = process.argv[2] || 'uno';
assert.ok(['uno', 'sim_hopper', 'sim_bowl'].includes(env), 'unknown fixture');
const build = join(__dirname, '../.pio/build', env);
const core = process.env.PLATFORMIO_CORE_DIR || join(homedir(), '.platformio');
const nm = join(core, 'packages/toolchain-atmelavr/bin/avr-nm');
const symbols = execFileSync(nm, ['firmware.elf'], { cwd: build, encoding: 'utf8' });
const millisAddr = parseInt(symbols.match(/^(\w+) [bB] timer0_millis$/m)[1], 16) & 0xffff;
const image = new Uint8Array(32768);
for (const line of readFileSync(join(build, 'firmware.hex'), 'utf8').trim().split(/\r?\n/)) {
  const rec = Buffer.from(line.slice(1), 'hex');
  if (rec[3] === 0) image.set(rec.subarray(4, 4 + rec[0]), rec.readUInt16BE(1));
}
const cpu = new avr.CPU(new Uint16Array(image.buffer), 2048);
const cyclesPerMs = 16000;
new avr.AVRTimer(cpu, avr.timer0Config);
new avr.AVRTimer(cpu, avr.timer1Config);
const b = new avr.AVRIOPort(cpu, avr.portBConfig);
const d = new avr.AVRIOPort(cpu, avr.portDConfig);
const twi = new avr.AVRTWI(cpu, avr.twiConfig, cyclesPerMs * 1000);
let stall = false, firstStall, near = false, sckEdges = 0, triggers = 0;
let word = -1, bit = 0, stuckLow = false;
let expander = 0, nibble = null, lcdText = '';
const time = () => cpu.cycles / cyclesPerMs;
// A stalled START never completes; the firmware must leave through Wire's timeout.
twi.eventHandler = {
  start() { if (!stall) twi.completeStart(); else firstStall ??= time(); },
  stop() { twi.completeStop(); }, connectToSlave(addr) { twi.completeConnect(addr === 0x27); },
  writeByte(value) {
    // PCF8574 -> HD44780: data nibbles latch on E's falling edge with RS high.
    if ((expander & 4) && !(value & 4) && (value & 1)) {
      if (nibble === null) nibble = value & 0xf0;
      else { lcdText += String.fromCharCode(nibble | (value >> 4)); nibble = null; }
    }
    expander = value; twi.completeWrite(true);
  }, readByte() { twi.completeRead(0xff); }
};
const pulses = [];
let raised = 0;
b.addListener((value, old) => {
  if ((value ^ old) & 2) { // Uno D9 = PB1
    if (value & 2) raised = time(); else pulses.push({ at: raised, width: time() - raised });
  }
});
d.addListener((value, old) => {
  if ((value & ~old) & 0x20) {
    ++sckEdges;
    ++bit;
    d.setPin(4, !stuckLow && (bit > 24 || !!((word >>> (24 - bit)) & 1)));
  }
  if ((old & ~value) & 0x40) ++triggers;
  if (near && ((old & ~value) & 0x40)) { // Echo for an object ~8 cm away.
    cpu.addClockEvent(() => d.setPin(7, true), 1600);
    cpu.addClockEvent(() => d.setPin(7, false), 1600 + 464 * 16);
  }
});
// Advance real emulated CPU cycles; only jump the software clock where explicitly stated.
function run(ms) {
  const end = cpu.cycles + ms * cyclesPerMs;
  while (cpu.cycles < end) { avr.avrInstruction(cpu); cpu.tick(); }
}
function jumpMillis(ms) {
  cpu.dataView.setUint32(millisAddr, cpu.dataView.getUint32(millisAddr, true) + ms, true);
}
function press(pin) { d.setPin(pin, false); run(55); d.setPin(pin, true); run(55); }
function opensAfter(at) { return pulses.filter(p => p.at > at && p.width > 1.2); }
d.setPin(2, true); d.setPin(3, true); d.setPin(4, true);
run(1800);
if (env !== 'uno') {
  testScaleFrames();
  testScaleRails();
  testPortionLimits();
  return;
}
// Start a second press just before closing finishes; even a short release bounce must not arm it.
press(3); run(2310);
let boundary = time();
d.setPin(3, false); run(15); d.setPin(3, true); run(4); d.setPin(3, false); run(100);
assert.equal(opensAfter(boundary).length, 0, 'a busy press must not cross the closing boundary');
d.setPin(3, true); run(55); boundary = time(); press(3);
assert.ok(opensAfter(boundary).length, 'a fresh press after stable release must work');
run(2450);
console.log('PASS: busy press suppressed across closing, fresh press accepted');
const initialEdges = sckEdges;
d.setPin(4, false); run(40); // Ready briefly, then held high from the first clock edge.
assert.equal(sckEdges - initialEdges, 25, 'one bounded A/128 read, no second ready wait');
assert.match(lcdText, /Scale: --/);
assert.doesNotMatch(lcdText, /Food:|add more food/, 'unconfigured calibration never displays grams or low stock');
let marker = time();
press(3); run(150);
assert.ok(opensAfter(marker).length, 'manual press must open with HX711 missing');
// A falsely ready DOUT during motion must not start a clock burst.
let edges = sckEdges;
d.setPin(4, false); run(80); d.setPin(4, true);
assert.equal(sckEdges, edges, 'no conversions during gate movement');
stall = true;
marker = time();
press(2); run(600);
assert.ok(firstStall >= marker + 420 && firstStall < marker + 500, 'defer OFF write through closing travel');
assert.ok(pulses.some(p => p.at > marker + 55 && p.width < 0.9), 'command close before detach');
assert.ok(pulses.at(-1).at <= firstStall, 'no pulses after standby LCD access');
marker = time();
press(2); run(450); press(3); run(2200);
assert.ok(opensAfter(marker).length, 'buttons and gate must still work with stalled LCD');
console.log('PASS: missing HX711, motion sampling guard, standby detach, stalled LCD and recovery');

// Jump into the breakfast window, then emulate a scheduling gap between near readings.
jumpMillis(7 * 60 * 60000); near = true; run(600);
marker = time(); jumpMillis(5000); run(3750);
assert.equal(opensAfter(marker).length, 0, 'a long unsampled gap must restart the near timer');
run(500);
assert.ok(opensAfter(marker).length, 'four seconds of fresh near readings must trigger');
run(2400); marker = time(); run(4500);
assert.equal(opensAfter(marker).length, 0, 'the same time window cannot dispense twice');
console.log('PASS: sampling gap, four-second qualification and once-per-window quota');

// Advance from today's consumed breakfast to tomorrow at 07:59, then cross 08:00.
jumpMillis((24 * 60 + 59) * 60000); marker = time();
const beforeBreakfast = triggers; run(600);
assert.ok(triggers > beforeBreakfast, 'midnight restores the breakfast quota');
assert.equal(opensAfter(marker).length, 0, 'fresh qualification required on the new day');
jumpMillis(60000); run(100); const atEnd = triggers; run(4500);
assert.equal(triggers, atEnd, 'do not ping after 08:00');
assert.equal(opensAfter(marker).length, 0, 'do not finish a pending qualification outside the slot');
jumpMillis(23 * 60 * 60000); marker = time(); run(4600);
assert.ok(opensAfter(marker).length, 'next breakfast can dispense again');
console.log('PASS: midnight quota reset and exclusive breakfast end');

// Present one signed 24-bit conversion per 100 ms, with DOUT released on pulse 25.
function samples(raw, n) {
  for (let i = 0; i < n; ++i) {
    word = raw; bit = 0; d.setPin(4, false); run(100);
  }
}

function testScaleFrames() {
  samples(180, 7);
  assert.match(lcdText, /Food: 110g/);
  const start = time();
  press(3);
  assert.ok(opensAfter(start).length, 'baseline captured and original feed started');
  lcdText = ''; stuckLow = true; d.setPin(4, false);
  const initialEdges = sckEdges;
  run(7800); // Includes gate travel and the full five-second verification timeout.
  assert.doesNotMatch(lcdText, /Last:|Check outlet/, 'a broken data line is not a portion or a jam');
  assert.match(lcdText, /Scale: --/);
  assert.match(lcdText, /Unverified/);
  assert.ok(sckEdges > initialEdges && sckEdges - initialEdges <= 25 * 79, 'bounded retries on stuck LOW');
  assert.equal(opensAfter(start + 2100).length, 0, 'verification failure must not reopen the gate');

  stuckLow = false; d.setPin(4, true); run(150); lcdText = '';
  samples(0, 7);
  assert.match(lcdText, /add more food/, 'a valid zero word is a measurement');
  // Use a real zero word in the stable baseline (bowl) or the final window (hopper).
  const hopper = env === 'sim_hopper';
  samples(hopper ? 18 : 0, 7); lcdText = '';
  press(3); run(2450); samples(hopper ? 0 : 18, 15);
  assert.match(lcdText, /Last: 18g/, 'recover and measure in the configured direction');
  console.log(`PASS: ${env}, stuck-low rejection, valid zero, recovery and 18 g portion`);
}

// Exercise both endpoints, swapping baseline/final roles and mass direction by mount.
function testScaleRails() {
  const hopper = env === 'sim_hopper';
  const pre = hopper ? 0x7fffff : -0x800000;
  const post = hopper ? -0x800000 : 0x7fffff;
  const inward = hopper ? -20 : 20;
  samples(pre + inward, 7); lcdText = ''; samples(pre, 1);
  assert.match(lcdText, /Scale: --/, 'one clipped sample invalidates the old window');
  let start = time(); press(3); run(2450); samples(pre + inward, 15);
  assert.ok(opensAfter(start).length, 'invalid baseline must not prevent requested feed');
  assert.match(lcdText, /Unverified/);
  assert.doesNotMatch(lcdText, /Last:|Check outlet/);
  assert.equal(opensAfter(start + 2100).length, 0);

  samples(post - inward, 7); lcdText = ''; start = time(); press(3); run(2450);
  samples(post, 55);
  assert.match(lcdText, /Checking\.\.\./, 'valid pre-feed baseline');
  assert.match(lcdText, /Unverified/, 'persistent clipping must time out');
  assert.doesNotMatch(lcdText, /Last:|Check outlet/);
  assert.equal(opensAfter(start + 2100).length, 0);

  samples(180, 7); lcdText = ''; press(3); run(2450); samples(180 + inward, 15);
  assert.match(lcdText, /Last: 20g/, 'next valid cycle recovers without reset');
  console.log(`PASS: ${env}, clipped baseline and final weight, recovery`);
}

// Use a large valid offset so absolute-weight float rounding cannot hide a threshold error.
function testPortionLimits() {
  const sign = env === 'sim_hopper' ? -1 : 1;
  for (const [delta, result] of [[3, 'Last: 3g'], [2, 'Check outlet'], [200, 'Last: 200g'],
                                 [201, 'Unverified'], [-4, 'Unverified']]) {
    const base = -sign * 8388001;
    samples(base, 7); lcdText = ''; press(3); run(2450); samples(base + sign * delta, 15);
    assert.ok(lcdText.includes(result), `${env} delta ${delta}: ${lcdText}`);
  }
  console.log(`PASS: ${env}, inclusive portion limits, small and reversed changes`);
}
