# Pine OS 0.2

Pine OS is a native C++20 handheld operating environment. Version 0.1 runs in a
resizable 720 × 1280 SDL device window and isolates host behavior behind platform
interfaces so the shell, applications, configuration, and service logic can move to
future ARM/Linux hardware unchanged.

## M5Stack Tab5

The native ESP32-P4 firmware target is in `platform/tab5`. See [Tab5 build, flash, and feature status](docs/TAB5.md). Run `build.ps1 tab5` or `sh build.sh tab5` from an ESP-IDF 5.5.1 terminal. The desktop binary cannot be copied directly onto the Tab5.

## Build and run

Windows PowerShell:

```powershell
.\build.ps1 desktop
.\run.ps1
```

Linux or macOS:

```sh
./build.sh desktop
./run.sh
```

The first build fetches pinned SDL 3.2.22, nlohmann/json 3.12.0, and SQLite 3.50.4 source releases.
Later builds use CMake's local dependency cache. Requirements are CMake 3.24+, a C++20
compiler, and internet access for the first configure. Windows `build.ps1` automatically
finds the CMake bundled with Visual Studio Build Tools when CMake is not on `PATH`.
Inter Regular and SemiBold are bundled under the SIL Open Font License, so the professional
type system works without installing a font on the host computer.

## Test

See [the September 12 audit and validation report](docs/AUDIT-2026-09-12.md)
for the repaired defects, sanitizer checks, simulator scenarios, and remaining
physical-device checks.

```sh
ctest --test-dir build/desktop --output-on-failure
```

With a multi-configuration Windows generator, add `-C Release`.

## Simulator controls

- Click/tap an application tile to open it.
- Use **Home** or **Back** at the bottom to leave an app.
- Notes provides local search, pinning, guarded deletion, debounced autosave, and one
  persistent JSON record per note under `data/notes`.
- Finance provides offline accounts, exact-money transactions, atomic transfers, budgets,
  bills, subscriptions, goals, analytics, calculators, customizable dashboards, CSV import,
  CSV/JSON export, backups, balance privacy, and optional device-PIN locking. Its indexed,
  migrated SQLite database lives under `data/finance`; no bank credentials are collected.
- Tap any compatible editable field to open Pine's animated system keyboard. It supports
  QWERTY, Shift/Caps Lock, symbols, multiline/Next/Search actions, Backspace hold-repeat,
  touch cursor placement, and physical keyboard input through the same session API.
- Press **F12** or click **Dev** to toggle the developer panel.
- The developer panel changes battery, charging, Wi-Fi, Bluetooth, USB, and camera
  state through the same services used by applications and the status bar.
- Settings save automatically to `data/settings.json`.
- Pine files remain inside `data/storage`; camera captures are stored in `DCIM`.
- Console and file logs are written; the persistent log is `data/logs/pine.log`.

## Source layout

- `src/core`: application lifecycle, configuration, logger, generated canonical version
- `src/services`: platform-independent Pine APIs, including Notes, notifications, and security
- `src/finance`: exact money, migrated persistence, analytics, providers, import/export, and locking
- `src/platform`: hardware interfaces and desktop backends
- `src/input`: reusable text sessions, focus manager, and system on-screen keyboard
- `src/ui`: SDL shell, boot screen, status/navigation, developer panel, theme, TrueType text
- `src/apps`: Files, Camera, Bluetooth, Notes, Finance, and Settings
- `tests`: native service and persistence tests run by CTest

The camera backend uses a deterministic generated preview and capture on desktop. This
makes camera behavior testable on machines without a webcam. A future SDL camera backend
can replace only `DesktopCamera` without changing CameraService or CameraApp.

## Visual system and third-party notices

Pine uses a near-black and charcoal interface, ivory text, warm gray secondary text,
antique-gold focus/actions, subtle bronze borders, rounded surfaces, and generous touch
targets. Typography uses [Inter](https://rsms.me/inter/) 4.1 by Rasmus Andersson. The
font license is included at `assets/fonts/LICENSE-Inter.txt`. TrueType rasterization uses
`stb_truetype.h` from the public-domain/MIT-licensed stb libraries.
