"""Profile an isolated real client using fictional loopback traffic and QPC stamps.

Measures completed-line delivery (not screen paint), normal Execute/echo latency,
CPU/private bytes/GDI/USER/handle trends. Never reads installed profiles.
"""
import argparse, ctypes, hashlib, json, pathlib, shutil, socket, sqlite3
import subprocess, tempfile, threading, time
import os, sys, uuid
from ctypes import wintypes as W
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from xml.sax.saxutils import quoteattr

ROOT=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
p.add_argument('--exe',type=pathlib.Path,default=ROOT/'WinRel/MUSHclient.exe')
p.add_argument('--seconds',type=int,default=600)
p.add_argument('--rate',type=int,default=250)
p.add_argument('--routes',type=int,default=2)
p.add_argument('--label',default='baseline')
p.add_argument('--no-heartbeat',action='store_true')
p.add_argument('--visible',action='store_true')
p.add_argument('--ascii',action='store_true')
p.add_argument('--font',default='Consolas')
p.add_argument('--image-url',default='https://raw.githubusercontent.com/Balerion300/mushclient/codex/inline-static-images/tests/fixture-scene.png')
p.add_argument('--blocked-images',action='store_true')
p.add_argument('--spawn-features',action='store_true',help='Exercise opt-in mention matching, priorities and following-image routes.')
p.add_argument('--split-spawns',action='store_true',help='Restore a balanced visible split layout instead of tabbing the routed panes.')
p.add_argument('--isolated-desktop',action='store_true',help='Use a verified private desktop, never display or switch to it.')
p.add_argument('--spell-drafts',action='store_true',help='Change fictional unsent Main prose once per second to exercise background spelling.')
a=p.parse_args()
if a.seconds<1 or a.rate<0 or not 0<=a.routes<=8:p.error('Use positive seconds, nonnegative rate and zero to eight routes.')
if a.split_spawns and a.routes<2:p.error('Split-spawn profiling requires at least two routes.')
dest=pathlib.Path(tempfile.mkdtemp(prefix='mush-perf-'+a.label+'-'))
(dest/'config.json').write_text(json.dumps({'sha256':hashlib.sha256(a.exe.read_bytes()).hexdigest(),'seconds':a.seconds,'rate':a.rate,'routes':a.routes,'heartbeat':not a.no_heartbeat,'ascii':a.ascii,'visible_startup':a.visible,'font':a.font,'spawn_features':a.spawn_features,'split_spawns':a.split_spawns,'images':'blocked-loopback' if a.blocked_images else a.image_url}))
runtime=ROOT/'.distribution-source/extracted'
shutil.copytree(runtime/'locale',dest/'locale')
shutil.copytree(ROOT/'spelling/packs',dest/'spelling/packs')
for f in (a.exe,runtime/'lua5.1.dll',ROOT/'third_party/scintilla/bin/Scintilla.dll'):
    shutil.copy2(f,dest/f.name)
# Keep matching private symbols with the exact fictional executable before a
# later build replaces the source-directory PDB. These files never enter Git.
symbols=a.exe.with_suffix('.pdb')
if symbols.exists():shutil.copy2(symbols,dest/symbols.name)
png=(ROOT/'tests/fixture-scene.png').read_bytes()
class Images(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200);self.send_header('Content-Type','image/png')
        self.send_header('Content-Length',str(len(png)));self.end_headers();self.wfile.write(png)
    def log_message(self,*args):pass
