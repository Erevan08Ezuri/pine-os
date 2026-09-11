#!/usr/bin/env python3
"""Fetch pinned source dependencies for the Tab5 firmware."""
from pathlib import Path
import subprocess
import urllib.request
import hashlib
import zipfile
import io
ROOT = Path(__file__).resolve().parent
DEPS = ROOT / '.deps'
REPOS = {
    'sdl': ('https://github.com/georgik/esp-idf-component-SDL.git', 'b5916d3cb940626c6ca490ee273d6ddd36831250'),
    'factory': ('https://github.com/m5stack/M5Tab5-UserDemo.git', '68b19d37fbf9cefd5f256992f5dca34794c62ab4'),
    'json': ('https://github.com/nlohmann/json.git', '55f93686c01528224f448c19128836e7df245f72'),
}
def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def replace_required(path, old, new):
    """Apply an idempotent vendor patch and fail if the expected source changed."""
    contents = path.read_text(encoding='utf-8')
    if new in contents:
        return
    if old in contents:
        path.write_text(contents.replace(old, new), encoding='utf-8')
    else:
        raise RuntimeError(f'Unable to patch unexpected vendor source: {path}')


def prepare():
    DEPS.mkdir(exist_ok=True)
    for name, (url, sha) in REPOS.items():
        dest = DEPS / name
        if not (dest / '.git').exists():
            run('git', 'init', str(dest))
            run('git', 'remote', 'add', 'origin', url, cwd=dest)
        current = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=dest, text=True, capture_output=True)
        if current.stdout.strip() != sha:
            run('git', 'fetch', '--depth', '1', 'origin', sha, cwd=dest)
            run('git', 'checkout', '--detach', sha, cwd=dest)
        if name == 'sdl':
            run('git', 'submodule', 'update', '--init', '--recursive', '--depth', '1', cwd=dest)
    # Explicit modifications to the upstream BSP: disable its LVGL dependency.
    bsp = DEPS / 'factory/platforms/tab5/components/m5stack_tab5'
    cfg = bsp / 'include/bsp/config.h'
    replace_required(cfg, '#define BSP_CONFIG_NO_GRAPHIC_LIB (0)', '#define BSP_CONFIG_NO_GRAPHIC_LIB (1)')
    cmake = bsp / 'CMakeLists.txt'
    replace_required(cmake, '        esp_lvgl_port', '        esp_lcd_touch_gt911')
    # The pinned factory BSP refers to ES7210 microphone selectors as ES7120,
    # which prevents the otherwise supported audio component from compiling.
    replace_required(bsp / 'm5stack_tab5.c', 'ES7120_SEL_MIC', 'ES7210_SEL_MIC')
    panel = DEPS / 'factory/platforms/tab5/components/esp_lcd_st7121/CMakeLists.txt'
    panel.write_text(
        'idf_component_register(SRCS "esp_lcd_st7121.c" INCLUDE_DIRS "include" REQUIRES esp_lcd)\n',
        encoding='utf-8',
    )
    sqlite = DEPS / 'sqlite'
    if not (sqlite / 'sqlite3.c').exists():
        with urllib.request.urlopen('https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip', timeout=120) as r:
            data = r.read()
        if hashlib.sha256(data).hexdigest() != '1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032':
            raise RuntimeError('SQLite checksum mismatch')
        sqlite.mkdir(exist_ok=True)
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            for name in ('sqlite3.c','sqlite3.h'):
                (sqlite / name).write_bytes(z.read('sqlite-amalgamation-3500400/' + name))
    # SQLITE_OS_OTHER normally selects no-op mutexes. PineOS supplies a custom
    # VFS but intentionally uses ESP-IDF pthreads, so do not select a second
    # mutex implementation when SQLITE_MUTEX_PTHREADS was explicitly chosen.
    replace_required(
        sqlite / 'sqlite3.c',
        '#if SQLITE_THREADSAFE && !defined(SQLITE_MUTEX_NOOP)',
        '#if SQLITE_THREADSAFE && !defined(SQLITE_MUTEX_NOOP) && !defined(SQLITE_MUTEX_PTHREADS)',
    )
    print('Tab5 dependencies ready.')
if __name__ == '__main__':
    prepare()
