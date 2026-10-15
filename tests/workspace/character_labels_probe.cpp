#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include "workspace/CharacterLabelsDialog.h"

namespace {
int scenario=0;
void CALLBACK Exercise(HWND dialog,UINT,UINT_PTR timer,DWORD)
{
  KillTimer(dialog,timer);
  if(scenario==0){SetDlgItemTextW(dialog,101,L"Changed");SendMessageW(dialog,WM_COMMAND,IDCANCEL,0);}
  else {SetDlgItemTextW(dialog,101,L"  Silver Harbor  ");SetDlgItemTextW(dialog,102,L"  M\x00edra  ");SendMessageW(dialog,WM_COMMAND,IDOK,0);}
}
LRESULT CALLBACK Observe(int code,WPARAM wp,LPARAM lp)
{
  if(code==HCBT_ACTIVATE){wchar_t title[128]={};GetWindowTextW((HWND)wp,title,128);if(!wcscmp(title,L"Game and character labels"))SetTimer((HWND)wp,42,30,Exercise);}
  return CallNextHookEx(NULL,code,wp,lp);
}
}
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int)
{
  INITCOMMONCONTROLSEX common={sizeof(common),ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
  std::ofstream report("character-label-results.txt");int failures=0;
  auto check=[&](const char* name,bool ok){report<<(ok?"PASS ":"FAIL ")<<name<<"\n";if(!ok)++failures;};
  HHOOK hook=SetWindowsHookExW(WH_CBT,Observe,NULL,GetCurrentThreadId());
  std::wstring game=L"Original game",character=L"Original character";
  check("cancel returns false",!ShowCharacterLabelsDialog(NULL,game,character));
  check("cancel preserves caller values",game==L"Original game" && character==L"Original character");
  scenario=1;
  check("save returns true",ShowCharacterLabelsDialog(NULL,game,character));
  check("save trims game label",game==L"Silver Harbor");
  check("save retains Unicode character label",character==L"M\x00edra");
  UnhookWindowsHookEx(hook);report<<"failures="<<failures<<"\n";return failures?1:0;
}
