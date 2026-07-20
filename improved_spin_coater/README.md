# SDSemi Spin Coater Controller

Arduino firmware for the Semiconductor Club spin coater. Controls a DC motor at closed-loop RPM targets defined by a user-editable multi-step spin profile, with an OLED menu UI driven by a rotary encoder.

---

## Hardware

| Component | Part | Connection |
|---|---|---|
| MCU | Arduino Uno R4 | — |
| Motor driver | XY160D | IN1=6, IN2=7, EN=5 |
| Encoder | Modulino I2C knob | Wire1, addr 0x3A |
| OLED | SSD1306 128×64 | Wire1, addr 0x3D |
| RPM sensor | Hall effect (1 magnet/rev) | pin 2 (interrupt) |
| Vibration sensor | GY-521 (MPU-6050) | Wire (default I2C), addr 0x68 (AD0→GND) |

---

## File Overview

| File | Purpose |
|---|---|
| `improved_spin_coater.ino` | Main sketch, app state machine, hardware init, loop |
| `OLEDLineDisplay` | SSD1306 line/list display driver with dirty tracking |
| `ModulinoKnob` | I2C rotary encoder — cumulative value + button state |
| `MenuUI` | Hierarchical menu with navigation stack |
| `SpinProfilePage` | Digit-by-digit spin profile editor (3-state FSM) |
| `SpinProfile` | `SpinStep` data, global profile array, `SpinRunner` |
| `RPMController` | PID + feedforward closed-loop RPM controller |
| `HallSensorRPM` | Interrupt-driven RPM measurement from Hall sensor |
| `XY160D` | DC motor driver (Forward / Backward / Brake) |
| `MotorMap` | PWM→RPM lookup table, SD-card-backed with hardcoded default |
| `SdLogger` | Driver for the Ethernet shield's on-board SD slot (shared SPI bus with the W5100) |
| `MotorCalibrator` | Sweeps PWM values and records average RPM at each step |
| `Mpu6050` | MPU-6050 driver + rolling vibration-window RMS/peak/crest-factor stats |

---

## App State Machine

```
APP_MENU          — root menu navigation via MenuUI
APP_EDIT_PROFILE  — digit-by-digit profile editor via SpinProfilePage
APP_SPIN          — closed-loop spin run via SpinRunner + RPMController
APP_CALIBRATE     — motor calibration sweep via MotorCalibrator
```

State transitions are triggered by deferred flags (`gDoStartSpin`, `gDoEditProfile`, `gDoCalibrate`) set inside menu callbacks and acted on at the top of the main loop after `menu.update()` returns. This avoids re-entrant state changes from inside a callback.

When spin or calibrate finishes, `menu.resetToRoot()` is called so the OLED returns to the root menu rather than the confirmation submenu.

---

## UI Navigation

### Rotary Encoder (ModulinoKnob)
- `update()` polls the I2C device each loop.
- `value()` returns a cumulative signed integer — the main loop diffs it against `prevKnobValue` to get a delta.
- `pressed()` returns current button state — the main loop detects a rising edge for single-fire presses.

### Main Menu (MenuUI)
A depth-5 navigation stack. Each level stores its item list, count, selected index, and scroll offset. When entering a submenu, a "< Back" item is automatically prepended at depth > 0. Pressing it calls `pop()`.

Menu tree:
```
Start Spin  →  [confirm submenu]  →  Confirm  (→ APP_SPIN)
Edit                                           (→ APP_EDIT_PROFILE)
Calibrate   →  [confirm submenu]  →  Confirm  (→ APP_CALIBRATE)
```

### OLED List Rendering (OLEDLineDisplay — list mode)
- `setList(items, count)` copies item strings into internal storage.
- `setListSelected(index)` / `setListOffset(offset)` update highlight and scroll position.
- `renderList()` redraws the full list only when `_listDirty` is true, then clears the flag.
- Selected item is rendered inverted (black text on white fill). A 2-px scrollbar appears on the right when the list is taller than the display.

