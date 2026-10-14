"""Actual-client first-login ordering regression, using fictional loopback worlds.

Copies a clean runtime into Temp; never reads installed worlds or credentials.
TLS uses a freshly generated, localhost-only certificate. Requires cryptography.
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import socket
import sqlite3
import ssl
import subprocess
import tempfile
import threading
import time
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parents[1]
IMAGE = 'https://raw.githubusercontent.com/Balerion300/mushclient/codex/inline-static-images/tests/fixture-scene.png'


def certificate(directory):
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.x509.oid import NameOID
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'localhost')])
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
            .public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now-datetime.timedelta(minutes=1))
            .not_valid_after(now+datetime.timedelta(days=1)).sign(key, hashes.SHA256()))
    (directory/'key.pem').write_bytes(key.private_bytes(serialization.Encoding.PEM,
        serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    (directory/'cert.pem').write_bytes(cert.public_bytes(serialization.Encoding.PEM))
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(directory/'cert.pem', directory/'key.pem')
    return context


def run_case(exe, root, name, greeting, detect=True, tls=False):
    directory = root/name
    directory.mkdir()
    runtime = ROOT/'.distribution-source/extracted'
    shutil.copytree(runtime/'locale', directory/'locale')
    for source in (exe, runtime/'lua5.1.dll', ROOT/'third_party/scintilla/bin/Scintilla.dll'):
        shutil.copy2(source, directory/source.name)
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    listener.settimeout(12)
    context = certificate(directory) if tls else None
    receipt = {'commands': [], 'rich_initial': False, 'errors': []}
    stopped = threading.Event()

    def serve():
        try:
            connection, _ = listener.accept()
            with connection:
                if context:
                    connection = context.wrap_socket(connection, server_side=True)
                with connection:
                    connection.settimeout(.025)
                    started = time.monotonic()
                    pending = bytearray()
                    state = 'text'
                    sent = False
                    negotiated = False
                    while not stopped.is_set():
                        elapsed = time.monotonic()-started
                        if name == 'disconnect-before-login' and elapsed >= .1:
                            return
                        if greeting is not None and not sent and elapsed >= greeting:
                            connection.sendall(b'This world is Pueblo 1.10 enhanced.\r\n')
                            sent = True
                        try:
                            data = connection.recv(4096)
                        except socket.timeout:
                            continue
                        if not data:
                            break
                        for byte in data:
                            if state == 'text':
                                if byte == 255:
                                    state = 'iac'
                                else:
                                    pending.append(byte)
                            elif state == 'iac':
                                if byte in (251, 252, 253, 254):
                                    state = 'option'
                                elif byte == 250:
                                    state = 'sub'
                                else:
                                    state = 'text'
                            elif state == 'option':
                                state = 'text'
                            elif state == 'sub':
                                if byte == 255:
                                    state = 'sub-iac'
                            elif state == 'sub-iac':
                                state = 'text' if byte == 240 else 'sub'
                        while b'\n' in pending:
                            line, _, tail = pending.partition(b'\n')
                            pending = bytearray(tail)
                            command = line.decode('utf-8', errors='replace').rstrip('\r')
                            receipt['commands'].append({'command': command, 'seconds': elapsed})
                            if command.startswith('PUEBLOCLIENT'):
                                negotiated = True
                                connection.sendall(b'</xch_mudtext>')
                            elif command == 'connect Fictional synthetic-only':
                                receipt['rich_initial'] = negotiated
                                if negotiated:
                                    connection.sendall(('<img src="'+IMAGE+'">\r\n'
                                        '<a xch_cmd="probe-click">FIRST_LOGIN_LINK</a>\r\n').encode())
                                else:
                                    connection.sendall(b'FIRST_LOGIN_PLAIN\r\n')
                                connection.sendall(b'FIRST_LOGIN_END\r\n')
        except (OSError, ssl.SSLError) as error:
            if not stopped.is_set():
                receipt['errors'].append(str(error))

    plugin = directory/'probe.xml'
    plugin.write_text('''<?xml version="1.0"?><!DOCTYPE muclient><muclient>
<plugin name="Pueblo_Login_Probe" id="310210ab310210ab310210ab" language="Lua" version="1.0"/>
<script><![CDATA[
local ticks,connected=0,0
function OnPluginConnect() connected=connected+1 end
function OnPluginTick()
 ticks=ticks+1
 if ticks<100 then return end
 local image,link,ended=false,false,false
 for line=1,GetLinesInBufferCount() do
  image=image or GetLineInfo(line,14)==true
  ended=ended or (GetLineInfo(line,1) or ''):find('FIRST_LOGIN_END',1,true)~=nil
  for style=1,(GetLineInfo(line,11) or 0) do
   link=link or GetStyleInfo(line,style,5)=='probe-click'
  end
 end
 local f=assert(io.open(GetPluginInfo(GetPluginID(),6)..'.receipt','w'))
 f:write(tostring(image),' ',tostring(link),' ',tostring(ended),' ',connected);f:close()
end
]]></script></muclient>''', encoding='utf-8')
    world = directory/'fictional.mcl'
    world.write_text('<?xml version="1.0"?><!DOCTYPE muclient><muclient><world '
        'name="Pueblo first login fictional" site="127.0.0.1" port="'+str(listener.getsockname()[1])+'" '
        'connect_method="1" player="Fictional" password="synthetic-only" use_mxp="1" '
        'detect_pueblo="'+('y' if detect else 'n')+'" use_ssl="'+('y' if tls else 'n')+'" '
        'enable_scripts="y" script_language="Lua"/><include name='+quoteattr(str(plugin))+' plugin="y"/></muclient>',
        encoding='utf-8')
    with sqlite3.connect(directory/'mushclient_prefs.sqlite') as db:
        db.executescript('CREATE TABLE control(name TEXT PRIMARY KEY,value INT);'
            'CREATE TABLE prefs(name TEXT PRIMARY KEY,value TEXT);CREATE TABLE worlds(name TEXT PRIMARY KEY,value TEXT);')
        db.executemany('INSERT INTO control VALUES(?,?)', [('database_version',1),('First time',0),('Version',507),('Tip_StartUp',1)])
        db.executemany('INSERT INTO prefs VALUES(?,?)', [('WorldList',str(world)),('EnableSpellCheck','0'),
            ('AutoConnectWorlds','1'),('ConfirmBeforeClosingMushclient','0'),('ConfirmBeforeClosingWorld','0')])
    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    client = subprocess.Popen([str(directory/exe.name), '/noregister'], cwd=directory, startupinfo=startup)
    try:
        deadline = time.monotonic()+15
        result_file = Path(str(plugin)+'.receipt')
        while time.monotonic() < deadline and not result_file.exists() and client.poll() is None:
            time.sleep(.1)
        receipt['client_receipt'] = result_file.read_text() if result_file.exists() else 'missing'
        expected_rich = detect and greeting is not None and greeting < 1
        logins = [item for item in receipt['commands'] if item['command'].startswith('connect ')]
        if name == 'disconnect-before-login':
            receipt['pass'] = (not receipt['errors'] and not logins and
                receipt['client_receipt']=='false false false 0')
        else:
            receipt['pass'] = (not receipt['errors'] and len(logins)==1 and
                receipt['rich_initial']==expected_rich and
                receipt['client_receipt']==('true true true 1' if expected_rich else 'false false true 1'))
            if name == 'silent-fallback' and logins:
                receipt['pass'] &= .9 <= logins[0]['seconds'] <= 3
        receipt['name'] = name
        return receipt
    finally:
        stopped.set()
        listener.close()
        client.terminate()
        client.wait(timeout=5)
        thread.join(timeout=2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=ROOT/'WinRel/MUSHclient.exe')
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix='mush-pueblo-login-'))
    cases = [('telnet-immediate',0,True,False), ('telnet-delayed',.2,True,False),
             ('tls-delayed',.2,True,True), ('silent-fallback',None,True,False),
             ('detection-disabled',.2,False,False), ('telnet-late-greeting',.8,True,False),
             ('disconnect-before-login',None,True,False)]
    results = [run_case(args.exe.resolve(),root,*case) for case in cases]
    report = {'directory':str(root),'sha256':hashlib.sha256(args.exe.read_bytes()).hexdigest(),'cases':results}
    (root/'results.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report,indent=2))
    raise SystemExit(0 if all(item['pass'] for item in results) else 1)
