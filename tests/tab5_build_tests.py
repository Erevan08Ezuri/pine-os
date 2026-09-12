"""Regression checks for the firmware packaging/flash safety gate."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'platform/tab5'))
import tab5


class FirmwareConfigTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        defaults = Path(tab5.__file__).parent / 'sdkconfig.defaults'
        (self.root / 'sdkconfig.defaults').write_text(defaults.read_text())
        self.valid = '\n'.join(('CONFIG_IDF_EXPERIMENTAL_FEATURES=y',
                               'CONFIG_SPIRAM_MODE_HEX=y',
                               'CONFIG_SPIRAM_SPEED_200M=y',
                               'CONFIG_CAMERA_SC202CS=y',
                               'CONFIG_CAMERA_SC202CS_AUTO_DETECT=y',
                               'CONFIG_CAMERA_SC202CS_AUTO_DETECT_MIPI_INTERFACE_SENSOR=y',
                               'CONFIG_CAMERA_SC202CS_MIPI_RAW8_1280x720_30FPS=y',
                               'CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE=y',
                               'CONFIG_ESP_VIDEO_ENABLE_ISP=y',
                               'CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE=y'))
        self.valid += '\n' + '\n'.join(f'{key}={value}' for key, value in tab5.bluetooth_config(self.root).items() if value != 'n')

    def config(self, text):
        (self.root / 'sdkconfig').write_text(text)

    def test_valid_config(self):
        self.config(self.valid)
        tab5.validate_generated_config(self.root)

    def test_comments_do_not_count_as_enabled_config(self):
        self.config(self.valid.replace('CONFIG_SPIRAM_SPEED_200M=y', '# CONFIG_SPIRAM_SPEED_200M=y'))
        with self.assertRaises(RuntimeError):
            tab5.validate_generated_config(self.root)

    def test_xip_rejected(self):
        self.config(self.valid + '\nCONFIG_SPIRAM_XIP_FROM_PSRAM=y')
        with self.assertRaises(RuntimeError):
            tab5.validate_generated_config(self.root)

    def test_disabled_bluetooth_rejected(self):
        self.config(self.valid.replace('CONFIG_BT_ENABLED=y', '# CONFIG_BT_ENABLED is not set'))
        with self.assertRaises(RuntimeError):
            tab5.validate_generated_config(self.root)

    def test_wrong_transport_rejected(self):
        self.config(self.valid + '\nCONFIG_BT_NIMBLE_TRANSPORT_UART=y')
        with self.assertRaises(RuntimeError):
            tab5.validate_generated_config(self.root)

    def test_old_config_migrated_without_losing_settings(self):
        original = '# CONFIG_BT_ENABLED is not set\nCONFIG_CUSTOM_DISPLAY=42\n'
        self.config(original)
        tab5.migrate_bluetooth_config(self.root)
        first = (self.root/'sdkconfig').read_text()
        self.assertIn('CONFIG_BT_ENABLED=y', first)
        self.assertIn('CONFIG_CUSTOM_DISPLAY=42', first)
        self.assertEqual((self.root/'sdkconfig.before-bluetooth').read_text(), original)
        tab5.migrate_bluetooth_config(self.root)
        self.assertEqual((self.root/'sdkconfig').read_text(), first)

    def test_camera_config_required(self):
        self.config(self.valid.replace('CONFIG_CAMERA_SC202CS=y', '# CONFIG_CAMERA_SC202CS is not set'))
        with self.assertRaises(RuntimeError):
            tab5.validate_generated_config(self.root)

    def test_invalid_build_never_reaches_flash(self):
        self.config('CONFIG_SPIRAM_SPEED_20M=y')
        with patch.object(tab5, '__file__', str(self.root/'tab5.py')), \
             patch.object(tab5, 'prepare'), \
             patch.object(tab5.shutil, 'which', return_value='/idf/idf.py'), \
             patch.dict(tab5.os.environ, {'IDF_PATH': '/idf'}), \
             patch.object(sys, 'argv', ['tab5.py', 'flash', '--port', 'COM5']), \
             patch.object(tab5.subprocess, 'run') as run:
            run.return_value.returncode = 0
            self.assertEqual(tab5.main(), 1)
            self.assertEqual(len(run.call_args_list), 1)
            self.assertEqual(run.call_args.args[0][-1], 'build')


if __name__ == '__main__':
    unittest.main()