http=ThreadingHTTPServer(('127.0.0.1',0),Images)
threading.Thread(target=http.serve_forever,daemon=True).start()
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(20)
plugin=dest/'performance.xml'
plugin.write_text('''<?xml version="1.0"?><!DOCTYPE muclient><muclient>
<plugin name="Performance_Fixture" author="MUSHclient contributors" id="12ab34cd56ef789012ab34cd" language="Lua" version="1.0" requires="5.07"/>
<script><![CDATA[
local active=false
local receipt
local total=0
local ticks=0
local previous_tick=nil
local image_receipt
local pending_images={}
local last_image_poll=0
function OnPluginInstall()
 active=GetAlphaOption('site')=='127.0.0.1' and WorldName()=='Performance fictional world'
 if active then
  local fonts=assert(io.open(GetPluginInfo(GetPluginID(),6)..'.fonts.txt','w'));fonts:write(tostring(GetInfo(20)),'\\n',tostring(GetInfo(23)),'\\n');fonts:close()
  image_receipt=assert(io.open(GetPluginInfo(GetPluginID(),6)..'.images.csv','w'));image_receipt:write('sequence,state,width,height,time\\n');image_receipt:flush()
  receipt=assert(io.open(GetPluginInfo(GetPluginID(),6)..'.csv','w'));receipt:write('kind,sequence,now,sent,total\\n');receipt:flush()
 end
end
function OnPluginLineReceived(text)
 if not active then return true end
 total=total+1
 if text:sub(1,9)=='PERF_END ' then
  local seq,sent=text:match('^PERF_END (%d+) ([%d%.]+)')
  if seq then receipt:write(string.format('output,%s,%.9f,%s,%d\\n',seq,utils.timer(),sent,total));receipt:flush() end
 elseif text:sub(1,10)=='PERF_ECHO ' then
  local seq,sent=text:match('^PERF_ECHO (%d+) ([%d%.]+)')
  if seq then receipt:write(string.format('input,%s,%.9f,%s,%d\\n',seq,utils.timer(),sent,total));receipt:flush() end
 elseif text:sub(1,10)=='PERF_PING ' then
  local seq=text:match('^PERF_PING (%d+)')
  if seq then Execute(string.format('perf-input %s %.9f',seq,utils.timer())) end
 elseif text:sub(1,11)=='PERF_IMAGE ' then
  local seq=text:match('^PERF_IMAGE (%d+)')
  local count=GetLinesInBufferCount()
  for line=count,math.max(1,count-2),-1 do
   if GetLineInfo(line,14) then
    pending_images[#pending_images+1]={sequence=seq,line=line,source=GetLineInfo(line,15)};break
   end
  end
 end
 return true
end
function OnPluginTick()
 if active and receipt then
  local now=utils.timer();ticks=ticks+1
  if previous_tick then receipt:write(string.format('tick,%d,%.9f,%.9f,%d\\n',ticks,now,previous_tick,total));receipt:flush() end
  previous_tick=now
  if now-last_image_poll>.5 then
   last_image_poll=now
   for index=#pending_images,1,-1 do
    local image=pending_images[index]
    local state=-1
    if GetLineInfo(image.line,15)==image.source then state=GetLineInfo(image.line,18) or -1 end
    if state~=0 then
     image_receipt:write(string.format('%s,%d,%d,%d,%.9f\\n',image.sequence or '0',state,GetLineInfo(image.line,16) or 0,GetLineInfo(image.line,17) or 0,now));image_receipt:flush();table.remove(pending_images,index)
    end
   end
  end
 end
end
function OnPluginClose() if receipt then receipt:close() end if image_receipt then image_receipt:close() end end
]]></script></muclient>''',encoding='utf-8')
if a.no_heartbeat:
    plugin.write_text(plugin.read_text().replace('function OnPluginTick()', 'function DisabledHeartbeat()'), encoding='utf-8')
if a.spell_drafts:
    plugin.write_text(plugin.read_text().replace('previous_tick=now',"previous_tick=now\n  if ticks%10==0 then SetCommand('The knight watched the weirwod qzxvvaal '..tostring(ticks)) end"),encoding='utf-8')
