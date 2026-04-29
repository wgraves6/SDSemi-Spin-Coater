# SDSemi Spin Coater Controller

Arduino firmware for the Semiconductor Club spin coater. Controls a DC motor at closed-loop RPM targets defined by a user-editable multi-step spin profile, with an OLED menu UI driven by a rotary encoder and a TM1637 7-segment display for numeric readout.

---

## Hardware

| Component | Part | Connection |
|---|---|---|
| MCU | Arduino Giga R1 | — |
| Motor driver | XY160D | IN1=6, IN2=7, EN=5 |
| Encoder | Modulino I2C knob | Wire1, addr 0x3A |
| OLED | SSD1306 128×64 | Wire1, addr 0x3D |
| 7-segment | TM1637 4-digit | CLK=9, DIO=10 |
| RPM sensor | Hall effect (1 magnet/rev) | pin 2 (interrupt) |

---

## File Overview

| File | Purpose |
|---|---|
| `improved_spin_coater.ino` | Main sketch, app state machine, hardware init, loop |
| `OLEDLineDisplay` | SSD1306 line/list display driver with dirty tracking |
| `TM1637BlinkerDigit` | TM1637 driver with per-digit blinking |
| `ModulinoKnob` | I2C rotary encoder — cumulative value + button state |
| `MenuUI` | Hierarchical menu with navigation stack |
| `SpinProfilePage` | Digit-by-digit spin profile editor (3-state FSM) |
| `SpinProfile` | `SpinStep` data, global profile array, `SpinRunner` |
| `RPMController` | PID + feedforward closed-loop RPM controller |
| `HallSensorRPM` | Interrupt-driven RPM measurement from Hall sensor |
| `XY160D` | DC motor driver (Forward / Backward / Brake) |
| `MotorMap` | PWM→RPM lookup table, EEPROM-backed with hardcoded default |
| `MotorCalibrator` | Sweeps PWM values and records average RPM at each step |

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
- `render()` redraws only dirty lines, then calls `disp.display()` only if at least one line was redrawn.
- Text is stored as `char[32]` — no heap allocation.

### 7-Segment Display (TM1637BlinkerDigit)
- `setNumber(int)` decomposes into 4 BCD digits and calls `showDigits()` immediately.
- `startBlink(index, delayMs)` enables blinking for one digit by index (0=leftmost).
- `clearBlink()` stops all blinking and refreshes the display.
- `update()` is called every 50 ms from the main loop. It toggles any blinking digits whose timer has elapsed and calls `showDigits()` only when a state actually changed (no-op on quiet loops).
- During spin: shows live RPM. During calibration: shows completion %. During digit edit: shows current value with the active digit blinking.

---

## Spin Profile Editor (SpinProfilePage)

Three internal states:

```
STEP_LIST    — scrollable list of steps + Add/Remove controls
FIELD_SELECT — choose RPM or Duration for the selected step
DIGIT_EDIT   — cycle through digits one at a time with encoder
```

**Digit editing:** digits are split into a `_digits[]` array. Encoder scroll increments/decrements the current digit (wraps 0–9). Each button press advances to the next digit. On the final digit, pressing saves back to `spinProfile[]` and returns to field select.

**TM1637 integration:** during digit edit, `setNumber(val)` shows the current assembled value and `startBlink(pos)` blinks the active digit position. RPM uses positions 0–3 directly. Duration offset by +1 because the 4-digit display has a leading zero padding character at position 0.

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

After all steps, prints the full map to Serial in copy-pasteable C array format. `AUTO_SAVE_TO_EEPROM` is `false` by default — copy the Serial output into `defaultMap[]` in `MotorMap.cpp` to bake new calibration in.

---

## Motor Map (MotorMap)

- 46-point default PWM→RPM table hardcoded in flash (`defaultMap[]` in `MotorMap.cpp`).
- On `MotorMap_init()`: tries to load from EEPROM (magic word `0xBEEF` at address 0). Falls back to default if EEPROM is blank or corrupt.
- `MotorMap_save()` writes header + all points to EEPROM starting at address 0.
- `RPMController` uses this table for feedforward interpolation (smoothstep between points).

**EEPROM layout:**
```
[0x0000]  MotorMapHeader { uint16 magic=0xBEEF, uint16 count }
[0x0004]  PWMRPMPoint[0]  { int pwm, float rpm }  (8 bytes each)
[0x000C]  PWMRPMPoint[1]
...
```

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
- `start()` resets the RPM controller and begins at step 0.
- `update(rpm)` calls `rpmController.update(step.rpm, rpm)` and drives the motor each loop. When a step's duration elapses, advances to the next. Returns `false` when all steps are complete (motor braked, controller reset).
- `stepRemainingS()` returns seconds left in the current step using `millis()`.
