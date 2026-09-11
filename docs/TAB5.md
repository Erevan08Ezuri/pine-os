# PineOS on M5Stack Tab5

This is the native ESP32-P4 firmware target. It runs the existing PineOS C++ shell,
Notes, on-screen keyboard, Files, Settings, and offline Finance application.
The desktop simulator remains available through the desktop build target.

## Current status

The original v0.2.0 image compiled but failed on a physical Tab5 before app_main:
`esp_startup_start_app app_startup.c:86 (res == pdTRUE)`. ESP-IDF could not allocate
the 64 KiB main task stack from internal SRAM after ESP-Hosted initialization.
PSRAM cannot satisfy this FreeRTOS allocation. The boot-fix build reduces that
stack to 24 KiB, moves the shell object to the heap, and logs internal free memory,
largest free block, and main stack high-water headroom at entry, shell startup,
and once per minute. Task stacks remain in internal memory for flash safety.

Desktop unit/service tests pass. Firmware builds also emit compiler stack-usage
reports (`*.su`) for PineOS sources. These report individual function frames;
device high-water measurements are still needed to validate complete call chains.

The boot-fix package requires device validation. Physical display, touch,
radio, and power testing still requires a connected M5Stack Tab5 and has not been
performed by the authoring environment; complete the checklist below before calling
a particular hardware installation production-validated.

## Hardware

16 MB flash, 32 MB PSRAM, native 720 x 1280 portrait interface. The pinned M5Stack
factory BSP supports the original ILI9881C/GT911 and newer ST7123/ST7121 panels.
Touch input uses native coordinates and release positions.
Wi-Fi uses the onboard ESP32-C6 over the factory SDIO pins; hosted 1.4.0 and
wifi-remote 0.8.5 match the factory application's versions.
The C6 factory firmware is required and is not overwritten by this project.

Internal LittleFS stores /pine/data, including settings, notes, and Finance.
Fonts are embedded. No SD card is needed. SQLite uses persistent rollback journals
with FULL synchronization and in-process locks; WAL/mmap are not used.
A damaged data partition is never formatted automatically. A completely erased
data partition is provisioned on first boot.

## Build from source

Install ESP-IDF **5.5.1**, including ESP32-P4 tools, and open its terminal.
Clone this repository or download this branch as a ZIP, then run from its root:

Windows:
```powershell
.\build.ps1 tab5
python platform/tab5/tab5.py flash --port COM5
python platform/tab5/tab5.py monitor --port COM5
```

Linux/macOS:
```sh
sh build.sh tab5
python platform/tab5/tab5.py flash --port /dev/ttyACM0
python platform/tab5/tab5.py monitor --port /dev/ttyACM0
```

Use your actual serial port. Connect with a USB-C data cable, hold Reset until the
green LED flashes rapidly, then release for download mode. After flashing, reset.
The firmware boots directly into PineOS.

## Prebuilt firmware

A successful **Tab5 firmware** GitHub Actions run provides a **PineOS-Tab5** ZIP.
Extract it, install esptool 4.x on the connected computer, and run inside the folder:

```sh
python -m esptool --chip esp32p4 --port COM5 write_flash 0x2000 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin 0x10000 pine_tab5.bin
```

The included flasher_args.json is authoritative for offsets. Replace COM5 as needed.
Regular updates do not write the data partition. Do not use erase-flash for updates.
First installation replaces the factory P4 application and partition table; export
anything valuable from the factory application first. If first boot reports that
pine_data cannot mount, old factory data may occupy that region. Only after backing
up and only for first installation, explicitly clear the new Pine data region:

```sh
python -m esptool --chip esp32p4 --port COM5 erase_region 0x710000 0x8f0000
```

This destroys everything in that region, including existing PineOS notes and Finance
if it has already been used. It is never part of a routine firmware update.

## First boot

Open Settings > Wi-Fi > Connect, enter a 2.4 GHz SSID and WPA2/WPA3-compatible
password. Credentials are kept in NVS, not in the repository or log.
Wi-Fi initialization runs in a background thread so an unavailable radio does not
block the UI. NTP sets the clock; the RX8130 saves UTC for later offline use.
The status bar shows --:-- until time is valid. Finance waits for a valid clock
to prevent incorrect transaction dates. Display time currently uses UTC.

## Feature boundaries

- Display, touch, software keyboard, embedded fonts, internal persistence, Wi-Fi
  configuration, NTP/RTC, and brightness are implemented for hardware.
- The existing Finance features are reused, not replaced by a smaller demo.
- Speaker volume/mute is connected to the codec; a music player is not included.
- Camera capture, Bluetooth pairing/audio, USB mass storage and host keyboard,
  microSD browsing, and battery percentage are not implemented in this target.
  Their desktop simulations are never used as hardware results.
- Battery level displays unavailable; the board does not expose a supported
  fuel gauge through this backend.
- The PIN feature is an application lock, not disk encryption.

## Device acceptance checklist

1. Confirm portrait image and touch at every corner and keyboard edge.
2. Create/edit a note, reset, and verify saved content.
3. Join Wi-Fi, check clock sync, reset, and confirm RTC recovery.
4. Create Finance accounts and a transfer; reset and verify exact balances.
5. Test Finance export, backup, restore, and PIN with noncritical sample records.
6. Test brightness, volume, power cycle, and disconnect/reconnect Wi-Fi.
7. Confirm unsupported features cannot produce fake captures or pairings.
8. Update firmware without erasing flash; confirm notes and Finance survive.

## Upstream references

- https://docs.m5stack.com/en/core/Tab5
- https://github.com/m5stack/M5Tab5-UserDemo
- https://github.com/georgik/esp-idf-component-SDL
- https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32p4/api-guides/cplusplus.html

Dependency source pins and validated factory BSP/SQLite build adaptations are in
prepare.py. The ESP Component Manager lock file fixes the resolved component graph.
Upstream licenses remain in the downloaded dependencies; Inter's license is bundled
in assets/fonts/LICENSE-Inter.txt.
