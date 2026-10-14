#include "stdafx.h"
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include "sqlite3/sqlite3.h"
#include <fstream>
#undef App
struct FixtureStore {sqlite3* db;};FixtureStore App={NULL};
CString WorkspacePersistentWorld(const CString& id){const int marker=id.Find(":runtime:");return marker<0?id:id.Left(marker);}
bool WorkspaceLayoutSchema(){return App.db!=NULL;}
std::wstring WorkspaceLayoutWide(const char* text){if(!text)return L"";int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0);std::wstring out(n,L'\0');if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,&out[0],n);if(n)out.resize(n-1);return out;}
int published=0;void WorkspacePublishToPane(const CString&,const CString&,const WorkspaceRecord&){++published;}
class CMainFrame
{
public:
 CWorkspacePane pane;
 std::vector<WorkspaceRoute> m_workspaceRoutes;
 std::set<CString> m_workspaceRestored,m_workspaceClosedConnections;
 bool RouteWorkspaceRecord(const WorkspaceRecord& record);
 void RestoreWorkspaceRoutes(const CString& identity);
 bool AppendWorkspaceRecord(const CString& name,const WorkspaceRecord& record){return pane.AppendRecord(name,record);}
 bool AddWorkspaceRoute(const WorkspaceRoute& source,CString& error)
 {WorkspaceRoute route=source;if(!route.Compile(error))return false;RestoreWorkspaceRoutes(route.connectionId);if(!pane.OpenTranscript(route.connectionId,route.transcriptName))return false;m_workspaceRoutes.push_back(route);return true;}
};
// Generated from the production methods by run_cold_route_probe.ps1.
#include "cold-route-methods.inc"
class ColdRouteApp:public CWinApp
{
public:
 int result=1;
 BOOL InitInstance()
 {
  AfxOleInit();AfxInitRichEdit2();std::ofstream log("WinRel/workspace-cold-route-probe.txt");
  sqlite3_open(":memory:",&App.db);sqlite3_exec(App.db,"CREATE TABLE workspace_route_v1(world_id,name,pattern,regex,omit_main);INSERT INTO workspace_route_v1 VALUES('cold-world','ColdChat','cold-start',0,1);",NULL,NULL,NULL);
  WorkspaceTestFrame* frame=new WorkspaceTestFrame;bool ok=frame->Create(NULL,"Cold route native fixture",WS_OVERLAPPEDWINDOW,CRect(50,50,950,750))!=FALSE;frame->EnableWorkspaceDocking();
  CMainFrame bridge;ok=ok && bridge.pane.Create(frame);bridge.pane.EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(&bridge.pane);
  WorkspaceRecord record;record.connectionId="cold-world:runtime:fixture";record.sequence=1;WorkspaceRun run;run.text=L"first cold-start completed record";record.runs.push_back(run);
  ok=ok && bridge.m_workspaceRoutes.empty() && bridge.pane.IsEmpty() && bridge.RouteWorkspaceRecord(record);
  wchar_t text[256]={0};GetWindowTextW(bridge.pane.GetDlgItem(100)->m_hWnd,text,256);
  ok=ok && bridge.pane.HasTranscript(record.connectionId,"ColdChat") && wcsstr(text,L"first cold-start") && published==1 && bridge.m_workspaceRoutes.size()==1;
  ok=ok && !bridge.RouteWorkspaceRecord(record) && published==1;
  bridge.m_workspaceClosedConnections.insert(record.connectionId);record.sequence=2;ok=ok && !bridge.RouteWorkspaceRecord(record) && published==1;
  log<<"extracted production RouteWorkspaceRecord+RestoreWorkspaceRoutes: cold first-record pane hydration, recursive restore guard, once publication, closed endpoint rejection "<<(ok?"PASS":"FAIL")<<"\n";
  frame->DestroyWindow();sqlite3_close(App.db);result=ok?0:1;return FALSE;
 }
 int ExitInstance(){return result;}
};
ColdRouteApp app;