world=dest/'fictional.mcl'
world.write_text('<?xml version="1.0"?><!DOCTYPE muclient><muclient><world name="Performance fictional world" id="ab12cd34ef56ab12cd34ef56" site="127.0.0.1" port="'+str(listener.getsockname()[1])+'" utf_8="y" use_mxp="2" detect_pueblo="n" enable_scripts="y" enable_triggers="y" enable_aliases="y" script_language="Lua" max_output_lines="5000" output_font_name="Consolas" output_font_height="11"/><include name='+quoteattr(str(plugin))+' plugin="y"/></muclient>',encoding='utf-8')
world.write_text(world.read_text().replace('output_font_name="Consolas"','output_font_name='+quoteattr(a.font)+' input_font_name='+quoteattr(a.font)),encoding='utf-8')
db=sqlite3.connect(dest/'mushclient_prefs.sqlite')
db.executescript('CREATE TABLE control(name TEXT PRIMARY KEY,value INT);CREATE TABLE prefs(name TEXT PRIMARY KEY,value TEXT);CREATE TABLE worlds(name TEXT PRIMARY KEY,value TEXT);CREATE TABLE workspace_route_v1(world_id TEXT,name TEXT,pattern TEXT,regex INTEGER,omit_main INTEGER,PRIMARY KEY(world_id,name,pattern,regex));')
db.executemany('INSERT INTO control VALUES(?,?)',[('database_version',1),('First time',0),('Version',507),('Tip_StartUp',1)])
db.executemany('INSERT INTO prefs VALUES(?,?)',[('WorldList',str(world)),('EnableSpellCheck','0'),('AutoConnectWorlds','1'),('OpenWorldsMaximised','1'),('ConfirmBeforeClosingMushclient','0'),('ConfirmBeforeClosingWorld','0')])
if a.spawn_features:
    db.executescript('CREATE TABLE workspace_spawn_preferences_v1(world_id TEXT NOT NULL,name TEXT NOT NULL,mention TEXT NOT NULL,priority INTEGER NOT NULL,shortcut INTEGER NOT NULL,PRIMARY KEY(world_id,name));CREATE TABLE workspace_route_images_v1(world_id TEXT NOT NULL,name TEXT NOT NULL,pattern TEXT NOT NULL,regex INTEGER NOT NULL,following_images INTEGER NOT NULL,PRIMARY KEY(world_id,name,pattern,regex));')
for i in range(a.routes):
    key=('ab12cd34ef56ab12cd34ef56','Profile '+str(i))
    pattern='^DATA' if a.spawn_features else '^DATA|fixture-scene'
    db.execute('INSERT INTO workspace_route_v1 VALUES(?,?,?,?,?)',(*key,pattern,1,0))
    if a.spawn_features:
        db.execute('INSERT INTO workspace_spawn_preferences_v1 VALUES(?,?,?,?,?)',(*key,'fictional output',i,0))
        db.execute('INSERT INTO workspace_route_images_v1 VALUES(?,?,?,?,?)',(*key,pattern,1,1))
if a.split_spawns:
    nodes=[]
    def split_model(indices, horizontal=False):
        if len(indices)==1:
            values=('ab12cd34ef56ab12cd34ef56','Profile '+str(indices[0]))
            node='-1 -1 500 0 0 1\n'+''.join(value.encode('utf-8').hex()+'\n' for value in values)
        else:
            middle=len(indices)//2
            left=split_model(indices[:middle],not horizontal)
            right=split_model(indices[middle:],not horizontal)
            node=f'{left} {right} 500 0 {int(horizontal)} 0\n'
        nodes.append(node)
        return len(nodes)-1
    root_node=split_model(list(range(a.routes)))
    forest=f'WDL3\n{len(nodes)} 1\n'+''.join(nodes)+f'{root_node} 32768 0 0 1 600 100 1800 1000\n'
    db.execute('CREATE TABLE IF NOT EXISTS workspace_dock_forest_v3(slot INTEGER PRIMARY KEY CHECK(slot=1),data BLOB NOT NULL)')
    db.execute('INSERT INTO workspace_dock_forest_v3 VALUES(1,?)',(forest.encode('ascii'),))
db.commit();db.close()
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=1 if a.visible else 0
desktop_handle=None
if a.isolated_desktop:
    from private_desktop_process import PrivateDesktopProcess
    desktop_api=ctypes.WinDLL('user32',use_last_error=True)
    desktop_api.CreateDesktopW.argtypes=[W.LPCWSTR,W.LPCWSTR,ctypes.c_void_p,W.DWORD,W.DWORD,ctypes.c_void_p];desktop_api.CreateDesktopW.restype=W.HANDLE
    desktop_api.CloseDesktop.argtypes=[W.HANDLE]
    desktop_name='ValyriaQA-'+uuid.uuid4().hex
    desktop_handle=desktop_api.CreateDesktopW(desktop_name,None,None,0,0x00ff,None)
    if not desktop_handle:raise ctypes.WinError(ctypes.get_last_error())
    try:
        proof_file=dest/'desktop-preflight.json'
        proof=PrivateDesktopProcess([sys.executable,str(pathlib.Path(__file__).with_name('inspect_fixture_desktop.py')),'0',str(proof_file)],desktop_name,dest,os.environ.copy())
        try:
            proof.wait(timeout=8)
            assert proof.returncode==0 and json.loads(proof_file.read_text())['desktop']==desktop_name,'Isolation proof failed; client will not launch'
        finally:
            if proof.poll() is None:proof.terminate();proof.wait(timeout=3)
            proof.close()
        client=PrivateDesktopProcess([str(dest/a.exe.name),'/noregister'],desktop_name,dest,os.environ.copy(),show=a.visible)
    except BaseException:
        desktop_api.CloseDesktop(desktop_handle);raise
