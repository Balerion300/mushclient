"""Run a disposable native fixture without displaying or switching desktops."""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import sys
import uuid
from private_desktop_process import PrivateDesktopProcess

p=argparse.ArgumentParser()
p.add_argument('--exe',required=True,type=Path)
p.add_argument('--cwd',required=True,type=Path)
p.add_argument('--timeout',type=int,default=60)
a=p.parse_args()
user=ctypes.WinDLL('user32',use_last_error=True)
user.CreateDesktopW.argtypes=[wintypes.LPCWSTR,wintypes.LPCWSTR,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p]
user.CreateDesktopW.restype=wintypes.HANDLE
user.CloseDesktop.argtypes=[wintypes.HANDLE]
name='ValyriaQA-'+uuid.uuid4().hex
handle=user.CreateDesktopW(name,None,None,0,0x00ff,None)
if not handle:raise ctypes.WinError(ctypes.get_last_error())
process=None
try:
    proof_file=a.cwd/'desktop-preflight.json'
    proof=PrivateDesktopProcess([sys.executable,str(Path(__file__).with_name('inspect_fixture_desktop.py')),'0',str(proof_file)],name,a.cwd,os.environ.copy())
    try:
        proof.wait(timeout=8)
        assert proof.returncode==0 and json.loads(proof_file.read_text())['desktop']==name
    finally:
        if proof.poll() is None:proof.terminate();proof.wait(timeout=3)
        proof.close()
    process=PrivateDesktopProcess([str(a.exe.resolve())],name,a.cwd,os.environ.copy())
    code=process.wait(timeout=a.timeout)
    (a.cwd/'private-desktop-exit.json').write_text(json.dumps({'desktop':name,'exit_code':code}),encoding='utf-8')
    print('Private desktop fixture exit:',code)
    sys.exit(0 if code==0 else 1)
finally:
    if process:
        if process.poll() is None:process.terminate();process.wait(timeout=3)
        process.close()
    user.CloseDesktop(handle)