### OLED Line Rendering (OLEDLineDisplay — line mode)
- Up to 4 lines (configurable at construction). Text size auto-selected to fit line height.
- `setText(line, fmt, ...)` uses `vsnprintf` into a fixed char buffer. Marks the line dirty only if content changed (strcmp).
- `render()` redraws only dirty lines, then calls `disp.display()` only if at least one line was redrawn — but `display()` still flushes the *entire* framebuffer regardless of which lines changed (see I2C Bus Map above), so "only dirty lines" only reduces how often a redraw is *triggered*, not its cost once triggered.
- Text is stored as `char[32]` — no heap allocation.
- `updateOledSpin()` in the main sketch throttles its own calls to `render()` to once per 150 ms, on top of the dirty-line check above — during `PHASE_RAMP` the displayed target RPM changes every control tick, which would otherwise trigger a full-buffer flush almost continuously and was the main visible source of UI lag during a ramp.

### Spin Screen Layout (`updateOledSpin`)
The OLED is 128px wide, 4 lines tall (64px / 4 = 16px/line), which makes `OLEDLineDisplay`'s constructor auto-select text size 2x (16px glyphs — the largest size that still fits the line height exactly). **At that size `setText()` caps every line at `128 / (6*2) = 10` characters** — anything longer is silently truncated, so labels have to stay short:

```
Ph:Final     <- current phase (Idle / Ramp / Final)
Left:42s     <- seconds remaining in the current phase
Tgt:4000     <- target RPM for the current phase
Act:3987     <- measured RPM (Hall sensor)
```

All four lines fit the 10-char budget with room to spare even at 4-digit RPM values. This replaced the previous 3-line layout (which had a static "SPINNING" header line and relied on a TM1637 7-segment display, since removed, to show live RPM) — actual RPM now has to live on the OLED since there's no other numeric readout.

**The 10-char budget applies to list mode too, but isn't enforced there:** `setText()` (line mode) truncates via a fixed buffer, but `setList()`/`renderList()` (used by `MenuUI` and the phase-list/field-select screens below) just copies up to 31 characters with no width check — a string longer than ~10 chars simply runs off the physical right edge of the screen instead of being cut cleanly. Every label built by this codebase needs to be sized with that in mind; see the Spin Profile Editor section below for how the phase-list labels are kept within it at every possible entered value, not just typical ones.

---

## Spin Profile Editor (SpinProfilePage)

Three internal states:

```
STEP_LIST    — scrollable list of steps + Add/Remove controls
FIELD_SELECT — choose RPM or Duration for the selected step
DIGIT_EDIT   — cycle through digits one at a time with encoder
```

**Digit editing:** digits are split into a `_digits[]` array. Encoder scroll increments/decrements the current digit (wraps 0–9). Each button press advances to the next digit. On the final digit, pressing saves back to `spinProfile[]` and returns to field select.

**Digit cursor:** `renderDigitEdit()` shows the assembled value on OLED line 1 (e.g. `RPM:4000` or `Sec:060`) and an arrow-cursor line below it (`    ^`, at column `4 + digitPos`) pointing at the digit currently being edited. This is the only indicator of edit position now — it used to be paired with a TM1637 digit blinking, since removed. The header line above it reads e.g. `"Final Spd"` — `"Speed"` is abbreviated to `"Spd"` specifically because `"Final Speed"` is 11 characters and would otherwise lose its last letter.

**Phase-list and field-select labels are sized for the worst case, not the common one:** since list mode doesn't clip to the display width (see above), `enterPhaseList()` builds labels like `"I %u/%u"` / `"F %u/%u"` (e.g. `"I 500/10"`) instead of the more readable `"Idle:%u/%us"` used before — the old format hit 14 characters at typical values and was already running off-screen. The compact format stays within 10 characters even at the extreme end of what the digit editor allows (4-digit RPM, 3-digit seconds: `"I 9999/999"` is exactly 10). Same reasoning for `fieldLabel()`'s `"Speed:%u"` / `"Time:%us"` (tight spacing, no padding) in field-select.

