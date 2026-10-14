"""Real-client scripted logging regression using only fictional loopback worlds.

An exact candidate executable is copied into fresh Temp runtimes. A guarded
driver invokes the same LogSession command as the menu; local control files
keep fixture commands out of server traffic. No installed profiles are read.
"""
import argparse
import hashlib
import json
from pathlib import Path
import queue
import re
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import threading
import time
import xml.etree.ElementTree as ET
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parents[1]
HELPER_ID = '7c1e480cf1d24cb28f9a1062'
DRIVER_ID = '4e78b729dbcd491c9b1d8402'
HEADER = ['=== Fictional Calendar ===', 'Day: 18', 'Month: 04', 'Year: 178', '=== End Calendar ===']

DRIVER = r'''
local enabled=false
local base=GetPluginInfo(GetPluginID(),6)
local function record(text)
 local f=assert(io.open(base..'.receipt','a'));f:write(text,'\n');f:close()
end
local function configuration_snapshot()
 local value=GetPluginVariable('HELPER_ID','scripted_log_configuration')
 local result=GetPluginVariable('HELPER_ID','scripted_log_configuration_result')
 local f=assert(io.open(base..'.configuration','wb'));f:write(value or '');f:close()
 f=assert(io.open(base..'.configuration-result','wb'));f:write(result or '');f:close()
 return result or ''
end
function OnPluginInstall()
 enabled=GetAlphaOption('site')=='127.0.0.1' and WorldName():find('Logging Fictional ',1,true)==1
 if enabled then SetVariable('fixture_scope','driver');record('install') end
end
function OnPluginConnect() if enabled then record('connect') end end
function OnPluginDisconnect() if enabled then record('disconnect') end end
function OnPluginLineReceived(text)
 if enabled then
  local f=assert(io.open(base..'.lines','a'));f:write(text,'\n');f:close()
 end
 return true
end
function FixtureStart(action)
 if not enabled then return end
 record('hook:'..tostring(action))
 assert(GetVariable('fixture_scope')=='driver','Plugin hook lost plugin context')
 record('recursive-start:'..tostring(DoCommand('LogSession'))) -- Must not queue another start/close.
 record('hook-open:'..tostring(OpenLogExclusive('plugin-hook.txt')))
 WriteLog('PLUGIN_HOOK_DATA')
end
function OnPluginTick()
 if not enabled then return end
 local control=io.open(base..'.control','r')
 if control then
  local command=control:read('*a');control:close();os.remove(base..'.control')
  command=command:gsub('[\r\n]+$','')
  if command=='start' then Activate();record('start:'..tostring(DoCommand('LogSession')))
  elseif command=='activate' then Activate();record('activate:0')
  elseif command=='cancel' then record('cancel:'..tostring(CallPlugin('HELPER_ID','CancelLog','cancel')))
  elseif command=='reconnect' then record('reconnect:'..tostring(Connect()))
  elseif command=='close' then record('close:'..tostring(CloseLog()))
  elseif command=='flush' then record('flush:'..tostring(FlushLog()))
  elseif command=='exclusive-existing' then record('exclusive-existing:'..tostring(OpenLogExclusive('prior.txt')))
  elseif command=='exclusive-new' then record('exclusive-new:'..tostring(OpenLogExclusive('exclusive-new.txt')));WriteLog('EXCLUSIVE_DATA')
  elseif command=='exclusive-active' then record('exclusive-active:'..tostring(OpenLogExclusive('must-not-exist.txt')))
  elseif command=='exclusive-missing-parent' then record('exclusive-missing-parent:'..tostring(OpenLogExclusive('missing-parent/no-file.txt')))
  elseif command=='configure' then
   local f=assert(io.open(base..'.settings','rb'));local settings=f:read('*a');f:close()
   local result=CallPlugin('HELPER_ID','ConfigureLogNames',settings)
   record('configure:'..tostring(result)..':'..configuration_snapshot())
  elseif command=='reload-helper' then
   local result=ReloadPlugin('HELPER_ID');configuration_snapshot()
   record('reload-helper:'..tostring(result))
  end
 end
 local f=assert(io.open(base..'.status','w'))
 f:write(tostring(IsConnected()),'|',tostring(IsLogOpen()),'|',tostring(GetInfo(51)))
 f:close()
end
'''.replace('HELPER_ID', HELPER_ID)


