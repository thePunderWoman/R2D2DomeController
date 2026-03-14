# Dome Controller

Arduino firmware for R2-D2's dome servo expander, running on an Arduino Pro Mini (16 MHz, ATmega328). Built with [PlatformIO](https://platformio.org/).

Written by Jessica Janiuk (thePunderWoman), iterated from John Thompson (FrettedLogic), based on Chris James' foundation.

## What it does

This board sits on the R2 serial bus and listens for `DM:` prefixed commands. When it receives one, it drives the dome's panel servos through the requested sequence and forwards the appropriate commands back out over serial to:

- **AstroPixelsPlus** (holo projectors, via `*` commands)
- **Astropixels / logic displays** (via `@` Marcduino commands)
- **Body controller** (via `BD:` prefixed commands)

## Hardware

- **MCU:** Arduino Pro Mini 16 MHz (ATmega328)
- **Servos:** 12 × VarSpeedServo, driven directly from digital pins 2–13
- **Serial:** Hardware Serial at 9600 baud (shared RX/TX bus)

### Panel assignments

| Panel | Pin | Type |
|---|---|---|
| PP1 | 2 | Pie panel |
| PP2 | 3 | Pie panel |
| PP5 | 4 | Pie panel |
| PP6 | 5 | Pie panel |
| P1 | 7 | Low panel |
| P2 | 8 | Low panel |
| P3 | 9 | Low panel |
| P4 | 10 | Low panel |
| P7 | 11 | Low panel |
| P10 | 12 | Low panel |
| P11 | 6 | Low panel |
| P13 | 13 | Low panel |

Pin 13 doubles as the status LED — it illuminates while a command sequence is running.

> The Dome Topper panel (DT, originally pin 6) is defined but currently commented out.

## Serial command protocol

Commands arrive on hardware Serial at 9600 baud, terminated with `\n` or `\r`. Only messages prefixed with `DM:` are acted on; everything else is ignored.

```
DM:COMMAND
```

### Supported commands

| Command | Description |
|---|---|
| `DM:RESET` | Close all panels, reset holos and logics |
| `DM:PIES` | Toggle pie panels open/closed (waves on open) |
| `DM:LOW` | Toggle low panels open/closed (waves on open) |
| `DM:OPENALL` | Toggle all panels open/closed |
| `DM:LEIA` | Leia hologram sequence (36 s, then auto-reset holos) |
| `DM:HEART` | Rainbow holos + "You're Wonderful" marquee (10 s, then auto-reset holos) |
| `DM:HELLO` | "Hello There" — P1 waves, marquee message |
| `DM:SCREAM` | Scream sequence — all panels open, holos flash, body scream audio |
| `DM:FLUTTER` | Flutter all panels |
| `DM:MUSE` | Toggle Muse mode (body controller) |

## Non-blocking holo timer

Some sequences run the holo projectors for a fixed duration before resetting. This is handled with a non-blocking `millis()`-based timer so the main loop stays responsive:

- `scheduleHoloReset(seconds)` — schedules `resetHolos()` to fire after N seconds
- `clearHoloTimer()` — cancels a pending scheduled reset
- `checkHoloReset()` — called every loop iteration; fires the reset when due

`resetHolos()` always calls `clearHoloTimer()` first, so a manual reset correctly cancels any pending auto-reset.

## AstroPixelsPlus holo commands

Holo projector commands use the `*` prefix and are forwarded over the shared serial bus. HP IDs: `01` = front, `02` = rear, `03` = top, `00` = all.

| Command | Effect |
|---|---|
| `*ST00` | Reset all holos |
| `*ON0x` / `*OF0x` | Turn a holo on or off |
| `*RD0x` | Random movement |
| `*HN0x` | Nod (up/down) |
| `*HP[pos][id]` | Move to preset position (0=down … 8=lower right) |
| `*HPS1[id]` | Leia LED sequence |
| `*HPS3[id]` | Dim pulse |
| `*HPS6[id]` | Rainbow |
| `*HPS7[id]` | Short circuit (alarm flash) |

## Project structure

```
src/
  Dome_Controller.ino   Main sketch
lib/
  Filthy/
    FlthyHPs_v1.8.ino   Reference — original Filthy HPs I2C command set
  README.md             Third-party library install instructions
include/                (empty, PlatformIO default)
platformio.ini          Build config — Arduino Pro Mini 16 MHz
```

## Building

1. Install [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
2. Clone [VarSpeedServo](https://github.com/pvanallen/VarSpeedServo) into `lib/`
3. `pio run` to build, `pio run --target upload` to flash
