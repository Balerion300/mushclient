"""Final-package agent acceptance against one fictional IPv4 loopback world.

Prepare first without opening a client; --run waits for GUI ownership handoff.
No profiles, accounts, installed clients, model providers or remote URLs are used.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import threading
import time
from xml.sax.saxutils import quoteattr

PACKAGE = Path(r'C:\MUSH\Builds\MUSHclient-review-fixes-20261014')
PLUGIN = r'''<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE muclient><muclient>
<plugin name="Agent_Acceptance_Fictional" author="MUSHclient contributors"
 id="aece20101400000000000001" language="Lua" version="1.0" requires="5.07"/>
<script><![CDATA[
local enabled=false
local base=GetPluginInfo(GetPluginID(),6)
local function record(text)
 local f=assert(io.open(base..'.receipt.txt','a'));f:write(text,'\n');f:close()
end
local function marker_count(marker)
 local count=0
 for i=1,GetLinesInBufferCount() do
  if GetLineInfo(i,1):find(marker,1,true) then count=count+1 end
 end
 return count
end
function OnPluginInstall()
 enabled=WorldName()=='Agent acceptance fictional' and GetAlphaOption('site')=='127.0.0.1'
 if enabled then record('READY '..base) end
end
function OnPluginLineReceived(line)
 if not enabled then return true end
 if line:match('^%x+ JlRFU1QgIzE9RklYVFVSRV9QUklWQVRFX0NPREVfT1dORURfUkVQTFk=$') then
  record('PRIVATE_REPLY_PLUGIN_OMITTED');return false
 end
 return true
end
function DeleteOutsideSendToScript()
 assert(enabled)
 local before=GetLinesInBufferCount()
 assert(marker_count('DELETE_TAIL_MARKER')==1,'expected one tail before delete')
 DeleteLines(1)
 local after=GetLinesInBufferCount()
 local remaining=marker_count('DELETE_TAIL_MARKER')
 record('DELETE before='..before..' after='..after..' remaining_tail='..remaining..
        ' keep_anchor='..marker_count('DELETE_KEEP_ANCHOR'))
 assert(after<before and remaining==0,'actual native tail was not deleted')
end
function OnPluginTick()
 if not enabled then return end
 local f=io.open(base..'.control.txt','r');if not f then return end
 local command=f:read('*a');f:close();os.remove(base..'.control.txt')
 local ok,err=pcall(function()
  if command=='prime-delete' then
   Simulate('DELETE_KEEP_ANCHOR\r\nDELETE_TAIL_MARKER\r\n')
   record('DELETE_PRIMED count='..GetLinesInBufferCount())
  elseif command=='delete' then
   local previous=GetAlphaOption('script_prefix')
   assert(SetAlphaOption('script_prefix','/')==0)
   local status=Execute('/CallPlugin("'..GetPluginID()..'", "DeleteOutsideSendToScript", "")')
   assert(SetAlphaOption('script_prefix',previous)==0)
   record('DELETE_EXECUTE status='..status)
  elseif command=='dump' then
   record('BUFFER_BEGIN count='..GetLinesInBufferCount())
   for i=1,GetLinesInBufferCount() do record('BUFFER '..i..' '..GetLineInfo(i,1)) end
   record('BUFFER_END')
  elseif command=='disconnect' then Disconnect();record('DISCONNECTED')
  else error('unknown fictional fixture command') end
 end)
 if not ok then record('ERROR '..tostring(err)) end
end
]]></script></muclient>'''


def prepare():
    destination = Path(tempfile.mkdtemp(prefix='mush-agent-final-acceptance-'))
    shutil.copytree(PACKAGE, destination, dirs_exist_ok=True)
    plugin = destination/'agent-acceptance-plugin.xml'
    plugin.write_text(PLUGIN, encoding='utf-8')
    manifest = {
        'fixture': str(destination),
        'package': str(PACKAGE),
        'exe_sha256': hashlib.sha256((destination/'MUSHclient.exe').read_bytes()).hexdigest(),
        'plugin': str(plugin),
        'boundaries': 'fictional loopback only; no personal worlds or remote model',
    }
    (destination/'agent-acceptance-manifest.json').write_text(json.dumps(manifest, indent=2))
    print(json.dumps(manifest, indent=2), flush=True)


def run(destination):
    plugin = destination/'agent-acceptance-plugin.xml'
    assert plugin.read_text(encoding='utf-8') == PLUGIN
    server = socket.socket()
    server.bind(('127.0.0.1', 0))
    server.listen(1)
    server.settimeout(.2)
    stop = threading.Event()
    world = destination/'agent-acceptance.mcl'
    world.write_text('<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE muclient><muclient>'
        '<world name="Agent acceptance fictional" id="aece20101400000000000002" '
        'site="127.0.0.1" port="'+str(server.getsockname()[1])+'" utf_8="y" '
        'enable_scripts="y" enable_triggers="y" script_language="Lua" '
        'output_font_name="Consolas" output_font_height="11" wrap_column="150" '
        'max_output_lines="5000"/><include name='+quoteattr(str(plugin))+
        ' plugin="y"/></muclient>', encoding='utf-8')
    db = sqlite3.connect(destination/'mushclient_prefs.sqlite')
    db.executescript('CREATE TABLE control(name TEXT PRIMARY KEY,value INT);'
                    'CREATE TABLE prefs(name TEXT PRIMARY KEY,value TEXT);'
                    'CREATE TABLE worlds(name TEXT PRIMARY KEY,value TEXT);'
                    'CREATE TABLE workspace_route_v1(world_id TEXT,name TEXT,pattern TEXT,regex INTEGER,omit_main INTEGER,PRIMARY KEY(world_id,name,pattern,regex));')
    for name, value in {'database_version':1, 'First time':0, 'Version':507, 'Tip_StartUp':1}.items():
        db.execute('INSERT INTO control VALUES(?,?)', (name,value))
    for name, value in {'WorldList':str(world), 'EnableSpellCheck':'0',
        'AutoConnectWorlds':'1', 'OpenWorldsMaximised':'1',
        'ConfirmBeforeClosingMushclient':'0', 'ConfirmBeforeClosingWorld':'0'}.items():
        db.execute('INSERT INTO prefs VALUES(?,?)', (name,value))
    db.execute('INSERT INTO workspace_route_v1 VALUES(?,?,?,?,?)',
               ('aece20101400000000000002', 'PrivateReplySentinel', '^[0-9a-f]{32} ', 1, 0))
    db.commit()
    db.close()

    def serve():
        connection = None
        while not stop.is_set():
            try:
                connection, _ = server.accept()
                break
            except socket.timeout:
                continue
        if connection is None:
            return
        connection.settimeout(.15)
        connection.sendall(b'AGENT_ACCEPTANCE_WELCOME fictional loopback only\r\n'+
                           b'0'*32+b' SENTINEL_READY\r\n')
        pending = b''
        try:
            with (destination/'server-received.bin').open('wb') as receipt:
                while not stop.is_set():
                    send_file = destination/'server-send.txt'
                    if send_file.exists():
                        data = send_file.read_text(encoding='utf-8')
                        send_file.unlink()
                        connection.sendall(data.replace('\n','\r\n').encode('utf-8'))
                    try:
                        data = connection.recv(4096)
                        if not data:
                            break
                    except socket.timeout:
                        continue
                    receipt.write(data)
                    receipt.flush()
                    pending += data
                    while b'\n' in pending:
                        command, pending = pending.split(b'\n',1)
                        match = re.fullmatch(rb'think ([0-9a-f]{32}) \[encode64\(get\(#1/TEST\)\)\]\r?',command)
                        if match:
                            source = b'&TEST #1=FIXTURE_PRIVATE_CODE_OWNED_REPLY'
                            encoded = base64.b64encode(source)
                            connection.sendall(match.group(1)+b' '+encoded+b'\r\n')
                            (destination/'private-read.receipt.json').write_text(json.dumps({
                                'nonce':match.group(1).decode(), 'encoded':encoded.decode(),
                                'source':source.decode(), 'response_sent':True}, indent=2))
                            connection.sendall(b'f'*32+b' WRONG_NONCE_ORDINARY\r\n')
        except OSError:
            pass
        finally:
            connection.close()

    threading.Thread(target=serve, daemon=True).start()
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    client = subprocess.Popen([str(destination/'MUSHclient.exe'), '/noregister'],
                              cwd=destination, startupinfo=startup)
    print(json.dumps({'fixture':str(destination),'pid':client.pid,
                      'port':server.getsockname()[1]}), flush=True)
    started = time.monotonic()
    try:
        while client.poll() is None and time.monotonic()-started < 3600:
            control = destination/'fixture.control'
            if control.exists() and control.read_text().strip() == 'stop':
                break
            time.sleep(.2)
    finally:
        stop.set()
        server.close()
        if client.poll() is None:
            client.terminate()
            client.wait(timeout=10)
        print('Fictional fixture stopped; evidence retained at '+str(destination), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--prepare', action='store_true')
    parser.add_argument('--run', type=Path)
    arguments = parser.parse_args()
    if arguments.prepare:
        prepare()
    else:
        assert arguments.run
        run(arguments.run)