else:client=subprocess.Popen([str(dest/a.exe.name),'/noregister'],cwd=dest,startupinfo=startup)
configuration=json.loads((dest/'config.json').read_text());configuration.update(isolated_desktop=a.isolated_desktop,spell_drafts=a.spell_drafts);(dest/'config.json').write_text(json.dumps(configuration))
print(json.dumps({'directory':str(dest),'pid':client.pid,'sha256':hashlib.sha256(a.exe.read_bytes()).hexdigest(),'seconds':a.seconds,'rate':a.rate,'routes':a.routes}),flush=True)
class Memory(ctypes.Structure):
    _fields_=[('cb',W.DWORD),('PageFaultCount',W.DWORD),('PeakWorkingSetSize',ctypes.c_size_t),('WorkingSetSize',ctypes.c_size_t),('QuotaPeakPagedPoolUsage',ctypes.c_size_t),('QuotaPagedPoolUsage',ctypes.c_size_t),('QuotaPeakNonPagedPoolUsage',ctypes.c_size_t),('QuotaNonPagedPoolUsage',ctypes.c_size_t),('PagefileUsage',ctypes.c_size_t),('PeakPagefileUsage',ctypes.c_size_t),('PrivateUsage',ctypes.c_size_t)]
k=ctypes.WinDLL('kernel32');ps=ctypes.WinDLL('psapi');u=ctypes.WinDLL('user32')
k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
ps.GetProcessMemoryInfo.argtypes=[W.HANDLE,ctypes.POINTER(Memory),W.DWORD]
u.GetGuiResources.argtypes=[W.HANDLE,W.DWORD];u.GetGuiResources.restype=W.DWORD
k.GetProcessHandleCount.argtypes=[W.HANDLE,ctypes.POINTER(W.DWORD)]
k.GetProcessTimes.argtypes=[W.HANDLE,ctypes.POINTER(W.FILETIME),ctypes.POINTER(W.FILETIME),ctypes.POINTER(W.FILETIME),ctypes.POINTER(W.FILETIME)]
k.CloseHandle.argtypes=[W.HANDLE]
WindowCallback=ctypes.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
u.EnumWindows.argtypes=[WindowCallback,W.LPARAM]
u.GetWindowThreadProcessId.argtypes=[W.HWND,ctypes.POINTER(W.DWORD)]
u.GetWindowRect.argtypes=[W.HWND,ctypes.POINTER(W.RECT)]
u.IsWindowVisible.argtypes=[W.HWND];u.IsIconic.argtypes=[W.HWND]
u.GetForegroundWindow.restype=W.HWND
handle=k.OpenProcess(0x410,False,client.pid)
def window_state():
    windows=[]
    @WindowCallback
    def inspect(window,_):
        owner=W.DWORD();u.GetWindowThreadProcessId(window,ctypes.byref(owner))
        if owner.value==client.pid:
            rect=W.RECT();u.GetWindowRect(window,ctypes.byref(rect))
            windows.append({'visible':bool(u.IsWindowVisible(window)),'minimized':bool(u.IsIconic(window)),'width':rect.right-rect.left,'height':rect.bottom-rect.top})
        return True
    u.EnumWindows(inspect,0)
    foreground=W.DWORD();u.GetWindowThreadProcessId(u.GetForegroundWindow(),ctypes.byref(foreground))
    return {'foreground':foreground.value==client.pid,'windows':windows}
def sample():
    m=Memory();m.cb=ctypes.sizeof(m);assert ps.GetProcessMemoryInfo(handle,ctypes.byref(m),m.cb)
    files=[W.FILETIME() for _ in range(4)];assert k.GetProcessTimes(handle,*[ctypes.byref(f) for f in files])
    cpu=sum((f.dwHighDateTime<<32)|f.dwLowDateTime for f in files[2:])/1e7
    count=W.DWORD();k.GetProcessHandleCount(handle,ctypes.byref(count))
    return {'time':time.perf_counter(),'private':m.PrivateUsage,'working_set':m.WorkingSetSize,'cpu':cpu,'gdi':u.GetGuiResources(handle,0),'user':u.GetGuiResources(handle,1),'handles':count.value,'window_state':window_state()}
