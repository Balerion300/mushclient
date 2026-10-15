"""Read windows of one disposable candidate, on its private test desktop only."""
import ctypes
from ctypes import wintypes
import json
import sys
from pathlib import Path

user = ctypes.WinDLL('user32', use_last_error=True)
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
user.GetThreadDesktop.argtypes = [wintypes.DWORD]
user.GetThreadDesktop.restype = wintypes.HANDLE
user.GetUserObjectInformationW.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
name = ctypes.create_unicode_buffer(256)
needed = wintypes.DWORD()
assert user.GetUserObjectInformationW(user.GetThreadDesktop(kernel.GetCurrentThreadId()), 2, name, ctypes.sizeof(name), ctypes.byref(needed))
assert name.value.startswith('ValyriaQA-'), 'Refusing to inspect the interactive desktop'
user.OpenInputDesktop.argtypes = [wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
user.OpenInputDesktop.restype = wintypes.HANDLE
user.CloseDesktop.argtypes = [wintypes.HANDLE]
input_name = ctypes.create_unicode_buffer(256)
input_desktop = user.OpenInputDesktop(0,False,1)
if input_desktop:
    try: user.GetUserObjectInformationW(input_desktop,2,input_name,ctypes.sizeof(input_name),ctypes.byref(needed))
    finally: user.CloseDesktop(input_desktop)
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
user.EnumChildWindows.argtypes = [wintypes.HWND, callback_type, wintypes.LPARAM]
user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
rows = []
def inspect(hwnd, parameter):
    pid = wintypes.DWORD()
    user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    if pid.value == int(sys.argv[1]):
        text, kind = ctypes.create_unicode_buffer(4096), ctypes.create_unicode_buffer(256)
        user.GetWindowTextW(hwnd, text, len(text))
        user.GetClassNameW(hwnd, kind, len(kind))
        rows.append({'hwnd':int(hwnd), 'class':kind.value, 'text':text.value})
    return True
child = callback_type(inspect)
def top(hwnd, parameter):
    inspect(hwnd, parameter)
    user.EnumChildWindows(hwnd, child, 0)
    return True
ok = user.EnumWindows(callback_type(top), 0)
result = json.dumps({'desktop':name.value, 'input_desktop':input_name.value, 'enumerated':bool(ok), 'error':ctypes.get_last_error(), 'windows':rows}, indent=2)
if len(sys.argv)>2: Path(sys.argv[2]).write_text(result, encoding='utf-8')
else: print(result)
