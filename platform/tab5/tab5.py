#!/usr/bin/env python3
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

from prepare import prepare


def package_firmware(tab5_dir: Path) -> Path:
    build_dir = tab5_dir / 'build'
    required = [
        build_dir / 'bootloader' / 'bootloader.bin',
        build_dir / 'partition_table' / 'partition-table.bin',
        build_dir / 'pine_tab5.bin',
        build_dir / 'flasher_args.json',
    ]
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError('Build succeeded but firmware package is incomplete; missing: ' + ', '.join(missing))

    dist_dir = tab5_dir.parent.parent / 'dist'
    dist_dir.mkdir(parents=True, exist_ok=True)
    zip_path = dist_dir / 'PineOS-Tab5.zip'

    entries = [
        (required[0], 'bootloader/bootloader.bin'),
        (required[1], 'partition_table/partition-table.bin'),
        (required[2], 'pine_tab5.bin'),
        (required[3], 'flasher_args.json'),
        (tab5_dir.parent.parent / 'docs' / 'TAB5.md', 'README.md'),
    ]

    with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for source, archive_name in entries:
            archive.write(source, archive_name)

    print(f'[PINE][PACKAGE] Created {zip_path}')
    return zip_path


def bluetooth_config(tab5_dir: Path) -> dict:
    """Read project-owned Bluetooth flags from the checked-in defaults."""
    flags = {}
    for line in (tab5_dir / 'sdkconfig.defaults').read_text().splitlines():
        match = re.fullmatch(r'(CONFIG_[A-Z0-9_]+)=(.*)', line)
        disabled = re.fullmatch(r'# (CONFIG_[A-Z0-9_]+) is not set', line)
        if match or disabled:
            key = (match or disabled).group(1)
            if key.startswith('CONFIG_BT_') or key.startswith('CONFIG_ESP_HOSTED_') and ('NIMBLE' in key):
                flags[key] = match.group(2) if match else 'n'
    return flags


def migrate_bluetooth_config(tab5_dir: Path) -> None:
    """Upgrade older local configs without discarding display/storage settings."""
    path = tab5_dir / 'sdkconfig'
    if not path.exists():
        return  # ESP-IDF will load sdkconfig.defaults on a fresh build.
    flags = bluetooth_config(tab5_dir)
    original = path.read_text()
    current = {}
    for line in original.splitlines():
        match = re.fullmatch(r'(CONFIG_[A-Z0-9_]+)=(.*)', line)
        disabled = re.fullmatch(r'# (CONFIG_[A-Z0-9_]+) is not set', line)
        if match or disabled:
            current[(match or disabled).group(1)] = match.group(2) if match else 'n'
    if all(current.get(key, 'n') == value for key, value in flags.items()):
        return
    lines = []
    for line in original.splitlines():
        match = re.match(r'(?:# )?(CONFIG_[A-Z0-9_]+)(?:=| is not set)', line)
        if not match or match.group(1) not in flags:
            lines.append(line)
    lines.extend(f'# {key} is not set' if value == 'n' else f'{key}={value}'
                 for key, value in flags.items())
    updated = '\n'.join(lines) + '\n'
    if updated != original:
        backup = tab5_dir / 'sdkconfig.before-bluetooth'
        if not backup.exists():
            backup.write_text(original)
        temporary = tab5_dir / 'sdkconfig.bluetooth-tmp'
        temporary.write_text(updated)
        temporary.replace(path)
        print('[PINE][CONFIG] Updated local Bluetooth flags; original config backed up.')


def validate_generated_config(tab5_dir: Path) -> None:
    """Refuse to package firmware when critical Tab5 hardware config was dropped."""
    sdkconfig = tab5_dir / 'sdkconfig'
    if not sdkconfig.is_file():
        raise RuntimeError('Generated sdkconfig is missing after build')

    text = set(sdkconfig.read_text(encoding='utf-8', errors='replace').splitlines())
    required = (
        'CONFIG_IDF_EXPERIMENTAL_FEATURES=y',
        'CONFIG_SPIRAM_MODE_HEX=y',
        'CONFIG_SPIRAM_SPEED_200M=y',
        'CONFIG_CAMERA_SC202CS=y',
        'CONFIG_CAMERA_SC202CS_AUTO_DETECT=y',
        'CONFIG_CAMERA_SC202CS_AUTO_DETECT_MIPI_INTERFACE_SENSOR=y',
        'CONFIG_CAMERA_SC202CS_MIPI_RAW8_1280x720_30FPS=y',
        'CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE=y',
        'CONFIG_ESP_VIDEO_ENABLE_ISP=y',
        'CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE=y',
    )
    missing = [entry for entry in required if entry not in text]
    if missing or 'CONFIG_SPIRAM_SPEED_20M=y' in text:
        details = ', '.join(missing) if missing else 'CONFIG_SPIRAM_SPEED_20M=y is still enabled'
        raise RuntimeError(
            'Unsafe/incomplete Tab5 hardware config: ' + details + '. '
            'Delete platform/tab5/sdkconfig and platform/tab5/build, then rebuild from current defaults.'
        )

    if 'CONFIG_SPIRAM_XIP_FROM_PSRAM=y' in text:
        raise RuntimeError(
            'Tab5 build still has CONFIG_SPIRAM_XIP_FROM_PSRAM=y, which can add PSRAM bus contention. '
            'Delete platform/tab5/sdkconfig and platform/tab5/build, then rebuild.'
        )

    for key, value in bluetooth_config(tab5_dir).items():
        if (value == 'n' and f'{key}=y' in text) or (value != 'n' and f'{key}={value}' not in text):
            raise RuntimeError(f'Tab5 Bluetooth configuration mismatch: {key} must be {value}')

    print('[PINE][CONFIG] Verified 200 MHz HEX PSRAM, MIPI camera/ISP, NimBLE VHCI, and XIP-from-PSRAM disabled.')


def main():
    parser = argparse.ArgumentParser(description='Build/flash PineOS for M5Stack Tab5')
    parser.add_argument('action', choices=['build', 'flash', 'monitor', 'menuconfig'], nargs='?', default='build')
    parser.add_argument('--port', help='COM5 or /dev/ttyACM0, for example')
    args = parser.parse_args()

    idf = shutil.which('idf.py')
    if not idf or not os.environ.get('IDF_PATH'):
        parser.error('Open an ESP-IDF 5.5.1 terminal first; see docs/TAB5.md')
    if args.action in ('flash', 'monitor') and not args.port:
        parser.error('--port is required to select the intended device')

    if args.action != 'monitor':
        prepare()
    tab5_dir = Path(__file__).resolve().parent
    if args.action in ('build', 'flash'):
        migrate_bluetooth_config(tab5_dir)
    command = [sys.executable, idf, '-C', str(tab5_dir)]
    if args.port:
        command += ['-p', args.port]

    # idf.py flash can implicitly build. Validate that completed build before
    # allowing any bytes to reach the device, just as the packaging path does.
    if args.action == 'flash':
        result = subprocess.run(command + ['build'])
        if result.returncode != 0:
            return result.returncode
        try:
            validate_generated_config(tab5_dir)
        except RuntimeError as exc:
            print(f'[PINE][FLASH][ERROR] {exc}', file=sys.stderr)
            return 1
    result = subprocess.run(command + [args.action])
    if result.returncode != 0:
        return result.returncode

    if args.action == 'build':
        try:
            validate_generated_config(tab5_dir)
            package_firmware(tab5_dir)
        except Exception as exc:
            print(f'[PINE][PACKAGE][ERROR] {exc}', file=sys.stderr)
            return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
