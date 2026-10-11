"""Ten-minute actual Release-client MXP/Pueblo transcript/resource soak.

Requires Windows and psutil. Uses disconnected worlds with simulated input;
the normal image loader downloads only the public test PNG. Never uses user worlds.
"""
import argparse
import csv
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import time
import psutil

ROOT = Path(__file__).resolve().parents[1]


def summarize(samples):
    active = [row for row in samples if 120 <= float(row[0]) < 600]
    early = [row for row in active if float(row[0]) < 180]
    late = [row for row in active if float(row[0]) >= 540]
    assert early and late, 'Incomplete measurement window'
    growth = sum(float(r[1]) for r in late)/len(late) - sum(float(r[1]) for r in early)/len(early)
    summary = {'private_growth_bytes': round(growth),
               'private_min_bytes': min(int(r[1]) for r in active),
               'private_max_bytes': max(int(r[1]) for r in active),
               'gdi_min': min(int(r[3]) for r in active),
               'gdi_max': max(int(r[3]) for r in active),
               'user_min': min(int(r[4]) for r in active),
               'user_max': max(int(r[4]) for r in active)}
    summary['bounded_resources'] = (growth <= 8*1024*1024 and
        max(int(r[3]) for r in late) <= min(int(r[3]) for r in early)+4 and
        max(int(r[4]) for r in late) <= min(int(r[4]) for r in early)+4)
    return summary

parser = argparse.ArgumentParser()
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--destination', type=Path, required=True)
args = parser.parse_args()
dest = args.destination.resolve()
assert not dest.exists(), 'Use a fresh isolated runtime'
subprocess.run([sys.executable, str(ROOT/'tests/prepare_native_worlds.py'),
    '--exe', str(args.exe.resolve()), '--runtime', str(args.runtime.resolve()),
    '--destination', str(dest), '--plugin', str(ROOT/'tests/inline_image_transcript_soak.xml')], check=True)
db = sqlite3.connect(dest/'mushclient_prefs.sqlite')
db.execute("INSERT OR REPLACE INTO prefs VALUES('AutoConnectWorlds','0')")
db.commit(); db.close()
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
client = subprocess.Popen([str(dest/'MUSHclient.exe'), '/noregister'], cwd=dest, startupinfo=startup)
process = psutil.Process(client.pid)
get_gui = ctypes.windll.user32.GetGuiResources
get_gui.argtypes = [wintypes.HANDLE, wintypes.DWORD]
get_gui.restype = wintypes.DWORD
samples = []
started = time.monotonic()
try:
    with (dest/'memory.csv').open('w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['elapsed_s','private_bytes','working_set_bytes','gdi_handles','user_handles'])
        while time.monotonic()-started < 630:
            assert client.poll() is None, 'Client exited during soak'
            memory = process.memory_info()
            row = [round(time.monotonic()-started,1), memory.private, memory.rss,
                   get_gui(wintypes.HANDLE(int(client._handle)),0),
                   get_gui(wintypes.HANDLE(int(client._handle)),1)]
            writer.writerow(row); f.flush(); samples.append(row)
            time.sleep(5)
    for protocol in ('mxp','pueblo'):
        report = (dest/(protocol+'-test.xml.results.txt')).read_text()
        assert 'PASS COMPLETE' in report, report
    summary = summarize(samples)
    summary['executable_sha256'] = hashlib.sha256((dest/'MUSHclient.exe').read_bytes()).hexdigest()
    (dest/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    assert summary['bounded_resources'], summary
    print('PASS both 600-second protocol transcript soaks')
    print(json.dumps(summary,indent=2))
    print('Evidence:',dest)
finally:
    if client.poll() is None: client.terminate(); client.wait(timeout=20)
