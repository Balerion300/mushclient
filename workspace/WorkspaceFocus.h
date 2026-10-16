#pragma once
#include <windows.h>
#include <stdio.h>

// Shared by the document frame and real detached pane frames. No input is
// copied, dispatched or synthesized; this only restores a valid native child.
class WorkspaceFocusState {
  HWND owner_=NULL,focus_=NULL;
  static LPCWSTR Property(){return L"Valyria.WorkspaceFocusOwner";}
  void Remember(HWND focus){
    if(!focus || !::IsChild(owner_,focus))return;
    if(focus_ && ::GetPropW(focus_,Property())==owner_)::RemovePropW(focus_,Property());
    focus_=focus;
    if(!::SetPropW(focus_,Property(),owner_))focus_=NULL;
  }
  void Restore(){
    // HWND properties disappear when controls are destroyed. A reused handle,
    // hidden/disabled view or pane moved to a different frame is rejected.
    if(focus_ && ::GetPropW(focus_,Property())==owner_ && ::IsChild(owner_,focus_) &&
       ::IsWindowVisible(focus_) && ::IsWindowEnabled(focus_) && ::GetActiveWindow()==owner_)
      ::SetFocus(focus_);
  }
public:
  static void Trace(const char* stage,HWND owner,UINT message,WPARAM wp){
    static int enabled=-1;static unsigned count=0;
    if(enabled<0){wchar_t value[8]={};enabled=GetEnvironmentVariableW(L"VALYRIA_FOCUS_TRACE",value,8)==1 && value[0]==L'1';}
    if(!enabled || count++>=2048)return;
    // Opt-in fixture diagnostics: handles/classes/control IDs only, never
    // draft text, world identities, credentials or player output.
    HWND focus=::GetFocus();char name[128]={};::GetClassNameA(focus,name,128);
    RECT bounds={};::GetWindowRect(owner,&bounds);
    char line[768]={};const int length=_snprintf_s(line,sizeof(line),_TRUNCATE,
      "%lu %s owner=%p msg=%04x wp=%p focus=%p class=%s id=%d active=%p visible=%d rect=%ld,%ld,%ld,%ld parent=%p\r\n",
      GetTickCount(),stage,owner,message,(void*)wp,focus,name,::GetDlgCtrlID(focus),::GetActiveWindow(),::IsWindowVisible(owner),bounds.left,bounds.top,bounds.right,bounds.bottom,::GetParent(owner));
    HANDLE file=CreateFileW(L"valyria-focus.txt",FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE){DWORD written;if(length>0)WriteFile(file,line,length,&written,NULL);CloseHandle(file);}
  }
  void Before(HWND owner,UINT message,WPARAM wp){
    owner_=owner;
    if(message==WM_ACTIVATE || message==WM_SETFOCUS || message==WM_KILLFOCUS)Trace("before",owner,message,wp);
    if(message==WM_ACTIVATE && LOWORD(wp)==WA_INACTIVE)Remember(::GetFocus());
    // SetFocus(frame) can happen while the application is already active.
    // WM_SETFOCUS carries the actual prior control; GetFocus is the frame now.
    if(message==WM_SETFOCUS)Remember((HWND)wp);
    if(message==WM_NCDESTROY && focus_ && ::GetPropW(focus_,Property())==owner_)::RemovePropW(focus_,Property());
  }
  void After(UINT message,WPARAM wp){
    // Synchronous restoration cannot overtake the next click or Tab operation.
    if(message==WM_SETFOCUS || (message==WM_ACTIVATE && LOWORD(wp)!=WA_INACTIVE && !HIWORD(wp)))Restore();
    if(message==WM_ACTIVATE || message==WM_SETFOCUS || message==WM_KILLFOCUS)Trace("after",owner_,message,wp);
  }
};
