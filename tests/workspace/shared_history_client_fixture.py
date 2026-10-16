"""Actual-client shared history acceptance on a verified private Windows desktop."""
import argparse,ctypes,hashlib,json,os,subprocess,sys,time
from ctypes import wintypes
from pathlib import Path
from private_desktop_process import PrivateDesktopProcess
p=argparse.ArgumentParser();p.add_argument('--exe',required=True,type=Path);a=p.parse_args()
child=subprocess.Popen([sys.executable,str(Path(__file__).with_name('modern_client_fixture.py')),'--exe',str(a.exe),'--main-composer-test','--isolated-desktop','--duration','240'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
parts=[]
while True:
 line=child.stdout.readline()
 if not line:raise RuntimeError('Fixture launch failed '+''.join(parts))
 parts.append(line)
 if line.strip()=='}':break
info=json.loads(''.join(parts));dest=Path(info['fixture']);checks=[];sequence=0
assert info['isolated_desktop'].startswith('ValyriaQA-')
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
kernel.GetExitCodeProcess.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)];kernel.CloseHandle.argtypes=[wintypes.HANDLE]
candidate_handle=kernel.OpenProcess(0x100000|0x1000,False,info['candidate_pid']);assert candidate_handle
def read(path):return path.read_text(encoding='utf-8') if path.exists() else ''
def wait(f,timeout=15):
 until=time.monotonic()+timeout
 while time.monotonic()<until:
  result=f()
  if result:return result
  time.sleep(.05)
 raise AssertionError('Timed out; evidence '+str(dest))
def action(label,name,value=''):
 receipt=dest/('fixture-plugin.xml.main-'+label+'.receipt');before=len(read(receipt))
 control=dest/('fixture-plugin.xml.main-'+label+'.control');pending=Path(str(control)+'.pending')
 pending.write_text(name+'\n'+value,encoding='utf-8',newline='');pending.replace(control)
 result=wait(lambda:read(receipt)[before:] if 'done:'+name+':' in read(receipt)[before:] else None)
 assert 'done:'+name+':ok' in result,result
 return result
def helper(mode,values):
 global sequence
 sequence+=1;out=dest/('history-ui-'+str(sequence)+'.json')
 process=PrivateDesktopProcess([sys.executable,str(Path(__file__).with_name('private_candidate_actions.py')),str(info['candidate_pid']),mode,json.dumps(values),str(out)],info['isolated_desktop'],dest,os.environ.copy())
 return process,out
def finish(pair):
 process,out=pair
 try:assert process.wait(25)==0,'Private UI helper failed; evidence '+str(out)
 finally:
  if process.poll() is None:process.terminate();process.wait(3)
  process.close()
 return json.loads(out.read_text())
def check(label,value):
 checks.append({'name':label,'passed':bool(value)});print(('PASS ' if value else 'FAIL ')+label,flush=True);assert value,label
try:
 wait(lambda:'INSTALL Workspace acceptance Beta' in read(dest/'fixture-plugin.xml.receipt.txt'))
 def wire(label):return read(dest/(label.lower()+'.received.txt')).splitlines()
 action('Alpha','historyalias');action('Beta','historyalias')
 action('Alpha','draft','+alpha alias value');action('Alpha','submit');wait(lambda:'+alpha ALIAS_value' in wire('Alpha'))
 check('Main alias expands once into canonical wire command',wire('Alpha').count('+alpha ALIAS_value')==1)
 action('Alpha','draft','MAIN_UNSENT');action('Beta','draft','BETA_MAIN_UNSENT')
 finish(helper('set-channel',{'label':'Alpha Chat','text':'CHANNEL_UNSENT'}))
 recalled=finish(helper('channel-key',{'label':'Alpha Chat','key':'up','expected':'+alpha ALIAS_value'}))
 check('channel Up recalls canonical Main command across actual document pipeline',recalled['results'][0]['text']=='+alpha ALIAS_value')
 restored=finish(helper('channel-key',{'label':'Alpha Chat','key':'down','expected':'CHANNEL_UNSENT'}))
 check('channel Down restores its own unfinished draft',restored['results'][0]['text']=='CHANNEL_UNSENT')
 finish(helper('channel-key',{'label':'Alpha Chat','key':'up','expected':'+alpha ALIAS_value'}))
 finish(helper('channel-key',{'label':'Alpha Chat','key':'enter','expected':''}));wait(lambda:wire('Alpha').count('+alpha ALIAS_value')==2)
 check('recalled channel sends original command without duplicate prefix or alias reexpansion',not any('REEXPANDED_' in line or '+alpha +alpha' in line for line in wire('Alpha')))
 check('channel recall and submit preserve separate original Main draft','draft:MAIN_UNSENT\n' in action('Alpha','snapshot'))
 finish(helper('set-channel',{'label':'Beta Chat','text':'BETA_CHANNEL_UNSENT'}))
 beta=finish(helper('channel-key',{'label':'Beta Chat','key':'up','expected':'BETA_CHANNEL_UNSENT'}))
 check('other character does not inherit first character command history',beta['results'][0]['text']=='BETA_CHANNEL_UNSENT' and 'draft:BETA_MAIN_UNSENT\n' in action('Beta','snapshot'))
 finish(helper('set-channel',{'label':'Alpha Chat','text':'alias channel'}))
 finish(helper('channel-key',{'label':'Alpha Chat','key':'enter','expected':''}));wait(lambda:'+alpha ALIAS_channel' in wire('Alpha'))
 check('normal channel send uses prefix then ordinary alias pipeline once',wire('Alpha').count('+alpha ALIAS_channel')==1)
 action('Alpha','activate');action('Alpha','recallup')
 check('original Main recalls canonical channel command','draft:+alpha ALIAS_channel\n' in action('Alpha','snapshot'))
 action('Alpha','recalldown')
 check('Main Down restores its distinct unfinished draft','draft:MAIN_UNSENT\n' in action('Alpha','snapshot'))
 action('Alpha','recallup');action('Alpha','submit');wait(lambda:wire('Alpha').count('+alpha ALIAS_channel')==2)
 check('Main canonical recall also bypasses alias reexpansion',not any('REEXPANDED_' in line for line in wire('Alpha')))
finally:
 kernel.CloseHandle(candidate_handle)
 (dest/'shared-history-client-results.json').write_text(json.dumps({'candidate':info,'sha256':hashlib.sha256((dest/'MUSHclient.exe').read_bytes()).hexdigest(),'checks':checks},indent=2),encoding='utf-8')
 (dest/'fixture.control').write_text('stop');child.wait(timeout=20)
 print('Evidence: '+str(dest),flush=True)