def wait_until(predicate, timeout=8):
    deadline = time.monotonic()+timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(.025)
    return False


class Endpoint:
    def __init__(self):
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', 0))
        self.listener.listen(1)
        self.listener.settimeout(.1)
        self.port = self.listener.getsockname()[1]
        self.commands = []
        self.errors = []
        self.outgoing = queue.Queue()
        self.stop = threading.Event()
        self.connected = threading.Event()
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()

    def serve(self):
        try:
            while not self.stop.is_set():
                try:
                    connection, _ = self.listener.accept()
                    break
                except socket.timeout:
                    pass
            else:
                return
            with connection:
                connection.settimeout(.025)
                self.connected.set()
                pending, state = bytearray(), 'text'
                while not self.stop.is_set():
                    try:
                        outgoing = self.outgoing.get_nowait()
                        if outgoing is None:
                            return
                        connection.sendall(outgoing)
                    except queue.Empty:
                        pass
                    try:
                        data = connection.recv(8192)
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
                            state = 'option' if byte in (251,252,253,254) else ('sub' if byte == 250 else 'text')
                        elif state == 'option':
                            state = 'text'
                        elif state == 'sub':
                            if byte == 255:
                                state = 'sub-iac'
                        elif state == 'sub-iac':
                            state = 'text' if byte == 240 else 'sub'
                    while b'\n' in pending:
                        raw, _, rest = pending.partition(b'\n')
                        pending = bytearray(rest)
                        self.commands.append(raw.rstrip(b'\r').decode('utf-8', 'replace'))
        except OSError as error:
            if not self.stop.is_set():
                self.errors.append(str(error))

    def send(self, text):
        self.outgoing.put(text.encode('utf-8') if isinstance(text, str) else text)

    def header(self, lines=HEADER, split=True, tail=''):
        data = ('\x1b[36m'+'\r\n'.join(lines)+'\x1b[0m\r\n'+tail).encode()
        if split:
            # Splits an ANSI escape, a field label and the terminating newline.
            for offset in (2, 23, 57, len(data)-1):
                fragment, data = data[:offset], data[offset:]
                if fragment:
                    self.send(fragment)
                    time.sleep(.035)
        if data:
            self.send(data)

    def close(self):
        self.stop.set()
        self.listener.close()
        self.thread.join(2)

    def reaccept(self):
        self.thread.join(2)
        assert not self.thread.is_alive(), 'Previous synthetic connection did not close'
        self.connected.clear()
        self.thread = threading.Thread(target=self.serve, daemon=True)
        self.thread.start()