**Profile storage:** up to 6 `SpinStep` entries (`rpm: uint16`, `durationS: uint16`). Default is `{500 RPM, 30 s}` and `{4000 RPM, 60 s}`. Stored in a global array in RAM (not EEPROM — resets on power cycle unless save is added).

---

## Closed-Loop RPM Control (RPMController)

Algorithm per `update(targetRPM, measuredRPM)` call:

1. **Feedforward:** linear interpolation from the motor map gives the open-loop PWM estimate, scaled to 93% to leave room for the integrator.
2. **Error + deadband:** errors within ±15 RPM are treated as zero to prevent dither.
3. **Conditional integration:** integral accumulates only when error < 500 RPM to limit windup during large transients.
4. **Anti-windup clamp:** integral clamped to ±5000.
5. **Derivative:** rate of change of error for damping.
6. **Asymmetric Kp:** proportional gain is doubled when error is negative (motor overshooting), for faster correction.
7. **Overshoot braking:** additional penalty subtracted from output when measured RPM exceeds target (×0.25 soft, ×0.30 hard ceiling at 110% of target).
8. **Output clamp:** 0–255 PWM.
9. **Asymmetric ramp:** output changes by at most +4 PWM/update (soft ramp up) and −12 PWM/update (fast ramp down).

Tuned values (set in `setup()`): Kp=0.035, Ki=0.0010, Kd=0.08, rampRate=4, deadband=15.

**Fixed control-loop cadence:** the integral and derivative terms above are computed per *call*, not per second — `_integral += error` and `derivative = error - _lastError` have no `dt` in them. That only means what the tuned gains assume if `update()` runs at a steady rate. Since `loop()`'s actual period varies with how much OLED/MQTT/I2C work happens around it, `APP_SPIN` in the main sketch pins `spinRunner.update()` (which calls into this controller) to a fixed tick — `SPIN_CONTROL_INTERVAL_MS` (20 ms / 50 Hz) — instead of calling it on every raw `loop()` pass. This keeps Ki/Kd behaving consistently regardless of display/network jitter. If you ever change `SPIN_CONTROL_INTERVAL_MS`, the tuned gains above will need re-tuning, since their real-world effect scales with how often `update()` actually runs.

---

## Hall Sensor RPM (HallSensorRPM)

- Attaches a falling-edge interrupt to the hall sensor pin.
- ISR records `micros()` of each pulse and computes `_period = now - _lastPulseTime`.
- 500 µs debounce: pulses arriving faster than that are ignored.
- `getRPM()` reads `_period` atomically (noInterrupts/interrupts), returns 0 if no pulse in the last 500 ms or period is 0. Otherwise: `RPM = 60_000_000 / (period × magnetsPerRev)`.
- Configured for 1 magnet/rev (`HallSensorRPM sensor(2, 4)` — pin 2, 4 magnets arg... check actual wiring; constructor second arg is magnets per rev).

---

## Motor Calibration (MotorCalibrator)

Sweeps PWM from 30 to 255 in steps of 5. At each step:
1. **SETTLING (1500 ms):** applies PWM, waits for speed to stabilize.
2. **SAMPLING (500 ms):** accumulates RPM readings, computes average.
3. Stores `{pwm, avgRPM}` into a temporary map array.

After all steps, prints the full map to Serial in copy-pasteable C array format (for baking a new hardcoded `defaultMap[]` if you ever want to) and, exactly once — right after the sweep completes, never per-point and never on an aborted sweep — calls `MotorMap_setActive()` + `MotorMap_save()` to write the new map to `MOTORMAP.CSV` on the SD card. There is no separate opt-in flag; a finished calibration always overwrites the saved map.

**OLED screen (`updateOledCal`):** same 10-char-per-line budget as the spin screen (see above) — the previous `"CALIBRATING"` (11 chars) and `"Please wait"` (11 chars) each silently lost their last letter to that cap. Replaced with:

