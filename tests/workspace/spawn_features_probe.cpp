#include "stdafx.h"
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include "sqlite3/sqlite3.h"
#include <fstream>
#undef App
#undef Frame
class CMainFrame;
struct FixtureApp {sqlite3* db=NULL;CMainFrame* m_pMainWnd=NULL;} App;
#define Frame (*App.m_pMainWnd)
struct FixtureIdentity {CString id;unsigned __int64 sequence=0;};
class CMUSHclientDoc;
std::map<CMUSHclientDoc*,FixtureIdentity> identities;
struct FixtureAgentStore {unsigned appended=0;void append(const char*,const std::string&,unsigned __int64){++appended;}} agentStore;
bool PennEditorHandleOutput(const std::string&,const std::string& text){return text.compare(0,7,"PRIVATE")==0;}
std::string WorkspaceEndpointForWindow(const std::string& id){return id=="closed"?"":id;}
CString WorkspacePersistentWorld(const CString& id){int marker=id.Find(":runtime:");return marker<0?id:id.Left(marker);}
std::wstring WorkspaceLayoutWide(const char* text){if(!text)return {};int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,NULL,0);std::wstring result(count,L'\0');if(count){MultiByteToWideChar(CP_UTF8,0,text,-1,&result[0],count);result.resize(count-1);}return result;}
std::string WorkspaceLayoutUtf8(const std::wstring& text){int count=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),NULL,0,NULL,NULL);std::string result(count,'\0');if(count)WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),&result[0],count,NULL,NULL);return result;}
std::string utf8(const std::wstring& text){return WorkspaceLayoutUtf8(text);}
bool WorkspaceLayoutSchema(){return App.db && sqlite3_exec(App.db,"CREATE TABLE IF NOT EXISTS workspace_route_v1(world_id,name,pattern,regex,omit_main,PRIMARY KEY(world_id,name,pattern,regex))",NULL,NULL,NULL)==SQLITE_OK;}
unsigned published=0;void WorkspacePublishToPane(const CString&,const CString&,const WorkspaceRecord&){++published;}
bool acceleratorConflict=false,composition=false;
bool NativeShortcutConflict(CMainFrame&,unsigned short){return acceleratorConflict;}
bool WorkspaceCompositionActive(HWND){return composition;}
unsigned __int64 fixtureTime=10000;
unsigned __int64 FixtureTime(){return fixtureTime;}
class ProbePane:public CWorkspacePane {
public:
 size_t Count(const CString& id,const CString& name){auto* transcript=Find(id,name);return transcript?transcript->records.size():0;}
 void RefreshLabels(){OnTimer(1);}
};
class CMainFrame:public WorkspaceTestFrame {
public:
 void PostNcDestroy() override {} // Fixture owns its explicit final cleanup.
 ProbePane m_workspacePane;
 std::vector<CWorkspacePane*> m_workspaceSplits;
 std::vector<WorkspaceRoute> m_workspaceRoutes;
 std::set<CString> m_workspaceRestored,m_workspaceClosedConnections;
 workspace_spawn::ImageAssociation m_workspaceImages;
 std::map<std::pair<CString,CString>,workspace_spawn::Preferences> m_workspacePreferences;
 void ClearWorkspaceImageAssociation(const CString& id){m_workspaceImages.Clear((LPCTSTR)id);}
 workspace_spawn::Preferences WorkspaceTranscriptPreferences(const CString&,const CString&);
 bool WorkspaceShortcutAvailable(unsigned short,const CString&,const CString&,CString&);
 bool SetWorkspaceTranscriptPreferences(const CString&,const CString&,const workspace_spawn::Preferences&,CString&);
 bool HandleWorkspaceShortcut(MSG*);
 void ClearWorkspaceTranscript(const CString&,const CString&);
 bool FocusWorkspaceTranscript(size_t);
 bool FocusRelativeWorkspaceTranscript(int);
 bool AddWorkspaceRoute(const WorkspaceRoute&,CString&);
 bool RemoveWorkspaceRoute(const WorkspaceRoute&);
 bool RouteWorkspaceRecord(const WorkspaceRecord&);
 void RestoreWorkspaceRoutes(const CString&);
 bool OpenWorkspaceTranscript(const CString& id,const CString& name){
  if(m_workspacePane.HasTranscript(id,name))return true;
  for(auto* pane:m_workspaceSplits)if(pane->HasTranscript(id,name))return true;
  CWorkspacePane* pane=&m_workspacePane;
  if(!pane->IsEmpty()){auto* extra=new ProbePane;if(!extra->Create(this,0xE900+(UINT)m_workspaceSplits.size())){delete extra;return false;}extra->EnableDocking(CBRS_ALIGN_ANY);DockPane(extra);m_workspaceSplits.push_back(extra);pane=extra;}
  if(!pane->OpenTranscript(id,name))return false;pane->SetPreferences(id,name,WorkspaceTranscriptPreferences(id,name));return true;
 }
 bool AppendWorkspaceRecord(const CString& name,const WorkspaceRecord& record){bool ok=m_workspacePane.AppendRecord(name,record);for(auto* pane:m_workspaceSplits)ok=pane->AppendRecord(name,record)||ok;return ok;}
 ProbePane* Pane(const CString& id,const CString& name){if(m_workspacePane.HasTranscript(id,name))return &m_workspacePane;for(auto* pane:m_workspaceSplits)if(pane->HasTranscript(id,name))return (ProbePane*)pane;return NULL;}
};
struct SpawnNavigation {CWorkspacePane* pane;size_t index;WorkspacePaneLayout layout;int priority;};
#define GetTickCount64 FixtureTime
// Extracted unchanged production functions; transport/clock/accelerator inputs
// are fictional fixture adapters. All UI handles belong to this hidden frame.
#include "spawn-production.inc"
#undef GetTickCount64
WorkspaceRecord Record(const CString& id,unsigned __int64 sequence,const wchar_t* text,bool image=false){WorkspaceRecord record;record.connectionId=id;record.sequence=sequence;record.serverOutput=true;record.imageOnly=image;WorkspaceRun run;run.text=text;record.runs.push_back(run);return record;}
class SpawnFeaturesApp:public CWinApp {
 int result=1;
public:
 BOOL InitInstance(){
  AfxOleInit();AfxInitRichEdit2();INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
  wchar_t executable[32768];GetModuleFileNameW(NULL,executable,32768);std::wstring folder=executable;folder=folder.substr(0,folder.find_last_of(L"\\/")+1);std::ofstream log(folder+L"spawn-results.txt");unsigned checks=0,failed=0;
  auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;log<<"FAIL "<<label<<"\n";}};
  sqlite3_open(":memory:",&App.db);auto* frame=new CMainFrame;App.m_pMainWnd=frame;m_pMainWnd=frame;
  check(frame->Create(NULL,"Fictional hidden spawn fixture",WS_OVERLAPPEDWINDOW,CRect(20,20,1000,800))!=FALSE,"create");frame->EnableWorkspaceDocking();check(frame->m_workspacePane.Create(frame)!=FALSE,"pane");frame->m_workspacePane.EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(&frame->m_workspacePane);
  CString error;WorkspaceRoute a;a.connectionId="a:runtime:1";a.transcriptName="Public";a.pattern=L"[Public]";a.followingImages=true;a.omitFromMain=true;
  check(frame->AddWorkspaceRoute(a,error),"save route");WorkspaceRoute duplicate=a;duplicate.pattern=L"hello";duplicate.omitFromMain=false;check(frame->AddWorkspaceRoute(duplicate,error),"second matching rule");
  WorkspaceRoute b=a;b.transcriptName="Other";b.omitFromMain=false;check(frame->AddWorkspaceRoute(b,error),"second recipient");
  auto* publicPane=frame->Pane(a.connectionId,"Public");auto* otherPane=frame->Pane(a.connectionId,"Other");
  auto source=Record(a.connectionId,1,L"[Public] hello OMEGA \u03a9");check(frame->RouteWorkspaceRecord(source),"source omit");check(publicPane->Count(a.connectionId,"Public")==1 && otherPane->Count(a.connectionId,"Other")==1 && published==2,"deduplicate recipient/publication");
  for(unsigned i=2;i<=5;++i)check(frame->RouteWorkspaceRecord(Record(a.connectionId,i,L"[image]",true)),"four associated images");
  check(!frame->RouteWorkspaceRecord(Record(a.connectionId,6,L"[image]",true)) && publicPane->Count(a.connectionId,"Public")==5 && otherPane->Count(a.connectionId,"Other")==5,"association count cap");
  check(!frame->RouteWorkspaceRecord(Record("different:runtime:1",1,L"[image]",true)),"world isolation");
  frame->RouteWorkspaceRecord(Record(a.connectionId,7,L"[Public] hello"));fixtureTime+=2001;check(!frame->RouteWorkspaceRecord(Record(a.connectionId,8,L"[image]",true)),"association time cap");
  frame->RouteWorkspaceRecord(Record(a.connectionId,9,L"[Public] hello"));frame->RouteWorkspaceRecord(Record(a.connectionId,10,L"unmatched server text"));check(!frame->RouteWorkspaceRecord(Record(a.connectionId,11,L"[image]",true)),"unmatched text clears");
  frame->RouteWorkspaceRecord(Record(a.connectionId,12,L"[Public] hello"));auto note=Record(a.connectionId,13,L"local note");note.serverOutput=false;frame->RouteWorkspaceRecord(note);check(!frame->RouteWorkspaceRecord(Record(a.connectionId,14,L"[image]",true)),"local note clears");
  frame->RouteWorkspaceRecord(Record(a.connectionId,15,L"[Public] hello"));check(frame->AddWorkspaceRoute(a,error),"edit route");check(!frame->RouteWorkspaceRecord(Record(a.connectionId,16,L"[image]",true)),"route edit clears");
  frame->RouteWorkspaceRecord(Record(a.connectionId,17,L"[Public] hello"));check(!frame->RouteWorkspaceRecord(Record(a.connectionId,19,L"[image]",true)),"missing sequence fails closed");
  auto* doc=(CMUSHclientDoc*)0x1234;FixtureIdentity identity;identity.id="private:runtime:1";identities[doc]=identity;WorkspaceRoute privateRoute=a;privateRoute.connectionId=identity.id;check(frame->AddWorkspaceRoute(privateRoute,error),"private path route");
  auto* privatePane=frame->Pane(identity.id,"Public");auto privateSource=Record(identity.id,0,L"[Public] marker");check(WorkspacePublishRecord(doc,privateSource,false),"production publisher matching source");
  check(WorkspacePublishRecord(doc,Record(identity.id,0,L"PRIVATE correlated reply"),false),"private reply consumed");check(!WorkspacePublishRecord(doc,Record(identity.id,0,L"[image]",true),false) && privatePane->Count(identity.id,"Public")==1,"private reply closes association before sinks");
  WorkspacePublishRecord(doc,privateSource,false);WorkspacePublishRecord(doc,Record(identity.id,0,L"plugin-cancelled hidden text"),true);WorkspacePublishRecord(doc,Record(identity.id,0,L"[image]",true),false);check(privatePane->Count(identity.id,"Public")==2,"plugin-cancelled/omitted reply closes association");
  workspace_spawn::Preferences settings;settings.mention=L"omega \u03c9";settings.priority=25;settings.shortcut=MAKEWORD('J',HOTKEYF_CONTROL|HOTKEYF_ALT);
  check(frame->SetWorkspaceTranscriptPreferences(a.connectionId,"Public",settings,error),"save preferences");check(frame->WorkspaceTranscriptPreferences(a.connectionId,"Public").priority==25,"priority cached");frame->m_workspacePreferences.clear();check(frame->WorkspaceTranscriptPreferences(a.connectionId,"Public").mention==settings.mention,"preferences reload");
  auto mentioned=Record(a.connectionId,20,L"OMEGA \u03a9 is mentioned");check(publicPane->AppendRecord("Public",mentioned) && publicPane->MentionCount(a.connectionId,"Public")==1,"Unicode case-insensitive badge");
  check(!publicPane->AppendRecord("Public",mentioned) && publicPane->MentionCount(a.connectionId,"Public")==1,"duplicate no mention replay");mentioned.sequence=21;mentioned.serverOutput=false;publicPane->AppendRecord("Public",mentioned);check(publicPane->MentionCount(a.connectionId,"Public")==1,"no local-note mention");mentioned.sequence=22;mentioned.serverOutput=true;mentioned.imageOnly=true;publicPane->AppendRecord("Public",mentioned);check(publicPane->MentionCount(a.connectionId,"Public")==1,"no image fallback mention");publicPane->RefreshLabels();CString caption;publicPane->GetWindowText(caption);check(caption.Find("mentions: 1")>=0,"caption badge");check(frame->FocusWorkspaceTranscript(0) && publicPane->MentionCount(a.connectionId,"Public")==0,"priority focus clears badge");
  check(!frame->WorkspaceShortcutAvailable(settings.shortcut,a.connectionId,"Other",error),"saved shortcut duplicate rejected");acceleratorConflict=true;check(!frame->WorkspaceShortcutAvailable(MAKEWORD('K',6),a.connectionId,"Public",error),"native accelerator conflict rejected");acceleratorConflict=false;
  check(!frame->SetWorkspaceTranscriptPreferences("closed","Public",settings,error),"closed original settings rejected");
  auto order=WorkspaceNavigation(*frame);check(order.size()>=2 && order.front().layout.name=="Public" && order.front().priority==25,"priority navigation order");check(frame->FocusRelativeWorkspaceTranscript(1),"next transcript");check(frame->FocusRelativeWorkspaceTranscript(-1) && ::IsChild(publicPane->m_hWnd,::GetFocus()),"previous returns to priority transcript");check(frame->FocusRelativeWorkspaceTranscript(-1),"previous wraps");check(frame->FocusRelativeWorkspaceTranscript(1) && ::IsChild(publicPane->m_hWnd,::GetFocus()),"next wraps to first");
  auto invalid=settings;invalid.priority=101;check(!frame->SetWorkspaceTranscriptPreferences(a.connectionId,"Public",invalid,error) && frame->WorkspaceTranscriptPreferences(a.connectionId,"Public").priority==25,"invalid priority preserves settings");invalid=settings;invalid.mention=L"bad\nmention";check(!frame->SetWorkspaceTranscriptPreferences(a.connectionId,"Public",invalid,error),"invalid mention rejected");
  frame->RouteWorkspaceRecord(Record(a.connectionId,30,L"[Public] marker"));frame->ClearWorkspaceTranscript(a.connectionId,"Public");check(!frame->RouteWorkspaceRecord(Record(a.connectionId,31,L"[image]",true)),"explicit transcript clear closes image association");
  BYTE originalKeys[256]={};GetKeyboardState(originalKeys);BYTE keys[256]={};keys[VK_CONTROL]=keys[VK_LCONTROL]=keys[VK_MENU]=keys[VK_LMENU]=0x80;SetKeyboardState(keys);MSG key={};key.hwnd=publicPane->GetDlgItem(2)->m_hWnd;key.message=WM_KEYDOWN;key.wParam='J';check(frame->HandleWorkspaceShortcut(&key),"configured key dispatch");keys[VK_RMENU]=0x80;SetKeyboardState(keys);check(!frame->HandleWorkspaceShortcut(&key),"AltGr no focus theft");keys[VK_RMENU]=0;keys[VK_LWIN]=0x80;SetKeyboardState(keys);check(!frame->HandleWorkspaceShortcut(&key),"Windows key no focus theft");keys[VK_LWIN]=0;SetKeyboardState(keys);composition=true;check(!frame->HandleWorkspaceShortcut(&key),"IME composition no focus theft");composition=false;acceleratorConflict=true;check(!frame->HandleWorkspaceShortcut(&key),"runtime accelerator conflict rechecked");acceleratorConflict=false;
  settings.shortcut=MAKEWORD(VK_F12,HOTKEYF_ALT);check(frame->SetWorkspaceTranscriptPreferences(a.connectionId,"Public",settings,error),"alt function chord saved");memset(keys,0,sizeof(keys));keys[VK_MENU]=keys[VK_LMENU]=0x80;SetKeyboardState(keys);key.message=WM_SYSKEYDOWN;key.wParam=VK_F12;check(frame->HandleWorkspaceShortcut(&key),"Alt function WM_SYSKEYDOWN dispatch");SetKeyboardState(originalKeys);
  for(unsigned modifiers:{1U,2U,3U,4U,5U,6U,7U})for(unsigned function:{0x73U,0x75U,0x79U})check(!workspace_spawn::ShortcutValid((unsigned short)((modifiers<<8)|function)),"native close/switch/maximize reserved");
  check(!workspace_spawn::ShortcutValid(MAKEWORD('C',HOTKEYF_CONTROL)) && !workspace_spawn::ShortcutValid(MAKEWORD(VK_F1,HOTKEYF_CONTROL|HOTKEYF_SHIFT)),"editing and existing slots reserved");
  frame->m_workspaceRoutes.clear();frame->m_workspaceRestored.clear();frame->RestoreWorkspaceRoutes(a.connectionId);check(frame->m_workspaceRoutes.size()==3 && frame->m_workspaceRoutes[0].followingImages,"following-image route setting restores from SQLite");
  log<<"checks="<<checks<<" failed="<<failed<<"\n";frame->DestroyWindow();for(auto* pane:frame->m_workspaceSplits)delete pane;frame->m_workspaceSplits.clear();delete frame;App.m_pMainWnd=NULL;m_pMainWnd=NULL;sqlite3_close(App.db);result=failed?1:0;return FALSE;
 }
 int ExitInstance(){return result;}
};
SpawnFeaturesApp spawnFeaturesApp;
