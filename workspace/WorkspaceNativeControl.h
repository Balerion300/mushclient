#pragma once
#include <afxwin.h>
#include <commctrl.h>

namespace workspace_native_control {
// Attach alone registers MFC's permanent HWND map but does not receive native
// destruction. Keep Unicode EDIT/RICHEDIT50W processing native in this MBCS app;
// observe only lifetime, instead of routing text through an ANSI AfxWndProc.
inline LRESULT CALLBACK LifetimeProc(HWND window,UINT message,WPARAM wp,LPARAM lp,
                                    UINT_PTR id,DWORD_PTR data){
  if(message==WM_NCDESTROY){
    CWnd* control=reinterpret_cast<CWnd*>(data);
    RemoveWindowSubclass(window,LifetimeProc,id);
    if(CWnd::FromHandlePermanent(window)==control && control->GetSafeHwnd()==window)
      control->Detach();
    // Forward exactly once. Native teardown may delete an owning C++ object;
    // neither its wrapper nor its parent is accessed after this call.
  }
  return DefSubclassProc(window,message,wp,lp);
}
inline BOOL Attach(CWnd& control,HWND window){
  if(!control.Attach(window))return FALSE;
  if(!SetWindowSubclass(window,LifetimeProc,reinterpret_cast<UINT_PTR>(&LifetimeProc),
                        reinterpret_cast<DWORD_PTR>(&control))){
    control.Detach();return FALSE; // Caller still owns the raw native window.
  }
  return TRUE;
}
}
