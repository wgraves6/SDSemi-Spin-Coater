# SDSemi Spin Coater Controller

Arduino firmware for the Semiconductor Club spin coater. Controls a DC motor at closed-loop RPM targets using a Hall effect sensor, with an OLED menu UI driven by a rotary encoder and a TM1637 7-segment display for numeric readout.

## Hardware

| Component | Detail |
|---|---|
| MCU | Arduino Giga R1 (or compatible) |
| Motor driver | XY160D (PWM + direction) |
| Encoder | Modulino I2C rotary knob |
| OLED | SSD1306 128x64, I2C |
| 7-segment | TM1637 4-digit |
| RPM sensor | Hall effect, single magnet, interrupt-driven |
| Storage | EEPROM (motor map + spin profile) |

## File Overview

| File | Purpose |
|---|---|
| `improved_spin_coater.ino` | Main sketch, app state machine, hardware init |
| `OLEDLineDisplay` | SSD1306 line/list display driver with dirty tracking |
| `TM1637BlinkerDigit` | TM1637 driver with per-digit blinking |
| `ModulinoKnob` | I2C rotary encoder (delta + button) |
| `MenuUI` | Hierarchical menu with navigation stack |
| `SpinProfilePage` | Digit-by-digit spin profile editor |
| `SpinProfile` | SpinStep data, global profile array, SpinRunner |
| `RPMController` | PID + feedforward closed-loop RPM controller |
| `HallSensorRPM` | Interrupt-driven RPM measurement |
| `XY160D` | DC motor driver (forward/brake/reverse) |
| `MotorMap` | PWM→RPM lookup table, EEPROM-backed |
| `MotorCalibrator` | Sweep PWM values and record RPM for calibration |

## App States

```
APP_MENU        — main menu navigation (MenuUI)
APP_EDIT_PROFILE — digit-by-digit spin profile editor (SpinProfilePage)
APP_SPIN        — closed-loop spin run (SpinRunner + RPMController)
APP_CALIBRATE   — motor calibration sweep (MotorCalibrator)
```

## UI

- Rotary encoder: scroll (delta) / select (press)
- OLED shows scrollable lists with highlight + scrollbar; digit-edit mode during profile editing
- TM1637 shows live RPM during spin, calibration % during calibration, and blinks the active digit during profile editing

## Spin Profile

Up to 6 steps, each with a target RPM and duration in seconds. Persisted to EEPROM. Edited via the "Edit" menu item — step list → field select → digit-by-digit entry.

## Motor Map

46-point PWM→RPM table (default hardcoded, overwritten by calibration). Stored in EEPROM starting at address 0. Used by `RPMController` for feedforward.