stop=threading.Event();conn=None;send_lock=threading.Lock();rows=[];sent=[]
def receive():
    pending=b'';state=0
    while not stop.is_set():
        try:chunk=conn.recv(65536)
        except socket.timeout:continue
        except OSError:return
        if not chunk:return
        clean=bytearray()
        for byte in chunk:
            if state==0:
                if byte==255:state=1
                else:clean.append(byte)
            elif state==1:state=2 if byte in (251,252,253,254) else (3 if byte==250 else 0)
            elif state==2:state=0
            elif state==3:
                if byte==255:state=4
            elif state==4:state=0 if byte==240 else 3
        pending+=clean
        while b'\n' in pending:
            raw,pending=pending.split(b'\n',1)
            if raw.startswith(b'perf-input '):
                try:
                    with send_lock:conn.sendall(b'PERF_ECHO '+raw[len(b'perf-input '):].rstrip(b'\r')+b'\r\n')
                except OSError:return
try:
    conn,_=listener.accept();conn.settimeout(10)
    threading.Thread(target=receive,daemon=True).start()
    began=time.perf_counter();last=began;serial=0;line_credit=0.0
    with (dest/'resources.jsonl').open('w') as resource_log:
        while client.poll() is None and time.perf_counter()-began<a.seconds:
            if (dest/'stop').exists():break
            now=time.perf_counter();elapsed=now-began;cycle=elapsed%120
            phase='plain' if cycle<30 else ('styled' if cycle<60 else ('images' if cycle<90 else 'idle'))
            if now-last>=1:
                row=sample();row.update(elapsed=elapsed,phase=phase);rows.append(row);resource_log.write(json.dumps(row)+'\n');resource_log.flush();last=now
            serial+=1;stamp=time.perf_counter();batch=[]
            line_credit+=a.rate*.1 if phase!='idle' else 0
            count=int(line_credit);line_credit-=count
            for j in range(count):
                text=('DATA %d.%d fictional output '%(serial,j)+('ab ' if a.ascii else 'λ🐉 ')+'x'*80)
                if phase in ('styled','images'):text='\x1b[31m'+text+'\x1b[0m'
                batch.append(text+'\r\n')
            if phase=='images' and serial%10==0:
                image_url=('http://127.0.0.1:%d/fixture-scene-%d.png'%(http.server_port,serial%64)) if a.blocked_images else a.image_url+('&' if '?' in a.image_url else '?')+'profile-key='+str(serial%64)
                batch.append('\x1b[1z<image url='+quoteattr(image_url)+'>\r\nPERF_IMAGE %d\r\n'%serial)
            batch.append('PERF_END %d %.9f\r\n'%(serial,stamp))
            if serial%5==0:batch.append('PERF_PING %d\r\n'%serial)
            payload=''.join(batch).encode()
            try:
                with send_lock:conn.sendall(payload)
            except socket.timeout:
                (dest/'overload.json').write_text(json.dumps({'elapsed':elapsed,'phase':phase,'completed_batches':serial-1,'reason':'TCP backpressure exceeded 10 seconds'}));break
            sent.append({'sequence':serial,'time':stamp,'phase':phase,'lines':len(batch),'bytes':len(payload)})
            time.sleep(max(0,.1-(time.perf_counter()-stamp)))
        (dest/'completion.json').write_text(json.dumps({'requested_duration_completed':time.perf_counter()-began>=a.seconds,'client_exited':client.poll() is not None,'stopped':(dest/'stop').exists()}))
        # A bounded idle drain distinguishes queued output from completed work.
        for _ in range(100):
            if client.poll() is not None:break
            row=sample();row.update(elapsed=time.perf_counter()-began,phase='drain');rows.append(row);resource_log.write(json.dumps(row)+'\n');resource_log.flush();time.sleep(.1)
    (dest/'sent.json').write_text(json.dumps(sent))
finally:
    (dest/'sent.json').write_text(json.dumps(sent))
    stop.set()
    if conn:conn.close()
    listener.close();http.shutdown();k.CloseHandle(handle)
    if client.poll() is None:client.terminate();client.wait(timeout=10)
    if desktop_handle:client.close();desktop_api.CloseDesktop(desktop_handle)
print('Profile complete: '+str(dest),flush=True)