```
Calibrate    <- header (9 chars)
45%          <- sweep progress (MotorCalibrator_progress())
RPM:2312     <- live measured RPM at the current PWM step
PWM:185      <- PWM value currently being tested (MotorCalibrator_currentPWM())
```

---

## Motor Map (MotorMap)

- 46-point default PWM→RPM table hardcoded in flash (`defaultMap[]` in `MotorMap.cpp`) — used only as a fallback, see below.
- On `MotorMap_init()`: tries to load `MOTORMAP.CSV` from the SD card via `SdLogger` (see below). Falls back to `defaultMap[]` if the card is missing/unformatted, the file doesn't exist, or it has no parseable lines.
- `MotorMap_save()` rewrites `MOTORMAP.CSV` from the current in-RAM map (`SD.remove()` then re-append line by line — the SD library's `FILE_WRITE` only appends, so the old file has to be cleared first or a shorter new map would leave stale trailing points from a longer previous one).
- `MotorMap_setActive(points, count)` replaces the in-RAM map (used by `MotorCalibrator` to install a finished sweep) — it does not touch the SD card itself, call `MotorMap_save()` after.
- `RPMController` uses this table for feedforward interpolation (smoothstep between points). Loading a new SD-saved map only takes effect after the next `MotorMap_init()` (boot/reset) since `rpmController.begin()` is only called once in `setup()`.

**SD card file format (`MOTORMAP.CSV`, in the root directory):** one `pwm,rpm` line per point, no header row, e.g.:
```
30,531.00
35,924.00
...
```
Filename is kept 8.3-safe (`MOTORMAP` = 8 chars, `.CSV` = 3) per the note in `SdLogger.h`.

This replaced an earlier EEPROM-backed version (magic word `0xBEEF` + point array at address 0) — moved to SD once the Ethernet shield's SD slot was verified working (`ethernet_sd_test/`, a sibling project), mainly so a calibration run's results are a plain-text file you can pull off the card and inspect/edit directly, rather than opaque EEPROM bytes.

---

## SD Card (SdLogger)

Thin driver around the Arduino SD library for the Ethernet shield's on-board SD slot (`SdLogger.h`/`.cpp`), currently used only by `MotorMap`. `begin()` drives both the SD and Ethernet (W5100) CS pins HIGH before calling `SD.begin()`, since the two chips share MOSI/MISO/SCK and leaving the other one un-deselected corrupts transfers silently — see the header comment in `SdLogger.h` for the full rationale. `appendLine()`/`printFile()` block for a few ms per call (card-dependent SD.open()/close() cost), so callers should keep them off any hot loop — `MotorMap` only calls into this during `MotorMap_init()` (once, at boot) and `MotorMap_save()` (once, right after a calibration sweep finishes), never during a spin.

---

## Spin Profile (3-Phase)

The profile is a fixed `SpinPhaseProfile` struct in `SpinProfile.h` with five fields:

| Field | Description |
|---|---|
| `idleSpeed` | RPM during idle phase |
| `idleTime` | seconds in idle phase |
| `rampTime` | seconds to linearly ramp from `idleSpeed` to `finalSpeed` |
| `finalSpeed` | RPM during final spin |
| `finalTime` | seconds in final spin phase |

`SpinRunner` sequences through `PHASE_IDLE → PHASE_RAMP → PHASE_FINAL`. During the ramp, `targetRPM` is linearly interpolated each loop tick. `phaseName(int)` returns `"Idle"`, `"Ramp"`, or `"Final"` for display.

**Phase transition note:** `_ctrl->reset()` is called at spin start and end but calls `resetSoft()` at intermediate boundaries to preserve `_lastPWM` and avoid speed dips. If you add phases, follow the same pattern.

---

## XY160D Motor Driver

Three-wire interface: IN1, IN2 (direction), EN (PWM speed).

| Method | IN1 | IN2 | EN |
|---|---|---|---|
| `Forward(speed)` | HIGH | LOW | speed (0–255) |
| `Backward(speed)` | LOW | HIGH | speed (0–255) |
| `Brake()` | LOW | LOW | 0 |

---

## SpinRunner

Iterates through `spinProfile[]` steps sequentially.
- `start()` resets the RPM controller and begins at phase 0 (Idle).
- `update(rpm)` computes the interpolated target, calls `rpmController.update(target, rpm)`, and drives the motor each loop. Returns `false` when all phases are complete (motor braked, controller reset).
- `phaseRemainingS()` returns seconds left in the current phase.
- `lastTargetRPM()` returns the most recently computed target (used for OLED display during ramp).

---

## I2C Bus Map

The Uno R4 Wifi exposes two I2C peripherals. Keep this in mind when adding sensors.

| Bus | Devices | Addresses |
|---|---|---|
| `Wire1` | Modulino Knob, SSD1306 OLED | 0x3A, 0x3D |
| `Wire` (Wire0) | MPU-6050 vibration sensor | 0x68 |

New sensors should use `Wire` to avoid conflicts.

Both buses are set to Fast Mode (`setClock(400000)` in `setup()`, right after each `begin()`) instead of the default 100 kHz. This matters more than it sounds: `Adafruit_SSD1306::display()` always flushes the *entire* 1KB framebuffer over I2C (no partial update), so at 100 kHz a single redraw was ~90 ms — and if the OLED content changes often (e.g. every ~ms during a spin ramp, see below), that turns into a near-constant full-buffer transfer and reads as UI lag. Fast Mode alone cuts that to ~25 ms; throttling how often the OLED actually redraws (below) is the other half of the fix.

---

## Telemetry Upload (MPU6050 + Spin/Calibrate metrics -> MQTT)

The `Mpu6050` driver (`Mpu6050.h`/`.cpp`) reads the GY-521 (MPU-6050) over `Wire` and, together with the current spin-profile/calibration state, republishes the Ethernet/MQTT link that was previously used to upload a placeholder counter to Ignition — the network/broker config in `config.h` is unchanged, only the payload is now real data.

**Setup:** `imu.begin()` wakes the sensor and applies range/DLPF settings, then `calibrateGyro(500)` and `calibrateGravity(500)` average 500 stationary samples each (~3 s total, blocking) to find the gyro zero-offset and the static gravity vector. Keep the coater still while these run. `setVibrationWindow()` configures the accel poll rate (`MPU_SAMPLE_INTERVAL_MS`, 20 ms) and the stats window length (`MPU_PUBLISH_INTERVAL_MS`, 1000 ms).

`MPU_SAMPLE_INTERVAL_MS` is deliberately not pushed faster than it needs to be: each poll is a 6-byte I2C read, and there's no dedicated timer for it — it only runs as often as `loop()` happens to call `updateVibration()`, so the achievable rate is capped by everything else sharing that loop (knob poll, MQTT, motor control). At 20 ms (50 Hz) that's comfortably below the loop's ceiling, so `sampleCount` stays stable even under transient load, at the cost of some sample density — acceptable since the sensor's DLPF is already band-limited to 44 Hz (`MPU6050_DLPF_44HZ`), well under the ~50 samples/window this still yields.

**When it publishes:** telemetry is only sent while `APP_SPIN` or `APP_CALIBRATE` is active — not from the menu or profile editor. `imu.beginVibrationWindow()` is reset at the moment each of those states is entered (in the `gDoStartSpin`/`gDoCalibrate` handlers) so a stale window left over from menu idle time can't be flushed as a bogus first sample.

**MQTT is never allowed to block the coater:** `maintainMqtt()` (called once per `loop()`) makes at most one `mqttClient.connect()` attempt per `MQTT_RETRY_INTERVAL_MS` (5 s) and always returns immediately, connected or not — it replaced an earlier version that looped with `delay(5000)` until the broker answered, which froze the knob, motor, and spin/calibrate state machine indefinitely if the broker was ever unreachable. `mqttClient.publish()` in `publishTelemetry()` is a no-op (returns `false`) when disconnected, so a down broker just means missed telemetry, not a stuck coater.

That connect attempt itself can still block, though: PubSubClient's default socket timeout is 15 s while it waits for a TCP handshake that's never coming, which read as a "huge lag spike" once per retry interval. `setup()` calls `mqttClient.setSocketTimeout(2)` to cap that at 2 s, and `maintainMqtt()` skips the attempt entirely (no blocking call at all) when `Ethernet.linkStatus() == LinkOFF` — the common case of the cable just being unplugged.

**Gives up after 30 s:** if Ignition hasn't answered within `MQTT_GIVE_UP_MS` (30 s) of the first `loop()` after boot, `maintainMqtt()` stops attempting reconnects for the rest of the session — a coater run shouldn't keep paying a connect attempt every `MQTT_RETRY_INTERVAL_MS` indefinitely just because the broker is down or unreachable that day. This only affects live telemetry; motor-map calibration data is SD-card-backed (see `MotorMap`) and has no MQTT dependency. There's no automatic re-arm — reconnecting requires a reset/reboot.

**Loop:** `publishTelemetry()` is called once per iteration from inside the `APP_SPIN` and `APP_CALIBRATE` switch cases. It calls `imu.updateVibration(stats)`, which polls accel at the configured sample interval (non-blocking, `millis()`-based) and folds each reading — after subtracting the calibrated gravity vector — into the current window's RMS/peak accumulators. Once a full window elapses it finalizes the stats, resets the window, and returns `true`, at which point the payload is built and published:

| Field | Meaning |
|---|---|
| `phase` | `"Idle"` / `"Ramp"` / `"Final"` during a spin (from `phaseName()`), or `"Calibrate"` during a calibration sweep |
| `targetSpeed` | Target RPM for the current spin phase (`spinRunner.lastTargetRPM()`); `0` during calibration, which drives PWM open-loop and has no RPM target |
| `actualSpeed` | Measured RPM (Hall sensor) at publish time |
| `time` | Seconds elapsed since the run started — `spinRunner.elapsedS()` (since `start()`, spans all phases) during a spin, `MotorCalibrator_elapsedS()` (since `MotorCalibrator_start()`) during calibration |
| `rmsAccelG` | RMS of the gravity-removed acceleration magnitude, in g |
| `peakAccelG` | Largest instantaneous gravity-removed magnitude seen in the window, in g |
| `crestFactor` | `peakAccelG / rmsAccelG` — impulsive vibration reads high, smooth/periodic vibration reads near `sqrt(2)` |
| `sampleCount` | Number of accel samples folded into the window |

Float fields are written via `addFloatField()` / `MPU6050::formatFixed()` as fixed-decimal strings rather than raw floats, so the payload always contains a decimal point — otherwise Ignition's MQTT/JSON tag-creation can infer an Int tag from a whole-number float in the first message and reject or truncate every later fractional value.

---

## Pin Map

| Pin | Function | Notes |
|---|---|---|
| 2 | Hall sensor interrupt | falling edge, 500 µs debounce |
| 4 | SD card CS | Ethernet shield's on-board SD slot, used by `MotorMap`/`SdLogger` |
| 5 | XY160D EN (PWM) | analogWrite speed |
| 6 | XY160D IN1 | direction |
| 7 | XY160D IN2 | direction |
| 10 | Ethernet (W5100) CS | shield default, used by `Ethernet.begin()` |
| SDA/SCL (Wire1) | OLED + Knob | Qwiic header |
| SDA/SCL (Wire) | MPU-6050 | standard I2C header |


---

## Adding a New App State

Follow this pattern to add any new operating mode (vibration monitor, data logging, etc.):

1. Add an enum value to `AppState` in `improved_spin_coater.ino`.
2. Add a `bool gDoX = false` deferred flag and a menu callback that sets it.
3. In `loop()`, check `gDoX` after `menu.update()` returns, set up hardware, set `appState = APP_X`.
4. Add a `case APP_X:` to the switch. Call `menu.resetToRoot()` when done.

Never change `appState` directly inside a menu callback — the deferred flag pattern prevents re-entrant state changes.
