#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace workspace_draft {
struct ClosePrompt {std::wstring world;};
inline INT_PTR CALLBACK CloseProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
  if(message==WM_INITDIALOG){
    auto* prompt=reinterpret_cast<ClosePrompt*>(lp);
    const std::wstring text=L"Closing "+prompt->world+L" will discard its unsent Main and channel input.\n\nCancel keeps the world and your writing open.";
    HFONT font=(HFONT)SendMessageW(window,WM_GETFONT,0,0);
    auto control=[&](const wchar_t* kind,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id){
      RECT r={x,y,x+w,y+h};MapDialogRect(window,&r);
      HWND child=CreateWindowExW(0,kind,label,WS_CHILD|WS_VISIBLE|style,r.left,r.top,r.right-r.left,r.bottom-r.top,window,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
      SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE);return child;};
    control(L"STATIC",text.c_str(),SS_NOPREFIX,12,12,276,62,10);
    control(L"BUTTON",L"Discard input",WS_TABSTOP|BS_PUSHBUTTON,120,84,92,20,IDOK);
    HWND cancel=control(L"BUTTON",L"Cancel",WS_TABSTOP|BS_DEFPUSHBUTTON,220,84,68,20,IDCANCEL);
    SendMessageW(window,DM_SETDEFID,IDCANCEL,0);SetFocus(cancel);return FALSE;
  }
  if(message==WM_CLOSE){EndDialog(window,IDCANCEL);return TRUE;}
  if(message==WM_COMMAND && (LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL)){EndDialog(window,LOWORD(wp));return TRUE;}
  return FALSE;
}
inline bool ConfirmDiscard(HWND owner,const std::wstring& world)
{
  std::vector<DWORD> storage(128,0);auto* form=(DLGTEMPLATE*)storage.data();
  form->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT|DS_CENTER;
  form->cx=300;form->cy=116;
  auto* tail=(WORD*)((BYTE*)form+sizeof(DLGTEMPLATE));*tail++=0;*tail++=0;
  const wchar_t* title=L"Unsent input";wcscpy_s((wchar_t*)tail,64,title);tail+=wcslen(title)+1;
  *tail++=9;wcscpy_s((wchar_t*)tail,32,L"Segoe UI");ClosePrompt prompt={world};
  // Failure to create the confirmation must preserve the draft too.
  return DialogBoxIndirectParamW(GetModuleHandle(NULL),form,owner,CloseProcedure,(LPARAM)&prompt)==IDOK;
}
}
