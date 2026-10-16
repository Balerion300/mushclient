// Hidden dialog geometry/painting only. Never activates, shows or types into a desktop window.
#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include <set>
#include <cstring>
static UINT simulatedDpi=0;
static UINT WINAPI SimulatedDpi(HWND){return simulatedDpi;}
static FARPROC WINAPI ProbeGetProcAddress(HMODULE module,LPCSTR name){if(simulatedDpi && (ULONG_PTR)name>65535 && !strcmp(name,"GetDpiForWindow"))return (FARPROC)SimulatedDpi;return GetProcAddress(module,name);}
#define GetProcAddress ProbeGetProcAddress
#include "workspace/WorkspaceSpell.h"
#undef GetProcAddress
static bool good=true;

static void Bitmap(HWND window,const char* path){
 RECT bounds;GetWindowRect(window,&bounds);int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
 BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 void* pixels=NULL;HBITMAP bitmap=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);HDC dc=CreateCompatibleDC(NULL);auto old=SelectObject(dc,bitmap);
 RECT fill={0,0,width,height};FillRect(dc,&fill,(HBRUSH)(COLOR_BTNFACE+1));SendMessageW(window,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);GdiFlush();
 BITMAPFILEHEADER header={};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+width*height*4;
 std::ofstream out(path,std::ios::binary);out.write((char*)&header,sizeof(header));out.write((char*)&info.bmiHeader,sizeof(info.bmiHeader));out.write((char*)pixels,width*height*4);SelectObject(dc,old);DeleteDC(dc);DeleteObject(bitmap);
}
static void Inspect(HWND window,std::ofstream& out,const char* label){
 typedef UINT(WINAPI* DpiFunction)(HWND);auto getDpi=(DpiFunction)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForWindow");
 out<<label<<" visible="<<IsWindowVisible(window)<<" dpi="<<(getDpi?getDpi(window):96)<<'\n';if(IsWindowVisible(window))good=false;RECT parent;GetClientRect(window,&parent);std::set<int> ids;
 for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
  wchar_t kind[80]={},text[1024]={};GetClassNameW(child,kind,80);GetWindowTextW(child,text,1024);RECT bounds;GetWindowRect(child,&bounds);MapWindowPoints(NULL,window,(POINT*)&bounds,2);
  const int id=GetDlgCtrlID(child);const bool duplicate=!ids.insert(id).second;if(duplicate)good=false;out<<"id="<<id<<" duplicate="<<duplicate<<" rect="<<bounds.left<<','<<bounds.top<<','<<bounds.right<<','<<bounds.bottom<<" enabled="<<IsWindowEnabled(child)<<" tab="<<!!(GetWindowLongPtrW(child,GWL_STYLE)&WS_TABSTOP);
  if(id==4)out<<" editClientEdge="<<!!(GetWindowLongPtrW(child,GWL_EXSTYLE)&WS_EX_CLIENTEDGE)<<" childVisibleStyle="<<!!(GetWindowLongPtrW(child,GWL_STYLE)&WS_VISIBLE);
  if(id==102){wchar_t selected[300]={};const LRESULT index=SendMessageW(child,CB_GETCURSEL,0,0);SendMessageW(child,CB_GETLBTEXT,index,(LPARAM)selected);out<<" comboSelection="<<index<<" windowTextChars="<<wcslen(text)<<" selectedTextChars="<<wcslen(selected)<<" selectedIsPersonal="<<!wcscmp(selected,L"Personal dictionary");}
  HDC dc=GetDC(child);auto font=(HFONT)SendMessageW(child,WM_GETFONT,0,0);auto old=SelectObject(dc,font);TEXTMETRICW metrics;GetTextMetricsW(dc,&metrics);RECT needed={0,0,max(1,bounds.right-bounds.left),0};DrawTextW(dc,text,-1,&needed,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);ReleaseDC(child,dc);
  const bool outside=bounds.left<0||bounds.top<0||bounds.right>parent.right||bounds.bottom>parent.bottom;
  out<<" textHeight="<<needed.bottom<<" fontHeight="<<metrics.tmHeight<<" outside="<<outside<<" verticallyClipped="<<(needed.bottom>bounds.bottom-bounds.top)<<'\n';
  if(outside || needed.bottom>bounds.bottom-bounds.top)good=false;
 }
 const int defaultId=LOWORD(SendMessageW(window,DM_GETDEFID,0,0));out<<"defaultId="<<defaultId<<'\n';if(defaultId!=5)good=false;
}
int main(){
 const HWND original=GetForegroundWindow();INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
 std::ofstream out("dialog-ux-results.txt");wchar_t root[MAX_PATH];GetCurrentDirectoryW(MAX_PATH,root);CreateDirectoryW((std::wstring(root)+L"\\dictionary-fixture").c_str(),NULL);
 workspace_spell::Context context;context.root=std::wstring(root)+L"\\dictionary-fixture";context.gameKey=L"fictional-alpha";context.gameLabel=L"Blood of Dragons (fictional test)";
 HWND owner=CreateWindowExW(0,L"EDIT",L"",WS_POPUP,0,0,300,100,NULL,NULL,GetModuleHandle(NULL),NULL);
 auto state=std::make_shared<workspace_spell::State>();state->window=owner;state->current=context;state->context=[=](){return context;};workspace_spell::Editors()[owner]=state;
 workspace_spell::DictionaryDialog dialog;dialog.state=state;dialog.context=context;
 std::vector<DWORD> storage(128,0);auto form=(DLGTEMPLATE*)storage.data();form->style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_SETFONT;form->cx=340;form->cy=260;auto tail=(WORD*)((BYTE*)form+sizeof(DLGTEMPLATE));*tail++=0;*tail++=0;const wchar_t* title=L"Spelling dictionaries";wcscpy_s((wchar_t*)tail,100,title);tail+=wcslen(title)+1;WORD* fontSize=tail;*tail++=9;wcscpy_s((wchar_t*)tail,32,L"Segoe UI");
 HWND window=CreateDialogIndirectParamW(GetModuleHandle(NULL),form,owner,workspace_spell::DictionaryDialog::Procedure,(LPARAM)&dialog);if(!window)return 2;KillTimer(window,1);Inspect(window,out,"native actual monitor");Bitmap(window,"spell-manager-native.bmp");
 SendDlgItemMessageW(window,102,CB_SETCURSEL,1,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(102,CBN_SELCHANGE),(LPARAM)GetDlgItem(window,102));const bool scopeChanged=IsWindow(window)&&dialog.scope==1;out<<"gameScopeChangePreservedDialog="<<scopeChanged<<'\n';if(!scopeChanged)good=false;
 SendDlgItemMessageW(window,102,CB_SETCURSEL,2,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(102,CBN_SELCHANGE),(LPARAM)GetDlgItem(window,102));const bool readOnly=!IsWindowEnabled(GetDlgItem(window,4))&&!IsWindowEnabled(GetDlgItem(window,5))&&!IsWindowEnabled(GetDlgItem(window,6));out<<"packReadOnly="<<readOnly<<'\n';if(!readOnly)good=false;
 SendDlgItemMessageW(window,102,CB_SETCURSEL,0,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(102,CBN_SELCHANGE),(LPARAM)GetDlgItem(window,102));
 DestroyWindow(window);
 // Simulated DPI drives production S() layout; the enlarged template font is
 // intentionally conservative. This is not a physical monitor DPI test.
 simulatedDpi=144;*fontSize=14;window=CreateDialogIndirectParamW(GetModuleHandle(NULL),form,owner,workspace_spell::DictionaryDialog::Procedure,(LPARAM)&dialog);KillTimer(window,1);Inspect(window,out,"simulated144DPI layout and enlarged font");Bitmap(window,"spell-manager-simulated-150.bmp");DestroyWindow(window);
 workspace_spell::Editors().erase(owner);DestroyWindow(owner);out<<"foregroundPreserved="<<(GetForegroundWindow()==original)<<'\n';return good && GetForegroundWindow()==original?0:3;
}
