#include "stdafx.h"
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include <fstream>

// Deliberately never shows a top-level window, calls SetFocus, or connects.
class HiddenSharedApp:public CWinApp {
 int result_=1;
 BOOL InitInstance()override {
  AfxOleInit();AfxInitRichEdit2();LoadLibraryW(L"Msftedit.dll");
  std::ofstream out("WinRel/workspace-shared-pane-probe.txt");out.setf(std::ios::unitbuf);bool ok=true;
  auto check=[&](const char* label,bool value){out<<(value?"PASS ":"FAIL ")<<label<<"\n";ok=value&&ok;};
  WorkspaceTestFrame& frame=*new WorkspaceTestFrame;CWorkspacePane publicPane,pages;
  check("hidden native frame creates",frame.Create(NULL,"Hidden shared input fixture",WS_OVERLAPPEDWINDOW,CRect(0,0,900,700))!=FALSE);
  m_pMainWnd=&frame;frame.EnableDocking(CBRS_ALIGN_ANY);
  check("real channel panes create",publicPane.Create(&frame) && pages.Create(&frame,ID_WORKSPACE_BAR+1));
  int selected=0;CString target;std::wstring sent;bool accept=true;
  auto select=[&](CWorkspacePane& pane,bool)->bool{++selected;CString id,name;return pane.SharedInputIdentity(id,name);};
  publicPane.SetSharedInputHandler(select);pages.SetSharedInputHandler(select);
  check("separate transcripts open",publicPane.OpenTranscript("alpha","Public") && pages.OpenTranscript("beta","Pages"));
  auto send=[&](const CString& id,const std::wstring& command){target=id;sent=command;return accept;};
  publicPane.SetInputHandler(send);pages.SetInputHandler(send);
  publicPane.SetConnectionHandler([](const CString&){return true;});pages.SetConnectionHandler([](const CString&){return true;});
  publicPane.SetInputPrefix("alpha","Public",L"+pub ");pages.SetInputPrefix("beta","Pages",L"page Mira=");
  check("per-pane input and Send controls are hidden in shared mode",!(::GetWindowLong(publicPane.SharedInputWindow(),GWL_STYLE)&WS_VISIBLE) && !(::GetWindowLong(publicPane.GetDlgItem(3)->m_hWnd,GWL_STYLE)&WS_VISIBLE));
  ::SetWindowTextW(publicPane.SharedInputWindow(),L"hello \x03bb\xd83d\xdc09");::SetWindowTextW(pages.SharedInputWindow(),L"private draft");
  publicPane.SelectSharedInput(false);publicPane.SubmitSharedInput();
  wchar_t draft[256]={};::GetWindowTextW(pages.SharedInputWindow(),draft,256);
  check("native submission uses original connection and exact Unicode prefix once",target=="alpha" && sent==L"+pub hello \x03bb\xd83d\xdc09" && !::GetWindowTextLengthW(publicPane.SharedInputWindow()) && wcscmp(draft,L"private draft")==0);
  MSG key={};key.hwnd=publicPane.SharedInputWindow();key.message=WM_SYSKEYDOWN;key.wParam=VK_UP;
  check("Alt Up recalls only channel history",publicPane.PreTranslateMessage(&key));::GetWindowTextW(key.hwnd,draft,256);check("history excludes applied prefix",wcscmp(draft,L"hello \x03bb\xd83d\xdc09")==0);
  key.hwnd=pages.SharedInputWindow();pages.PreTranslateMessage(&key);::GetWindowTextW(key.hwnd,draft,256);check("other character history remains independent",wcscmp(draft,L"private draft")==0);
  accept=false;publicPane.SetInputErrorHandler([](const CString&){return std::wstring(L"Fictional plugin declined command");});publicPane.SubmitSharedInput();
  check("rejected command retains draft and visible feedback",::GetWindowTextLengthW(publicPane.SharedInputWindow())>0 && (::GetWindowLong(publicPane.GetDlgItem(8)->m_hWnd,GWL_STYLE)&WS_VISIBLE));
  WorkspaceRecord record;record.connectionId="alpha";record.sequence=1;WorkspaceRun run;run.text=L"Fictional output";record.runs.push_back(run);publicPane.AppendRecord("Public",record);
  const HWND owner=publicPane.GetSafeHwnd();
  publicPane.SetSharedInputHandler([&](CWorkspacePane& pane,bool){pane.DestroyWindow();return false;});
  key.hwnd=publicPane.GetDlgItem(100)->m_hWnd;key.message=WM_KEYDOWN;key.wParam=VK_UP;
  check("keyboard selection callback may destroy pane safely",publicPane.PreTranslateMessage(&key) && !::IsWindow(owner));
  frame.DestroyWindow();out<<(ok?"PASS":"FAIL")<<" hidden actual-pane API checks; no physical focus or transport acceptance\n";result_=ok?0:1;return FALSE;
 }
 int ExitInstance()override{CWinApp::ExitInstance();return result_;}
} app;
