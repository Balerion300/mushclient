#pragma once
#include "../WorkspaceStyle.h"
#include <cwchar>
#include <cwctype>

// Shared tool chrome only. Scintilla and game transcript children deliberately
// keep their own typography, colors and rendering paths.
namespace workflow_ui {
inline void Polish(HWND window) {
  workspace_style::ApplySurface(window);
  for (HWND child = GetWindow(window, GW_CHILD); child;
       child = GetWindow(child, GW_HWNDNEXT)) {
    wchar_t name[64] = {};
    GetClassNameW(child, name, 64);
    if (!_wcsicmp(name, L"Scintilla")) continue;
    if (!_wcsicmp(name, L"BUTTON")) {
      const auto type = GetWindowLongPtrW(child, GWL_STYLE) & BS_TYPEMASK;
      if ((type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON) &&
          !GetPropW(child,L"MUSHclientWorkflowStyled"))
        workspace_style::StyleButton(child);
      else workspace_style::StyleControl(child);
    } else workspace_style::StyleControl(child);
    SetPropW(child,L"MUSHclientWorkflowStyled",reinterpret_cast<HANDLE>(1));
  }
}
inline void Button(HWND parent, int id, workspace_style::Icon icon,
                   bool primary = false, const wchar_t* hint = nullptr) {
  workspace_style::StyleButton(GetDlgItem(parent, id), icon, primary, hint);
}
inline void DpiChanged(HWND window, LPARAM proposed) {
  if (proposed) {
    const auto r = reinterpret_cast<const RECT*>(proposed);
    SetWindowPos(window, nullptr, r->left, r->top, r->right-r->left,
                 r->bottom-r->top, SWP_NOZORDER | SWP_NOACTIVATE);
  }
  Polish(window);
  workspace_style::FitToWorkArea(window);
}
inline bool Keyboard(HWND owner,MSG* message) {
  if(message->message==WM_KEYDOWN && message->wParam==VK_RETURN &&
      message->hwnd==GetFocus()) {
    wchar_t kind[32]={};GetClassNameW(message->hwnd,kind,32);
    const auto type=GetWindowLongPtrW(message->hwnd,GWL_STYLE)&BS_TYPEMASK;
    if(!_wcsicmp(kind,L"BUTTON") && IsWindowEnabled(message->hwnd) &&
        (type==BS_PUSHBUTTON || type==BS_DEFPUSHBUTTON)) {
      SendMessageW(message->hwnd,BM_CLICK,0,0);return true;
    }
  }
  if(message->message==WM_SYSCHAR) {
    // Offer only an explicit visible field mnemonic to the dialog manager.
    // Other Alt input keeps its existing editor/menu behavior.
    for(HWND child=GetWindow(owner,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
      wchar_t kind[32]={},label[256]={};GetClassNameW(child,kind,32);
      if(_wcsicmp(kind,L"STATIC") || !IsWindowVisible(child))continue;
      GetWindowTextW(child,label,256);
      for(int i=0;label[i];++i)if(label[i]==L'&' && label[i+1]) {
        if(label[i+1]==L'&'){++i;continue;}
        if(std::towlower(label[i+1])==std::towlower((wchar_t)message->wParam))
          return IsDialogMessageW(owner,message)!=FALSE;
      }
    }
  }
  return false;
}
} // namespace workflow_ui
