#include "stdafx.h"
#include "WorkspaceFrame.h"
#include "WorkspacePane.h"
#include <afxtabbedpane.h>
#include <fstream>
#include <dbghelp.h>
#pragma comment(lib,"dbghelp.lib")
LONG CALLBACK DebugException(EXCEPTION_POINTERS* fault)
{
 std::ofstream report("WinRel/modern-exception.txt",std::ios::app);report<<std::hex<<fault->ExceptionRecord->ExceptionCode<<" at "<<fault->ExceptionRecord->ExceptionAddress<<"\n";
 SymInitialize(GetCurrentProcess(),NULL,TRUE);void* frames[30];USHORT count=CaptureStackBackTrace(0,30,frames,NULL);
 for(USHORT i=0;i<count;++i){char buffer[sizeof(SYMBOL_INFO)+512];SYMBOL_INFO* symbol=(SYMBOL_INFO*)buffer;memset(buffer,0,sizeof(buffer));symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=511;DWORD64 offset=0;if(SymFromAddr(GetCurrentProcess(),(DWORD64)frames[i],&offset,symbol))report<<symbol->Name<<" + "<<offset<<"\n";else report<<frames[i]<<"\n";}
 return EXCEPTION_CONTINUE_SEARCH;
}
class CompatibilityFrame:public CWorkspaceFrame {public: BOOL CloseDock(CDockablePane* pane){return OnCloseDockingPane(pane);} protected: virtual BOOL OnCreateClient(LPCREATESTRUCT cs,CCreateContext*) { if(!CreateClient(cs,NULL))return FALSE;return m_wndClientArea.SubclassWindow(m_hWndMDIClient); }};
int toolClosed=0;
LRESULT CALLBACK CloseToolProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
 if(message==WM_CLOSE){++toolClosed;DestroyWindow(window);return 0;}
 return DefWindowProcW(window,message,wp,lp);
}
class ProbeApp:public CWinApp
{
public:
 BOOL InitInstance()
 {
  AddVectoredExceptionHandler(1,DebugException);AfxOleInit();
  std::ofstream log("WinRel/modern-docking-compatibility.txt");log.setf(std::ios::unitbuf);log<<"start\n";
  CompatibilityFrame* frame=new CompatibilityFrame;
  bool ok=frame->Create(NULL,"Modern docking compatibility",WS_OVERLAPPEDWINDOW,CRect(50,50,1050,850))!=FALSE;
  log<<"frame created "<<ok<<"\n";m_pMainWnd=frame;frame->EnableWorkspaceDocking();log<<"docking enabled\n";
  CToolBar toolbar;CStatusBar status;CWorkspacePane pane,second;
  ok=ok && toolbar.Create(frame) && status.Create(frame);
  log<<"bars created "<<ok<<"\n";toolbar.SetButtons(NULL,2);toolbar.EnableDocking(CBRS_ALIGN_ANY);
  frame->CMDIFrameWnd::DockControlBar(&toolbar);
  ok=ok && pane.Create(frame,0xe900) && second.Create(frame,0xe901);
  ok=ok && pane.OpenTranscript("session-A","First") && second.OpenTranscript("session-B","Second");
  WorkspaceRecord record;record.connectionId="session-A";record.sequence=1;WorkspaceRun run;run.text=L"Native styled Unicode \u03bb\U0001f409";run.bold=true;run.foreground=RGB(31,201,81);record.runs.push_back(run);ok=ok && pane.AppendRecord("First",record);
  const HWND transcript=pane.GetDlgItem(100)->m_hWnd;
  pane.EnableDocking(CBRS_ALIGN_ANY);second.EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(&pane);
  log<<"panes created "<<ok<<"\n";CDockablePane* tabbed=NULL;second.AttachToTabWnd(&pane,DM_SHOW,TRUE,&tabbed);
  CTabbedPane* group=DYNAMIC_DOWNCAST(CTabbedPane,tabbed);
  CMDIChildWnd* world=new CMDIChildWnd;
  ok=ok && world->Create(NULL,"Classic world",WS_CHILD|WS_VISIBLE|WS_OVERLAPPEDWINDOW,CRect(0,0,350,250),frame);
  frame->ShowWindow(SW_SHOWNOACTIVATE);frame->RecalcLayout();
  CRect tr,pr,sr,md;toolbar.GetWindowRect(tr);if(group)group->GetWindowRect(pr);else pane.GetWindowRect(pr);status.GetWindowRect(sr);::GetWindowRect(frame->m_hWndMDIClient,md);
  ok=ok && group && group->GetTabWnd()->GetTabsNum()==2 && pr.top>=tr.bottom-2 && pr.bottom<=sr.top && md.top>=tr.bottom-2 && md.bottom<=sr.top;
  log<<"classic toolbar="<<tr.top<<","<<tr.bottom<<" pane="<<pr.top<<","<<pr.bottom<<" status="<<sr.top<<","<<sr.bottom<<" mdi="<<md.top<<","<<md.bottom<<" tabs="<<(group?group->GetTabWnd()->GetTabsNum():0)<<" "<<(ok?"PASS":"FAIL")<<"\n";
  if(group){group->GetTabWnd()->SwapTabs(0,1);ok=ok && group->GetTabWnd()->GetTabWnd(0)==&second;std::vector<WorkspacePaneLayout> saved;pane.GetLayouts(saved,false);ok=ok && saved.size()==1 && saved[0].tabOrder==1 && saved[0].groupConnectionId=="session-B" && saved[0].groupName=="Second";}
  log<<"native reorder captures stable cross-world tab membership/order "<<(ok?"PASS":"FAIL")<<"\n";
  CWorkspacePane split;ok=ok && split.Create(frame,0xe902) && split.OpenTranscript("session-C","Third");split.EnableDocking(CBRS_ALIGN_ANY);
  ok=ok && split.DockToWindow(group,CBRS_ALIGN_BOTTOM);frame->RecalcLayout();std::vector<WorkspacePaneLayout> splitLayout;split.GetLayouts(splitLayout,true);
  ok=ok && splitLayout.size()==1 && !splitLayout[0].splitName.IsEmpty() && splitLayout[0].splitAlignment==CBRS_ALIGN_BOTTOM;
  log<<"native split captures stable bounded neighbor relationship "<<(ok?"PASS":"FAIL")<<"\n";
  second.FloatPane(CRect(120,120,520,620),DM_SHOW);frame->RecalcLayout();
  ok=ok && second.IsFloating();second.AttachToTabWnd(&pane,DM_SHOW);frame->RecalcLayout();
  ok=ok && pane.GetDlgItem(100)->m_hWnd==transcript && pane.HasTranscript("session-A","First") && !pane.HasTranscript("session-B","Second");
  log<<"native tab attach/detach/reattach stable transcript "<<(ok?"PASS":"FAIL")<<"\n";
  frame->SetWorkspaceCloseHandler([](CWnd* window){CWorkspacePane* pane=DYNAMIC_DOWNCAST(CWorkspacePane,window);return pane?pane->CloseHostedTool():0;});
  WNDCLASSW toolClass={0};toolClass.lpfnWndProc=CloseToolProcedure;toolClass.hInstance=AfxGetInstanceHandle();toolClass.lpszClassName=L"ModernToolCloseFixture";RegisterClassW(&toolClass);
  CWorkspacePane tool;ok=ok && tool.Create(frame,0xe903);tool.EnableDocking(CBRS_ALIGN_ANY);
  const HWND host=tool.GetSafeHwnd();
  HWND child=CreateWindowExW(0,toolClass.lpszClassName,L"Review tool",WS_POPUP,0,0,200,300,NULL,NULL,AfxGetInstanceHandle(),NULL);
  ok=ok && tool.HostTool(child,"Review tool");tool.AttachToTabWnd(&pane,DM_SHOW,TRUE);CBaseTabbedPane* toolGroup=tool.GetParentTabbedPane();
  const BOOL defaultClose=frame->CloseDock(toolGroup);
  ok=ok && !defaultClose && toolClosed==1 && !IsWindow(child) && IsWindow(host) && pane.HasTranscript("session-A","First");
  log<<"docked active tool close forwards child WM_CLOSE, transcript survives "<<(ok?"PASS":"FAIL")<<"\n";
  for(int attempt=0;attempt<40;++attempt)
    {child=CreateWindowExW(0,toolClass.lpszClassName,L"Reopened tool",WS_POPUP,0,0,200,300,NULL,NULL,AfxGetInstanceHandle(),NULL);ok=ok && tool.HostTool(child,"Reopened tool");tool.FloatPane(CRect(170,170,670,620),DM_SHOW);tool.ShowPane(TRUE,FALSE,TRUE);CPaneFrameWnd* mini=tool.GetParentMiniFrame();mini->SendMessage(WM_CLOSE);ok=ok && !IsWindow(child) && tool.GetSafeHwnd()==host && !tool.IsToolHost();}
  log<<"floating frame WM_CLOSE child teardown and40 same-host reopen cycles closes="<<toolClosed<<" "<<(ok && toolClosed==41?"PASS":"FAIL")<<"\n";
  ok=ok && toolClosed==41;
  frame->DestroyWindow();ExitProcess(ok?0:1);return FALSE;
 }
};
ProbeApp app;
