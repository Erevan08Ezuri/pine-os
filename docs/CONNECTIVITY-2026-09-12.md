# Tab5 connectivity and device lock implementation

This update adds a BLE central, Wi-Fi network discovery, and the requested
numeric device PIN screen. It integrates the camera, brightness, clock and display
changes through main commit `1e91d9d`.

## User behavior

- A fresh device starts locked with PIN **123456**. Enter six digits and tap
  Unlock. Change PIN and Lock controls are in Settings. The keypad also handles
  physical number keys, Backspace and Enter in the simulator. Home, Escape,
  developer shortcuts and application prompts cannot bypass it.
- PIN derivation and checks run asynchronously. The PIN is stored as a salted
  hash under `data/device-lock`, independently of the existing Finance PIN.
  The default is never reapplied over existing or damaged credentials. Five
  failed attempts start a 30-second in-session retry delay, increasing to five
  minutes. This is an application lock, not encrypted storage.
- Settings > Wi-Fi > Networks scans visible 2.4 GHz networks. Join uses the
  selected SSID and prompts only for its password. Open networks connect without
  a password. Hidden Network retains manual entry. Results include RSSI/security,
  deduplicate repeated APs, and paginate up to 32 networks. The worker handles
  radio busy errors, scan completion/timeout, off requests, and saved connections.
- Bluetooth scans BLE advertisements for ten seconds, with Stop Scan, signal
  strength, sanitized names, bounded results, and one central link at a time.
  Connect/Disconnect indicate pending work and wait for controller confirmation.
  Late connection success after Off is terminated. Snapshots copy data under a
  mutex so radio callbacks cannot invalidate UI iteration.

## Firmware integration

NimBLE runs on the P4 and uses ESP-Hosted 1.4.0 VHCI over the existing SDIO bus to
its C6. Initial Wi-Fi/BLE transport setup is serialized. Startup is lazy and stays
off the UI thread. A preflight checks the transport without the public hosted
reset wrapper's fatal assertion. The P4 package does not overwrite the C6.

The pinned hosted version lacks IDF 5.5.1's `ble_transport_ll_deinit` hook.
The adapter supplies a no-op transport teardown: NimBLE stops its GAP activity,
while the shared SDIO/Wi-Fi transport remains alive. Project-owned Bluetooth
sdkconfig flags are migrated from checked-in defaults, with a backup of the
previous local config, and verified before packaging or flashing.

Integration compilation also required explicitly retaining the camera's esp_ipa
component and checking nullptr for the pinned esp_video mmap failure convention.
The resolved dependency lock now includes the camera's esp_h264 dependency.

## Validation

- All six CTest suites pass with AddressSanitizer and UndefinedBehaviorSanitizer.
  LeakSanitizer is disabled because of tracing in this execution environment.
- BLE state regression tests cover discovery deduplication, address types, RSSI,
  bounded lists, rescans with an active link, rejected requests, connection
  failures, late success, UTF-8/control-character names and copied snapshots.
- The actual Tab5Bluetooth.cpp runs against a controlled GAP/HCI API shim in
  `pine_tab5_bluetooth`: unavailable C6, scans, stop, address parsing, asynchronous
  connect/disconnect, late success after Off, reset, and host/worker teardown.
- Wi-Fi/PIN regression tests cover strongest-AP deduplication, open/secured
  selection, default PIN creation, invalid PINs, current-PIN verification,
  change/restart persistence, retry delay, damaged storage, stale unlock results,
  and real keypad taps through all three Change PIN stages.
- 40 simulator acceptance renders pass: 20 in RGBA32 and 20 in RGB565. The
  device keypad, Settings, Wi-Fi list and Bluetooth list were visually inspected.
- Native ESP32-P4 firmware builds with ESP-IDF 5.5.1, with 200 MHz HEX PSRAM and
  PSRAM XIP disabled. Packaging includes bootloader, partition table, app, flash
  arguments, and updated instructions.

## Device validation still needed

The API shim is not RF or ESP-Hosted wire-protocol emulation. Physical BLE scans,
connections, Wi-Fi scans/joins, concurrent Wi-Fi/BLE use and task stack headroom
remain to be checked on a Tab5. The C6 needs compatible firmware with BLE HCI
support. No pairing/bonding, application GATT profiles, Bluetooth audio, keyboard
input or phone file transfer are implemented in this stage. WPA3-only, enterprise
and WEP networks are not selectable through the initial network picker.

See [Tab5 setup and device checks](TAB5.md) for build, flash and use instructions.
