"""Full candidate draft close/exit acceptance on an explicit private Windows desktop."""
import argparse,ctypes,json,os,subprocess,sys,time
from ctypes import wintypes
from pathlib import Path
from private_desktop_process import PrivateDesktopProcess
p=argparse.ArgumentParser();p.add_argument('--exe',required=True,type=Path);p.add_argument('--package',required=True,type=Path);a=p.parse_args()
child=subprocess.Popen([sys.executable,str(Path(__file__).with_name('modern_client_fixture.py')),'--exe',str(a.exe),'--package',str(a.package),'--default-settings','--main-composer-test','--isolated-desktop','--duration','240'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
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
 sequence+=1;out=dest/('draft-ui-'+str(sequence)+'.json')
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
 action('Alpha','draft','ALPHA_UNSENT');action('Beta','draft','BETA_UNSENT')
 action('Alpha','disconnect');action('Beta','disconnect');action('Alpha','activate')
 prompt=helper('prompts',['cancel']);action('Alpha','close');finish(prompt)
 check('Cancel preserves disconnected original Main draft and open world','draft:ALPHA_UNSENT\n' in action('Alpha','snapshot') and 'alpha-open:true' in action('Beta','snapshot'))
 prompt=helper('prompts',['discard']);action('Alpha','close');finish(prompt)
 check('Discard explicitly permits closing the original world','alpha-open:false' in action('Beta','snapshot'))
 check('Closing another world preserves unrelated unsent writing','draft:BETA_UNSENT\n' in action('Beta','snapshot'))
 action('Beta','reopenalpha');action('Alpha','disconnect');action('Alpha','draft','');action('Alpha','activate')
 finish(helper('set-channel',{'label':'Alpha Chat','text':'CHANNEL_UNSENT_\u03bb'}))
 prompt=helper('prompts',['cancel']);action('Alpha','close');finish(prompt)
 channel=finish(helper('read-channel',{'label':'Alpha Chat'}))
 check('Channel-only draft prompts while Main is empty; Cancel preserves exact Unicode input',channel['results'][0]['text']=='CHANNEL_UNSENT_\u03bb' and 'draft:\n' in action('Alpha','snapshot'))
 finish(helper('set-channel',{'label':'Alpha Chat','text':''}));action('Alpha','close')
 check('Empty Main and channel inputs close without a discard prompt','alpha-open:false' in action('Beta','snapshot'))
 action('Beta','reopenalpha');action('Alpha','disconnect');action('Alpha','draft','ALPHA_EXIT_UNSENT')
 prompt=helper('prompts',['discard','cancel']);action('Beta','exit');first=finish(prompt)
 check('Exit prompts separately for both worlds and Cancel retains writing',len({row['message'] for row in first['results']})==2 and 'draft:ALPHA_EXIT_UNSENT\n' in action('Alpha','snapshot') and 'draft:BETA_UNSENT\n' in action('Beta','snapshot'))
 prompt=helper('prompts',['discard','discard']);action('Beta','exit');finish(prompt)
 exited=kernel.WaitForSingleObject(candidate_handle,10000)==0;exit_code=wintypes.DWORD();assert kernel.GetExitCodeProcess(candidate_handle,ctypes.byref(exit_code))
 check('Exit accepts only explicit discard for each world and exits normally',exited and exit_code.value==0)
finally:
 kernel.CloseHandle(candidate_handle)
 (dest/'draft-close-results.json').write_text(json.dumps({'candidate':info,'checks':checks},indent=2),encoding='utf-8')
 (dest/'fixture.control').write_text('stop');child.wait(timeout=20)
 print('Evidence: '+str(dest),flush=True)
