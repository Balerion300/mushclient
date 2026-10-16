"""Operate only one explicitly owned disposable candidate on a proven private desktop."""
import ctypes
from ctypes import wintypes as w
import json
from pathlib import Path
import sys,time
u=ctypes.WinDLL('user32',use_last_error=True);k=ctypes.WinDLL('kernel32')
u.GetThreadDesktop.argtypes=[w.DWORD];u.GetThreadDesktop.restype=w.HANDLE
u.GetUserObjectInformationW.argtypes=[w.HANDLE,ctypes.c_int,ctypes.c_void_p,w.DWORD,ctypes.POINTER(w.DWORD)]
name=ctypes.create_unicode_buffer(256);needed=w.DWORD()
assert u.GetUserObjectInformationW(u.GetThreadDesktop(k.GetCurrentThreadId()),2,name,ctypes.sizeof(name),ctypes.byref(needed)) and name.value.startswith('ValyriaQA-')
pid=int(sys.argv[1]);mode=sys.argv[2];values=json.loads(sys.argv[3]);output=Path(sys.argv[4])
callback=ctypes.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
u.EnumWindows.argtypes=[callback,w.LPARAM];u.EnumChildWindows.argtypes=[w.HWND,callback,w.LPARAM]
u.GetWindowThreadProcessId.argtypes=[w.HWND,ctypes.POINTER(w.DWORD)];u.GetParent.argtypes=[w.HWND];u.GetParent.restype=w.HWND
u.GetDlgItem.argtypes=[w.HWND,ctypes.c_int];u.GetDlgItem.restype=w.HWND
u.GetDlgCtrlID.argtypes=[w.HWND];u.GetWindowTextW.argtypes=[w.HWND,w.LPWSTR,ctypes.c_int];u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,ctypes.c_int]
u.SendMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.SendMessageW.restype=w.LPARAM
u.SetWindowTextW.argtypes=[w.HWND,w.LPCWSTR]
def text(h):
 b=ctypes.create_unicode_buffer(18000);u.SendMessageW(h,0xd,len(b),ctypes.cast(b,ctypes.c_void_p).value);return b.value
def kind(h):
 b=ctypes.create_unicode_buffer(256);u.GetClassNameW(h,b,len(b));return b.value
def windows():
 rows=[]
 def add(h,p):
  owner=w.DWORD();u.GetWindowThreadProcessId(h,ctypes.byref(owner))
  if owner.value==pid:rows.append(h)
  return True
 cb=callback(add)
 def top(h,p):
  add(h,p);u.EnumChildWindows(h,cb,0);return True
 u.EnumWindows(callback(top),0);return rows
def wait(f):
 until=time.monotonic()+15
 while time.monotonic()<until:
  value=f()
  if value:return value
  time.sleep(.05)
 output.write_text(json.dumps({'desktop':name.value,'candidate_pid':pid,'mode':mode,'timeout_windows':[{'hwnd':int(h),'class':kind(h),'text':text(h)} for h in windows()]},indent=2),encoding='utf-8')
 raise AssertionError('Private owned candidate action timed out: '+mode)
results=[]
if mode=='prompts':
 for choice in values:
  h=wait(lambda:next((h for h in windows() if text(h)=='Unsent input' and kind(h)=='#32770'),None))
  default=u.SendMessageW(h,0x400,0,0)&0xffff
  assert default==2,'Cancel must be the default'
  message=text(u.GetDlgItem(h,10));assert 'unsent Main and channel input' in message
  button=u.GetDlgItem(h,2 if choice=='cancel' else 1);assert text(button)==('Cancel' if choice=='cancel' else 'Discard input')
  results.append({'choice':choice,'message':message,'default':default})
  u.SendMessageW(button,0xf5,0,0)
  wait(lambda:h not in windows())
elif mode in ('set-channel','read-channel'):
 label=values['label'];h=wait(lambda:next((h for h in windows() if kind(h).lower()=='edit' and u.GetDlgCtrlID(h)==2 and label in text(u.GetParent(h)) and u.GetDlgItem(u.GetParent(h),100)),None))
 if mode=='set-channel':
  content=ctypes.create_unicode_buffer(values['text']);assert u.SendMessageW(h,0xc,0,ctypes.cast(content,ctypes.c_void_p).value)
 results.append({'label':label,'text':text(h),'hwnd':int(h)})
else:raise AssertionError(mode)
output.write_text(json.dumps({'desktop':name.value,'candidate_pid':pid,'mode':mode,'results':results},indent=2),encoding='utf-8')
