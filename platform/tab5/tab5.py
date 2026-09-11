#!/usr/bin/env python3
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
from prepare import prepare
def main():
    parser=argparse.ArgumentParser(description='Build/flash PineOS for M5Stack Tab5')
    parser.add_argument('action',choices=['build','flash','monitor','menuconfig'],nargs='?',default='build')
    parser.add_argument('--port',help='COM5 or /dev/ttyACM0, for example')
    args=parser.parse_args()
    idf=shutil.which('idf.py')
    if not idf or not os.environ.get('IDF_PATH'):
        parser.error('Open an ESP-IDF 5.5.1 terminal first; see docs/TAB5.md')
    if args.action in ('flash','monitor') and not args.port:
        parser.error('--port is required to select the intended device')
    prepare()
    command=[sys.executable,idf,'-C',str(Path(__file__).resolve().parent)]
    if args.port: command+=['-p',args.port]
    return subprocess.run(command+[args.action]).returncode
if __name__=='__main__':
    sys.exit(main())