class Fixture:
    def __init__(self, directory, exe, helper, labels=('Alpha',), hook='helper'):
        self.directory, self.endpoints, self.drivers = directory, {}, {}
        directory.mkdir()
        runtime = ROOT/'.distribution-source/extracted'
        shutil.copytree(runtime/'locale', directory/'locale')
        for source in (exe, runtime/'lua5.1.dll', ROOT/'third_party/scintilla/bin/Scintilla.dll'):
            shutil.copy2(source, directory/source.name)
        (directory/'lua').mkdir()
        # SaveState stays entirely inside this copied runtime, including the
        # new configuration persistence/reload cases.
        (directory/'state').mkdir()
        if (ROOT/'lua/scripted_log_start.lua').exists():
            shutil.copy2(ROOT/'lua/scripted_log_start.lua', directory/'lua/scripted_log_start.lua')
        worlds = []
        for index, label in enumerate(labels):
            endpoint = self.endpoints[label] = Endpoint()
            driver = self.drivers[label] = directory/(label+'-driver.xml')
            driver.write_text('<?xml version="1.0"?><!DOCTYPE muclient><muclient><plugin name="Logging_Fixture_Driver" id="'+DRIVER_ID+'" language="Lua" version="1.0"/><script><![CDATA['+DRIVER+']]></script></muclient>', encoding='utf-8')
            plugin_id, function = (HELPER_ID, 'StartLog') if hook == 'helper' else ((DRIVER_ID, 'FixtureStart') if hook == 'plugin' else ('', 'FixtureWorldStart'))
            includes = '<include name='+quoteattr(str(driver))+' plugin="y"/>'
            if hook == 'helper':
                target = directory/(label+'-helper.xml')
                target.write_text(configure_helper(helper.read_text(encoding='utf-8'), directory), encoding='utf-8')
                includes = '<include name='+quoteattr(str(target))+' plugin="y"/>'+includes
            script = directory/(label+'-world.lua')
            script.write_text("function FixtureWorldStart(...) local scope=GetVariable('fixture_scope');local f=assert(io.open('world-hook.receipt','a'));f:write(tostring(select('#',...)),'|',tostring(scope),'\\n');f:close();assert(scope=='world','World hook lost world context');assert(GetAlphaOption('log_start_plugin')=='','World hook read plugin options');DoCommand('LogSession');assert(OpenLogExclusive('world-hook.txt')==0);WriteLog('WORLD_HOOK_DATA') end", encoding='utf-8')
            world = directory/(label+'.mcl')
            world.write_text('<?xml version="1.0"?><!DOCTYPE muclient><muclient><world name="Logging Fictional '+label+'" id="'+('%02x' % (index+1))*12+'" site="127.0.0.1" port="'+str(endpoint.port)+'" utf_8="y" detect_pueblo="n" enable_scripts="y" enable_triggers="y" script_language="Lua" log_output="y" log_input="n" log_notes="n" log_start_script='+quoteattr(function)+' log_start_plugin='+quoteattr(plugin_id)+' script_filename='+quoteattr(str(script))+'/><variables><variable name="fixture_scope">world</variable></variables>'+includes+'</muclient>', encoding='utf-8')
            worlds.append(str(world))
        with sqlite3.connect(directory/'mushclient_prefs.sqlite') as db:
            db.executescript('CREATE TABLE control(name TEXT PRIMARY KEY,value INT);CREATE TABLE prefs(name TEXT PRIMARY KEY,value TEXT);CREATE TABLE worlds(name TEXT PRIMARY KEY,value TEXT);')
            db.executemany('INSERT INTO control VALUES(?,?)',[('database_version',1),('First time',0),('Version',507),('Tip_StartUp',1)])
            db.executemany('INSERT INTO prefs VALUES(?,?)',[('WorldList','*'.join(worlds)),('EnableSpellCheck','0'),('AutoConnectWorlds','1'),('ConfirmLogFileClose','0'),('ConfirmBeforeClosingMushclient','0'),('ConfirmBeforeClosingWorld','0'),('StateFilesDirectory',str(directory/'state')+'\\')])
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        self.client = subprocess.Popen([str(directory/exe.name), '/noregister'], cwd=directory, startupinfo=startup)
        try:
            assert wait_until(lambda: all(self.status(label).startswith('true|false|') for label in labels)), self.evidence()
            time.sleep(.25)
            assert all(not endpoint.commands for endpoint in self.endpoints.values()), self.evidence()
            assert not list(directory.glob('*.txt')), 'Install/connect opened an unsolicited log'
        except Exception:
            self.close()
            raise

    def status(self, label='Alpha'):
        path = Path(str(self.drivers[label])+'.status')
        try:
            return path.read_text()
        except (FileNotFoundError, PermissionError):
            return ''

    def control(self, action, label='Alpha'):
        path = Path(str(self.drivers[label])+'.control')
        assert not path.exists()
        receipt_prefix = action+':'
        previous = self.receipt(label).count(receipt_prefix)
        temporary = path.with_suffix('.pending')
        temporary.write_text(action)
        temporary.replace(path)
        assert wait_until(lambda: not path.exists() and self.receipt(label).count(receipt_prefix) > previous), self.evidence()

    def start(self, label='Alpha', expected=1):
        self.control('start', label)
        assert wait_until(lambda: len(self.endpoints[label].commands) >= expected), self.evidence()
        assert self.endpoints[label].commands == ['fixture-date']*expected, self.evidence()

    def receipt(self, label='Alpha'):
        path = Path(str(self.drivers[label])+'.receipt')
        return path.read_text() if path.exists() else ''

    def configure(self, data, label='Alpha'):
        Path(str(self.drivers[label])+'.settings').write_bytes(data)
        self.control('configure', label)
        return self.configuration(label)

    def configuration(self, label='Alpha'):
        base = str(self.drivers[label])
        return (Path(base+'.configuration').read_bytes(),
                Path(base+'.configuration-result').read_text())

    def evidence(self):
        return {'statuses': {label:self.status(label) for label in self.drivers}, 'driver': {label:self.receipt(label) for label in self.drivers}, 'commands': {label:endpoint.commands[:] for label,endpoint in self.endpoints.items()}, 'logs':[path.name for path in self.directory.glob('*.txt')]}

    def close(self):
        for endpoint in self.endpoints.values():
            endpoint.close()
        if self.client.poll() is None:
            self.client.terminate()
        self.client.wait(5)


