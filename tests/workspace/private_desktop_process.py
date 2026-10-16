"""Create only disposable fixture processes on an explicit private desktop.

Python 3.11 subprocess.STARTUPINFO ignores arbitrary lpDesktop attributes, so use
the real Win32 structure. No desktop switching or user-input APIs are used.
"""
import ctypes
from ctypes import wintypes
import subprocess

class StartupInfo(ctypes.Structure):
    _fields_ = [('cb',wintypes.DWORD),('lpReserved',wintypes.LPWSTR),('lpDesktop',wintypes.LPWSTR),('lpTitle',wintypes.LPWSTR),('dwX',wintypes.DWORD),('dwY',wintypes.DWORD),('dwXSize',wintypes.DWORD),('dwYSize',wintypes.DWORD),('dwXCountChars',wintypes.DWORD),('dwYCountChars',wintypes.DWORD),('dwFillAttribute',wintypes.DWORD),('dwFlags',wintypes.DWORD),('wShowWindow',wintypes.WORD),('cbReserved2',wintypes.WORD),('lpReserved2',ctypes.c_void_p),('hStdInput',wintypes.HANDLE),('hStdOutput',wintypes.HANDLE),('hStdError',wintypes.HANDLE)]
class ProcessInfo(ctypes.Structure):
    _fields_ = [('hProcess',wintypes.HANDLE),('hThread',wintypes.HANDLE),('dwProcessId',wintypes.DWORD),('dwThreadId',wintypes.DWORD)]

class PrivateDesktopProcess:
    def __init__(self, args, desktop, cwd, env, show=False):
        assert desktop.startswith('ValyriaQA-')
        self.api = ctypes.WinDLL('kernel32', use_last_error=True)
        self.api.CreateProcessW.argtypes = [wintypes.LPCWSTR,wintypes.LPWSTR,ctypes.c_void_p,ctypes.c_void_p,wintypes.BOOL,wintypes.DWORD,ctypes.c_void_p,wintypes.LPCWSTR,ctypes.POINTER(StartupInfo),ctypes.POINTER(ProcessInfo)]
        self.api.CloseHandle.argtypes = [wintypes.HANDLE]
        self.api.GetExitCodeProcess.argtypes = [wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)]
        self.api.TerminateProcess.argtypes = [wintypes.HANDLE,wintypes.UINT]
        self.api.WaitForSingleObject.argtypes = [wintypes.HANDLE,wintypes.DWORD]
        startup, info = StartupInfo(), ProcessInfo()
        startup.cb = ctypes.sizeof(startup)
        startup.lpDesktop = desktop
        startup.dwFlags, startup.wShowWindow = 1, (1 if show else 0)
        command = ctypes.create_unicode_buffer(subprocess.list2cmdline(args))
        block = ctypes.create_unicode_buffer('\0'.join(k+'='+v for k,v in sorted(env.items()))+'\0\0')
        if not self.api.CreateProcessW(None,command,None,None,False,0x400,block,str(cwd),ctypes.byref(startup),ctypes.byref(info)):
            raise ctypes.WinError(ctypes.get_last_error())
        self.api.CloseHandle(info.hThread)
        self.handle, self.pid, self.thread_id, self.returncode = info.hProcess, info.dwProcessId, info.dwThreadId, None
    def poll(self):
        code = wintypes.DWORD()
        if not self.api.GetExitCodeProcess(self.handle,ctypes.byref(code)):
            raise ctypes.WinError(ctypes.get_last_error())
        if code.value != 259: self.returncode = code.value
        return self.returncode
    def wait(self, timeout):
        if self.api.WaitForSingleObject(self.handle,int(timeout*1000)) == 258:
            raise subprocess.TimeoutExpired('private desktop candidate',timeout)
        return self.poll()
    def terminate(self):
        if not self.api.TerminateProcess(self.handle,1):
            raise ctypes.WinError(ctypes.get_last_error())
    def close(self):
        self.api.CloseHandle(self.handle)
