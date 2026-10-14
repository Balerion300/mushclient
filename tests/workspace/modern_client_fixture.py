"""Two fictional loopback worlds for modern tab/input UI acceptance.

Copies only clean build/runtime components into a fresh temporary directory.
No real credentials, installed profiles, production connections or player output.
The CLI prints the exact candidate PID/path; use normal UI close during acceptance.
Writing 'stop' to fixture.control stops only this fixture and its candidate process.
"""
import argparse
import json
from pathlib import Path
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import threading
import time
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, default=ROOT/'WinRel/MUSHclient.exe')
p.add_argument('--runtime', type=Path, default=ROOT/'.distribution-source/extracted')
p.add_argument('--destination', type=Path)
p.add_argument('--duration', type=int, default=1800)
a = p.parse_args()
dest = a.destination or Path(tempfile.mkdtemp(prefix='mush-modern-acceptance-'))
dest.mkdir(exist_ok=True)
for name in ('locale', 'pennmush-reference'):
    source = a.runtime/name if name == 'locale' else ROOT/'third_party'/name
    shutil.copytree(source, dest/name)
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
            if label == 'Alpha':
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
for key, value in {'WorldList':'*'.join(worlds), 'EnableSpellCheck':'0', 'AutoConnectWorlds':'1', 'OpenWorldsMaximised':'1', 'ConfirmBeforeClosingMushclient':'0', 'ConfirmBeforeClosingWorld':'0'}.items():
    db.execute('INSERT INTO prefs VALUES(?,?)', (key,value))
db.commit()
db.close()
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
client = subprocess.Popen([str(dest/'MUSHclient.exe'), '/noregister'], cwd=dest, startupinfo=startup)
print(json.dumps({'fixture':str(dest), 'candidate_pid':client.pid, 'candidate_exe':str(dest/'MUSHclient.exe')}, indent=2), flush=True)
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
    if client.poll() is None:
        client.terminate()
        client.wait(timeout=10)
    print('Isolated fixture stopped; evidence retained at '+str(dest), flush=True)
