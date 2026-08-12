# Feature: Additional Turner Motor (4th Relay)

## Overview

Add control for a 4th relay channel to drive a unidirectional DC motor for egg turning, running alongside the existing servo. The motor provides an alternative turning mechanism — ON for half the configured turn duration, OFF for the other half.

## Hardware

| Component | Pin | Active | Notes |
|-----------|-----|--------|-------|
| Turner Motor | D6 (GPIO12) | LOW = ON | 4th relay, one-way rotation only |

- Same relay convention as existing: **active LOW** (`LOW` = motor ON, `HIGH` = motor OFF)
- Motor is unidirectional — no polarity reversal, just ON/OFF timing

## Behavior

- Triggers on the same `EGG_TURN_INTERVAL` schedule as the servo
- One turn cycle = `TURNER_MOTOR_DURATION` milliseconds (new configurable setting)
  - **Phase 1 ("Left")**: first 50% — motor **ON**
  - **Phase 2 ("Right")**: second 50% — motor **OFF**
- Runs **independently** of the servo sweep; both activate on the same schedule but motor does not wait for servo completion
- Respects lockdown stage (day 18+): motor stays OFF during lockdown
- Supports manual override: `?device=motor&mode=off` kills the motor; `mode=auto` restores

## Settings

| Setting | Default | Range | Description |
|---------|---------|-------|-------------|
| `turnerMotorDuration` | 10000 ms | 1000–60000 ms | Total motor turn cycle time (phase 1 + phase 2) |

Add to:
- Settings API (`/settings/api`): GET returns value, POST accepts `turnerMotorDuration`
- DeviceSettings struct: persisted to EEPROM
- Web UI settings page: new input field

## Web UI Changes

### Dashboard
- Add motor status indicator card (ON/OFF, animated icon)
- Reflect in `/status` JSON as `"turnerMotor": 1|0`

### Control
- `?device=motor&mode=off` → KILL_OFF
- `?device=motor&mode=auto` → AUTO

### Settings page
- New field: "Turner motor duration (s)"

## Status JSON

```json
{
  "turnerMotor": 0,
  "turnerMotorDuration": 10000,
  ...
}
```

## Logging

Motor state is not added to the 5-byte compact `LogEntry` struct (bits 0-2 = heater/atomizer/fan, bits 3-7 = servo step, no room). Motor ON/OFF is logically a timing effect and does not need per-entry logging — the motor operates deterministically on `EGG_TURN_INTERVAL` + `TURNER_MOTOR_DURATION` so its state is always inferrable.

## Files Changed

| File | Changes |
|------|---------|
| `config.h` | Add `#define RELAY_TURNER_MOTOR D6` |
| `eggubator.ino` | Add motor global vars, pin init in `setup()`, motor control function, call from `loop()`, status JSON field, control handler, settings API handler, DeviceSettings struct field |
| `web_ui.h` | Dashboard: motor status card, settings page: duration input |