def configure_helper(source, directory):
    """Replace the fixture copy's documented configuration block only."""
    config = r'''ScriptedLogConfig = {
 folder = FOLDER,
 template = '{world}-{year}-{month}-{day}-{number}.txt',
 request_command = 'fixture-date',
 start_regex = '^=== Fictional Calendar ===$',
 end_regex = '^=== End Calendar ===$',
 required = {'day','month','year'},
 fields = {
  {regex='^Day: ([0-9][0-9])$',capture=1,name='day'},
  {regex='^Month: ([0-9][0-9])$',capture=1,name='month'},
  {regex='^Year: ([0-9][0-9][0-9])$',capture=1,name='year'},
 },
 preserve_header = true, timeout = 1, max_records = 30, max_bytes = 16384,
 collision_attempts = 3,
}'''.replace('FOLDER', json.dumps(str(directory).replace('\\','/')))
    # The helper owns the exact marker; fail clearly if its public config moves.
    begin, end = '-- BEGIN SCRIPTED LOG CONFIG', '-- END SCRIPTED LOG CONFIG'
    assert begin in source and end in source, 'Helper configuration markers missing'
    return source[:source.index(begin)+len(begin)]+'\n'+config+'\n'+source[source.index(end):]


def saved_name_settings(directory, *, invalid_pcre=False, prefix='configured',
                        request='fixture-config-date'):
    """The data-only SLN1 format produced by the native automatic-names form."""
    fields = [
        ('day', '[' if invalid_pcre else '^Day: ([0-9][0-9])$', '1', 'pad2', '1'),
        ('month', '^Month: ([0-9][0-9])$', '1', 'pad2', '1'),
        ('year', '^Year: ([0-9][0-9][0-9])$', '1', 'number', '1'),
    ]
    values = [str(directory).replace('\\', '/'),
              prefix+'-{world}-{year}-{month}-{day}-{number}.txt', request,
              '^=== Fictional Calendar ===$', '^=== End Calendar ===$',
              '1', str(len(fields))]
    values.extend(value for field in fields for value in field)
    return ('SLN1\n'+''.join(value.encode('ascii').hex()+'\n' for value in values)).encode('ascii')


