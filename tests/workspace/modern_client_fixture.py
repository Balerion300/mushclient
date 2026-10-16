"""Two fictional loopback worlds for modern tab/input UI acceptance.

Copies only clean build/runtime components into a fresh temporary directory.
No real credentials, installed profiles, production connections or player output.
The CLI prints the exact candidate PID/path; use normal UI close during acceptance.
Writing 'stop' to fixture.control stops only this fixture and its candidate process.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import socket
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, default=ROOT/'WinRel/MUSHclient.exe')
p.add_argument('--runtime', type=Path, default=ROOT/'.distribution-source/extracted')
p.add_argument('--package',type=Path,help='Copy a clean full package instead of assembling runtime components')
p.add_argument('--default-settings',action='store_true',help='Keep normal close confirmation, spelling and window defaults')
p.add_argument('--destination', type=Path)
p.add_argument('--duration', type=int, default=1800)
p.add_argument('--focus-trace', action='store_true', help='Bounded own-client focus diagnostics; logs no input text')
p.add_argument('--main-composer-test', action='store_true', help='Enable the fictional Main-composer regression driver')
p.add_argument('--tabbed-worlds', action='store_true', help='Start fictional acceptance in optional character document tab mode')
p.add_argument('--isolated-desktop', action='store_true', help='Confine all candidate windows to a private non-input Windows desktop; never switch desktops')
a = p.parse_args()
dest = a.destination or Path(tempfile.mkdtemp(prefix='mush-modern-acceptance-'))
dest.mkdir(exist_ok=True)
if a.package:
    assert not (a.package/'mushclient_prefs.sqlite').exists(),'Use a clean package without user state'
    shutil.copytree(a.package,dest,dirs_exist_ok=True)
    shutil.copy2(a.exe,dest/'MUSHclient.exe')
else:
    for name in ('locale', 'pennmush-reference'):
        source = a.runtime/name if name == 'locale' else ROOT/'third_party'/name
        shutil.copytree(source, dest/name)
    shutil.copytree(ROOT/'spelling/packs',dest/'spelling/packs')
    for source in (a.exe, a.runtime/'lua5.1.dll', ROOT/'third_party/scintilla/bin/Scintilla.dll', ROOT/'WinRel/MUSHclientAgent.exe'):
        shutil.copy2(source, dest/source.name)

plugin = dest/'fixture-plugin.xml'
plugin.write_text('''<?xml version="1.0"?><!DOCTYPE muclient><muclient>
<plugin name="Modern_Workspace_Fixture" author="MUSHclient contributors"
 id="c1c8f84d8a2e407aba0b7139" language="Lua" version="1.0" requires="5.07"/>
<aliases><alias name="fixture_alias" enabled="y" regexp="y" match="^(\\+alpha|\\+beta) alias (.*)$"><send>%1 ALIAS_%2</send></alias></aliases>
<script><![CDATA[
local enabled=false
local function record(text)
 local f=assert(io.open(GetPluginInfo(GetPluginID(),6)..'.receipt.txt','a'));f:write(text,'\\n');f:close()
end
function OnPluginInstall()
 enabled=GetAlphaOption('site')=='127.0.0.1' and WorldName():find('Workspace acceptance ',1,true)==1
 if enabled then record('INSTALL '..WorldName()) end
end
function OnPluginCommandEntered(text)
 if not enabled then return text end
 record('HUMAN_ENTERED '..text)
 if text=='+alpha hook' then return '+alpha HOOK_TRANSFORMED' end
 if text=='+beta hook' then return '+beta HOOK_TRANSFORMED' end
 if text=='+alpha decline' or text=='+beta decline' then return '\\r' end
 return text
end
function OnPluginCommand(text) if enabled then record('COMMAND '..text) end return true end
]]></script></muclient>''', encoding='utf-8')
if a.main_composer_test:
    driver = (Path(__file__).with_name('main_composer_driver.lua')).read_text(encoding='utf-8')
    plugin.write_text(plugin.read_text(encoding='utf-8').replace(']]></script>', driver+'\n]]></script>'), encoding='utf-8')

stop = threading.Event()
listeners = []
worlds = []
db = sqlite3.connect(dest/'mushclient_prefs.sqlite')
db.executescript('''CREATE TABLE control(name TEXT PRIMARY KEY,value INT);
CREATE TABLE prefs(name TEXT PRIMARY KEY,value TEXT);
CREATE TABLE worlds(name TEXT PRIMARY KEY,value TEXT);
CREATE TABLE workspace_route_v1(world_id TEXT,name TEXT,pattern TEXT,regex INTEGER,omit_main INTEGER,PRIMARY KEY(world_id,name,pattern,regex));
CREATE TABLE workspace_input_prefix_v1(world_id TEXT,name TEXT,prefix TEXT,PRIMARY KEY(world_id,name));''')

def serve(listener, label):
    receipt = dest/(label.lower()+'.received.txt')
    while not stop.is_set():
        try:
            conn, _ = listener.accept()
        except socket.timeout:
            continue
        except OSError:
            return
        conn.settimeout(.2)
        pending = b''
        telnet_state = 'text'
        try:
            conn.sendall((label+':: fictional welcome \u03bb\U0001f409\r\n').encode())
            conn.sendall((label+':: \x1b[1z<send href="look fixture">Synthetic command link</send>\r\n').encode())
            if label == 'Alpha' and not a.isolated_desktop:
                conn.sendall(b'\x1b[1z<image url="https://raw.githubusercontent.com/Balerion300/mushclient/codex/inline-static-images/tests/fixture-scene.png">\r\n')
            while not stop.is_set():
                command_file = dest/(label.lower()+'.control')
                if command_file.exists() and command_file.read_text().strip() == 'disconnect':
                    break
                try:
                    chunk = conn.recv(4096)
                    if not chunk:
                        break
                    clean = bytearray()
                    for byte in chunk:
                        if telnet_state == 'text':
                            if byte == 255:telnet_state = 'iac'
                            else:clean.append(byte)
                        elif telnet_state == 'iac':
                            if byte in (251,252,253,254):telnet_state = 'option'
                            elif byte == 250:telnet_state = 'sub'
                            else:
                                if byte == 255:clean.append(byte)
                                telnet_state = 'text'
                        elif telnet_state == 'option':telnet_state = 'text'
                        elif telnet_state == 'sub':
                            if byte == 255:telnet_state = 'sub_iac'
                        elif telnet_state == 'sub_iac':telnet_state = 'text' if byte == 240 else 'sub'
                    pending += clean
                except socket.timeout:
                    continue
                while b'\n' in pending:
                    raw, pending = pending.split(b'\n', 1)
                    command = raw.decode('utf-8', errors='replace').rstrip('\r')
                    with receipt.open('a', encoding='utf-8') as f:
                        f.write(command+'\n')
                    conn.sendall((label+':: ECHO '+command+'\r\n').encode())
        except OSError:
            pass
        finally:
            conn.close()

for index, label in enumerate(('Alpha', 'Beta')):
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(2)
    listener.settimeout(.2)
    listeners.append(listener)
    world_id = ('11' if index == 0 else '22')*12
    world = dest/(label.lower()+'.mcl')
    world.write_text('<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE muclient><muclient><world name="Workspace acceptance '+label+'" id="'+world_id+'" site="127.0.0.1" port="'+str(listener.getsockname()[1])+'" utf_8="y" use_mxp="2" detect_pueblo="n" enable_scripts="y" enable_triggers="y" enable_aliases="y" script_language="Lua" output_font_name="Consolas" output_font_height="11"/><include name='+quoteattr(str(plugin))+' plugin="y"/></muclient>', encoding='utf-8')
    worlds.append(str(world))
    pattern = '^'+label+'::'+ ('|fixture-scene\\.png' if label == 'Alpha' else '')
    db.execute('INSERT INTO workspace_route_v1 VALUES(?,?,?,?,?)', (world_id, label+' Chat', pattern, 1, 0))
    db.execute('INSERT INTO workspace_input_prefix_v1 VALUES(?,?,?)', (world_id, label+' Chat', '+'+label.lower()+' '))
    threading.Thread(target=serve, args=(listener,label), daemon=True).start()

for key, value in {'database_version':1, 'First time':0, 'Version':507, 'Tip_StartUp':1}.items():
    db.execute('INSERT INTO control VALUES(?,?)', (key,value))
preferences={'WorldList':'*'.join(worlds)}
if not a.default_settings:preferences.update({'EnableSpellCheck':'0', 'AutoConnectWorlds':'1', 'OpenWorldsMaximised':'1', 'ConfirmBeforeClosingMushclient':'0', 'ConfirmBeforeClosingWorld':'0'})
for key, value in preferences.items():
    db.execute('INSERT INTO prefs VALUES(?,?)', (key,value))
db.commit()
if a.tabbed_worlds:
    db.execute('CREATE TABLE workspace_window_mode_v1(slot INTEGER PRIMARY KEY,tabbed INTEGER NOT NULL)')
    db.execute('INSERT INTO workspace_window_mode_v1 VALUES(1,1)')
    db.commit()
db.close()
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
desktop_handle = None
desktop_name = None
desktop_api = None
if a.isolated_desktop:
    # STARTF_USESHOWWINDOW alone is insufficient: MFC and fixture actions can
    # subsequently show or activate windows. A separate desktop confines all
    # those windows and focus changes, including dialogs and reopened worlds.
    from ctypes import wintypes
    desktop_api = ctypes.WinDLL('user32', use_last_error=True)
    desktop_api.CreateDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
    desktop_api.CreateDesktopW.restype = wintypes.HANDLE
    desktop_api.CloseDesktop.argtypes = [wintypes.HANDLE]
    desktop_api.CloseDesktop.restype = wintypes.BOOL
    desktop_name = 'ValyriaQA-' + uuid.uuid4().hex
    # No DESKTOP_SWITCHDESKTOP right, and no SwitchDesktop call anywhere.
    desktop_handle = desktop_api.CreateDesktopW(desktop_name, None, None, 0, 0x00ff, None)
    if not desktop_handle:
        raise ctypes.WinError(ctypes.get_last_error())
    from private_desktop_process import PrivateDesktopProcess
    proof_file = dest/'desktop-preflight.json'
    proof = PrivateDesktopProcess([sys.executable,str(Path(__file__).with_name('inspect_fixture_desktop.py')),'0',str(proof_file)], desktop_name, dest, os.environ.copy())
    try:
        proof.wait(timeout=8)
        assert proof.returncode == 0 and json.loads(proof_file.read_text())['desktop'] == desktop_name, 'Private desktop preflight failed; candidate will not launch'
    except BaseException:
        desktop_api.CloseDesktop(desktop_handle)
        raise
    finally:
        if proof.poll() is None: proof.terminate();proof.wait(timeout=3)
        proof.close()
environment = os.environ.copy()
if a.focus_trace or a.main_composer_test: environment['VALYRIA_FOCUS_TRACE'] = '1'
try:
    if desktop_handle:
        client = PrivateDesktopProcess([str(dest/'MUSHclient.exe'), '/noregister'], desktop_name, dest, environment)
    else:
        client = subprocess.Popen([str(dest/'MUSHclient.exe'), '/noregister'], cwd=dest, startupinfo=startup, env=environment)
except BaseException:
    if desktop_handle: desktop_api.CloseDesktop(desktop_handle)
    raise
print(json.dumps({'fixture':str(dest), 'candidate_pid':client.pid, 'candidate_exe':str(dest/'MUSHclient.exe'), 'isolated_desktop':desktop_name}, indent=2), flush=True)
started = time.monotonic()
try:
    while time.monotonic()-started < a.duration and not stop.is_set():
        control = dest/'fixture.control'
        if control.exists() and control.read_text().strip() == 'stop':
            break
        time.sleep(.2)
finally:
    stop.set()
    for listener in listeners:
        listener.close()
    if desktop_handle:
        # Diagnostics are optional evidence, never a reason to skip cleanup.
        try:
            inspected = PrivateDesktopProcess([sys.executable, str(Path(__file__).with_name('inspect_fixture_desktop.py')), str(client.pid),str(dest/'isolated-desktop-inspection.json')], desktop_name, dest, os.environ.copy())
            try: inspected.wait(timeout=8)
            finally:
                if inspected.poll() is None: inspected.terminate();inspected.wait(timeout=3)
                inspected.close()
        except BaseException as error:
            (dest/'desktop-diagnostic-error.txt').write_text(str(error), encoding='utf-8')
    if client.poll() is None:
        client.terminate()
        client.wait(timeout=10)
        terminated_by_fixture = True
    else:
        terminated_by_fixture = False
    (dest/'candidate-exit.json').write_text(json.dumps({'exit_code':client.returncode, 'terminated_by_fixture':terminated_by_fixture}), encoding='utf-8')
    if desktop_handle:
        client.close()
        desktop_api.CloseDesktop(desktop_handle)
    print('Isolated fixture stopped; evidence retained at '+str(dest), flush=True)
