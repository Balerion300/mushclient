"""Isolated actual-client pipeline fixture. Uses no installed worlds/credentials.

Preparation copies only compiled exe, Lua DLL, locale DLL, independent plugin.
Socket binds IPv4 loopback only; synthetic text and controlled fragmentation.
The fixture remains running for UI grant checks; write plugin.control.txt commands.
"""
import argparse
import base64
import hashlib
import json
import pathlib
import re
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import threading
import time
from xml.sax.saxutils import quoteattr

ROOT=pathlib.Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--exe',type=pathlib.Path,default=ROOT/'WinRel/MUSHclient.exe')
parser.add_argument('--runtime',type=pathlib.Path,default=ROOT/'.portable-full-mxp-recovery')
parser.add_argument('--duration',type=int,default=300)
parser.add_argument('--private-editor-response',action='store_true',help='Reply only to the exact fictional #1/TEST read request')
args=parser.parse_args()
destination=pathlib.Path(tempfile.mkdtemp(prefix='mushclient-workspace-integration-'))
(destination/'locale').mkdir()
shutil.copy2(args.exe,destination/'MUSHclient.exe')
shutil.copy2(ROOT/'WinRel/MUSHclientAgent.exe',destination/'MUSHclientAgent.exe')
shutil.copy2(args.runtime/'lua5.1.dll',destination/'lua5.1.dll')
shutil.copy2(args.runtime/'locale/en.dll',destination/'locale/en.dll')
shutil.copy2(ROOT/'third_party/scintilla/bin/Scintilla.dll',destination/'Scintilla.dll')
shutil.copytree(ROOT/'third_party/pennmush-reference',destination/'pennmush-reference')
plugin=destination/'plugin.xml';shutil.copy2(ROOT/'tests/workspace/integration_plugin.xml',plugin)
child_plugin=destination/'child.xml';shutil.copy2(ROOT/'tests/workspace/integration_child_omit.xml',child_plugin)
server=socket.socket();server.bind(('127.0.0.1',0));server.listen(1);port=server.getsockname()[1]
world=destination/'fixture.mcl'
world.write_text('<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE muclient><muclient><world name="Workspace isolated integration" site="127.0.0.1" port="'+str(port)+'" enable_scripts="y" enable_triggers="y" script_language="Lua" output_font_name="Consolas" output_font_height="11" wrap_column="20" max_output_lines="5000"/><include name='+quoteattr(str(plugin))+' plugin="y"/><include name='+quoteattr(str(child_plugin))+' plugin="y"/></muclient>',encoding='utf-8')
db=sqlite3.connect(destination/'mushclient_prefs.sqlite')
db.executescript('CREATE TABLE control(name TEXT PRIMARY KEY,value INT);CREATE TABLE prefs(name TEXT PRIMARY KEY,value TEXT);CREATE TABLE worlds(name TEXT PRIMARY KEY,value TEXT);')
for name,value in {'database_version':1,'First time':0,'Version':507,'Tip_StartUp':1}.items():db.execute('INSERT INTO control VALUES(?,?)',(name,value))
for name,value in {'WorldList':str(world),'EnableSpellCheck':'0','AutoConnectWorlds':'1','OpenWorldsMaximised':'1','ConfirmBeforeClosingMushclient':'0','ConfirmBeforeClosingWorld':'0'}.items():db.execute('INSERT INTO prefs VALUES(?,?)',(name,value))
db.commit();db.close()
stop=threading.Event()
def serve():
 connection,address=server.accept();connection.settimeout(.2)
 fragments=[b'WS_NORMAL completed\r\n',b'WS_WRAP '+b'w'*90,b'w'*90+b'_END\r\n',b'WS_REENTRANT callback\r\n',b'WS_OMIT private omitted\r\n']
 for fragment in fragments:connection.sendall(fragment);time.sleep(.1)
 (destination/'fixture-sent.txt').write_text('synthetic fragments delivered\n')
 with (destination/'received.bin').open('wb') as received:
  pending=b''
  while not stop.is_set():
   try:
    data=connection.recv(4096)
    if not data:break
    received.write(data);received.flush()
    if args.private_editor_response:
     pending+=data
     while b'\n' in pending:
      command,pending=pending.split(b'\n',1)
      match=re.fullmatch(rb'think ([0-9a-f]{32}) \[encode64\(get\(#1/TEST\)\)\]\r?',command)
      if match:
       source=b'&TEST #1=FIXTURE_PRIVATE_CODE_OWNED_REPLY'
       encoded=base64.b64encode(source)
       connection.sendall(match.group(1)+b' '+encoded+b'\r\n')
       (destination/'private-read.receipt.json').write_text(json.dumps({'read_requests':1,'response_sent':True,'nonce':match.group(1).decode(),'encoded':encoded.decode(),'source':source.decode()}))
       connection.sendall(b'f'*32+b' WRONG_NONCE_ORDINARY\r\n')
   except socket.timeout:pass
 connection.close()
threading.Thread(target=serve,daemon=True).start()
client=subprocess.Popen([str(destination/'MUSHclient.exe'),'/noregister'],cwd=destination)
manifest={'destination':str(destination),'pid':client.pid,'port':port,'exe_sha256':hashlib.sha256((destination/'MUSHclient.exe').read_bytes()).hexdigest(),'plugin_path':str(plugin)}
(destination/'manifest.json').write_text(json.dumps(manifest,indent=2))
print(json.dumps(manifest),flush=True)
started=time.monotonic()
try:
 while client.poll() is None and time.monotonic()-started<args.duration:time.sleep(.2)
finally:
 stop.set();server.close()
 if client.poll() is None:client.terminate();client.wait(timeout=10)
 print('Fixture evidence retained at '+str(destination),flush=True)