def run_case(root, exe, helper, name):
    directory = root/name
    fixture = Fixture(directory, exe, helper, ('Alpha','Beta') if name == 'two-worlds' else ('Alpha',), 'world' if name == 'world-hook' else ('plugin' if name == 'plugin-hook' else 'helper'))
    try:
        if name in ('world-hook','plugin-hook'):
            fixture.control('start')
            assert wait_until(lambda: '|true|' in fixture.status()), fixture.evidence()
            time.sleep(.15)
            assert '|true|' in fixture.status(), 'Queued recursive start closed the requested log'
            fixture.control('close')
            target = directory/('world-hook.txt' if name == 'world-hook' else 'plugin-hook.txt')
            assert ('WORLD_HOOK_DATA' if name == 'world-hook' else 'PLUGIN_HOOK_DATA') in target.read_text()
            if name == 'world-hook':
                assert (directory/'world-hook.receipt').read_text() == '0|world\n'
            else:
                assert fixture.receipt().count('hook:start\n') == 1
            assert fixture.endpoints['Alpha'].commands == []
            (directory/'prior.txt').write_bytes(b'PRIOR_LOG_MUST_SURVIVE\r\n')
            fixture.control('exclusive-existing')
            fixture.control('exclusive-missing-parent')
            fixture.control('exclusive-new')
            fixture.control('exclusive-active')
            fixture.control('close')
            receipt = fixture.receipt()
            for expected in ('exclusive-existing:30013','exclusive-missing-parent:30013','exclusive-new:0','exclusive-active:30015'):
                assert expected in receipt, receipt
            assert (directory/'prior.txt').read_bytes() == b'PRIOR_LOG_MUST_SURVIVE\r\n'
            assert not (directory/'must-not-exist.txt').exists()
            assert 'EXCLUSIVE_DATA' in (directory/'exclusive-new.txt').read_text()
        elif name in ('configured-success','configured-invalid-pcre',
                      'configured-pending','configured-reload'):
            saved = saved_name_settings(directory)
            actual, result = fixture.configure(saved)
            assert actual == saved and result == 'ok', fixture.evidence()
            assert fixture.endpoints['Alpha'].commands == [], fixture.evidence()
            assert not list(directory.glob('*.txt')), 'Saving settings started a log'
            state_files = list((directory/'state').glob('*-'+HELPER_ID+'-state.xml'))
            assert len(state_files) == 1, fixture.evidence()
            stored = {node.attrib.get('name'):node.text or ''
                      for node in ET.parse(state_files[0]).iter('variable')}
            assert stored.get('scripted_log_configuration', '').encode('ascii') == saved, stored
            if name == 'configured-invalid-pcre':
                actual, result = fixture.configure(saved_name_settings(directory, invalid_pcre=True))
                assert actual == saved and result == '', fixture.evidence()
                assert fixture.endpoints['Alpha'].commands == []
                assert not list(directory.glob('*.txt')), 'Invalid rules started a log'
                stored = {node.attrib.get('name'):node.text or ''
                          for node in ET.parse(state_files[0]).iter('variable')}
                assert stored.get('scripted_log_configuration', '').encode('ascii') == saved, stored
            elif name == 'configured-reload':
                # Reload destroys the helper's Lua state and imports its saved
                # variables. Its XML defaults request fixture-date and use a
                # different filename, so the manual start below proves the
                # persisted UI settings were loaded and actually used.
                fixture.control('reload-helper')
                assert 'reload-helper:0' in fixture.receipt(), fixture.evidence()
                actual, _ = fixture.configuration()
                # The client's XML variable loader restores structural LF as
                # CRLF. The configuration decoders must accept that native
                # persistence roundtrip; the next Start must still use it.
                assert actual.replace(b'\r\n', b'\n') == saved, fixture.evidence()
                assert fixture.endpoints['Alpha'].commands == [], 'Reload sent a metadata request'
                assert not list(directory.glob('*.txt')), 'Reload started a log'
            fixture.control('start')
            assert wait_until(lambda: fixture.endpoints['Alpha'].commands), fixture.evidence()
            assert fixture.endpoints['Alpha'].commands == ['fixture-config-date'], fixture.evidence()
            assert '|false|' in fixture.status(), 'Log opened before configured reply'
            if name == 'configured-pending':
                actual, result = fixture.configure(saved_name_settings(
                    directory, prefix='must-not-use', request='must-not-send'))
                assert actual == saved and result == '', fixture.evidence()
                assert fixture.endpoints['Alpha'].commands == ['fixture-config-date'], fixture.evidence()
            fixture.endpoints['Alpha'].header(tail='CONFIGURED_TAIL\r\n')
            assert wait_until(lambda: '|true|' in fixture.status()), fixture.evidence()
            fixture.endpoints['Alpha'].send('CONFIGURED_PAYLOAD\r\n')
            time.sleep(.15)
            fixture.control('close')
            paths = list(directory.glob('configured-Logging Fictional Alpha-178-04-18-*.txt'))
            assert len(paths) == 1, fixture.evidence()
            text = paths[0].read_text()
            assert all(text.count(line) == 1 for line in HEADER), text
            assert text.count('CONFIGURED_TAIL') == 1 and text.count('CONFIGURED_PAYLOAD') == 1, text
            assert fixture.endpoints['Alpha'].commands == ['fixture-config-date'], fixture.evidence()
        elif name in ('malformed','timeout','cancel','disconnect'):
            fixture.start()
            if name == 'malformed':
                fixture.endpoints['Alpha'].header([line for line in HEADER if not line.startswith('Year:')])
            elif name == 'cancel':
                fixture.control('cancel')
                fixture.endpoints['Alpha'].header()
            elif name == 'disconnect':
                fixture.endpoints['Alpha'].send(None)
                assert wait_until(lambda: fixture.status().startswith('false|false|'))
                fixture.endpoints['Alpha'].reaccept()
                fixture.control('reconnect')
                assert wait_until(lambda: fixture.status().startswith('true|false|'))
                # Replayed metadata from the old session must remain stale after reconnect.
                fixture.endpoints['Alpha'].header()
            time.sleep(1.4)
            if name in ('timeout','malformed'):
                # A valid but late response cannot create a guessed/current-date log.
                fixture.endpoints['Alpha'].header()
                time.sleep(.2)
            assert '|false|' in fixture.status(), fixture.evidence()
            assert not list(directory.glob('*.txt')), fixture.evidence()
        else:
            for label in fixture.drivers:
                fixture.start(label)
                if name == 'two-worlds':
                    fixture.control('activate', 'Beta' if label == 'Alpha' else 'Alpha')
                fixture.endpoints[label].header(tail='SCENE_BEFORE_LOG_TICK_'+label+'\r\n')
            assert wait_until(lambda: all('|true|' in fixture.status(label) for label in fixture.drivers)), fixture.evidence()
            for label in fixture.drivers:
                fixture.endpoints[label].send('PAYLOAD_'+label+'\r\n')
            time.sleep(.15)
            for label in fixture.drivers:
                fixture.control('close', label)
                paths = list(directory.glob('Logging Fictional '+label+'-178-04-18-*.txt'))
                assert len(paths) == 1, fixture.evidence()
                text = paths[0].read_text()
                assert all(text.count(line) == 1 for line in HEADER), text
                assert text.count('SCENE_BEFORE_LOG_TICK_'+label) == 1, text
                assert text.count('PAYLOAD_'+label) == 1 and 'PAYLOAD_'+('Beta' if label == 'Alpha' else 'Alpha') not in text, text
            if name == 'collision':
                prior = next(directory.glob('*.txt'))
                digest = hashlib.sha256(prior.read_bytes()).hexdigest()
                fixture.start(expected=2)
                fixture.endpoints['Alpha'].header()
                assert wait_until(lambda: '|true|' in fixture.status()), fixture.evidence()
                fixture.endpoints['Alpha'].send('SECOND_LOG_PAYLOAD\r\n')
                time.sleep(.15)
                fixture.control('close')
                assert len(list(directory.glob('*.txt'))) == 2, fixture.evidence()
                assert hashlib.sha256(prior.read_bytes()).hexdigest() == digest
        evidence = fixture.evidence()
        assert all(not endpoint.errors for endpoint in fixture.endpoints.values()), evidence
        return {'case': name, 'pass': True, **evidence}
    finally:
        fixture.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT/'WinRel/MUSHclient.exe')
    parser.add_argument('--helper', type=Path, default=ROOT/'plugins/Scripted_Log_Names.xml')
    parser.add_argument('--destination', type=Path)
    parser.add_argument('--cases', nargs='+', default=['plugin-hook','world-hook','success','malformed','timeout','cancel','disconnect','collision','two-worlds','configured-success','configured-invalid-pcre','configured-pending','configured-reload'])
    args = parser.parse_args()
    root = args.destination or Path(tempfile.mkdtemp(prefix='mush-scripted-logging-'))
    root.mkdir(exist_ok=True)
    results = {'executable':str(args.exe.resolve()),'sha256':hashlib.sha256(args.exe.read_bytes()).hexdigest(),'fixture':str(root),'cases':[]}
    for name in args.cases:
        try:
            result = run_case(root, args.exe.resolve(), args.helper.resolve(), name)
        except Exception as error:
            result = {'case':name,'pass':False,'error':repr(error)}
        results['cases'].append(result)
        (root/'results.json').write_text(json.dumps(results, indent=2))
        print(json.dumps(result), flush=True)
    results['pass'] = all(case['pass'] for case in results['cases'])
    (root/'results.json').write_text(json.dumps(results, indent=2))
    print(json.dumps({'pass':results['pass'],'fixture':str(root),'sha256':results['sha256']}), flush=True)
    return 0 if results['pass'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
