#include "stdafx.h"
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include "sqlite3/sqlite3.h"
#include <fstream>
#undef App
#undef Frame
struct FixtureApp {sqlite3* db=NULL;} App;
CString WorkspacePersistentWorld(const CString& id){int marker=id.Find(":runtime:");return marker<0?id:id.Left(marker);}
std::wstring WorkspaceLayoutWide(const char* value){if(!value)return {};const int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,-1,NULL,0);std::wstring result(size,L'\0');if(size){MultiByteToWideChar(CP_UTF8,0,value,-1,&result[0],size);result.resize(size-1);}return result;}
std::string WorkspaceLayoutUtf8(const std::wstring& value){int size=WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),NULL,0,NULL,NULL);std::string result(size,'\0');if(size)WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),&result[0],size,NULL,NULL);return result;}
std::string WorkspaceEndpointForWindow(const std::string& id){return id;}
class ProbePane:public CWorkspacePane {
public:
 void PaintRetained(){auto* transcript=Find("alpha:runtime:1","Public");Rebuild(*transcript);}
 void Tick(){OnTimer(1);}
 void Frozen(bool value){Find("alpha:runtime:1","Public")->paused=value;}
 size_t LinkCount(){return Find("alpha:runtime:1","Public")->links.size();}
 HWND Text(){return GetDlgItem(100)->GetSafeHwnd();}
};
class CMainFrame {
public:
 ProbePane m_workspacePane;std::vector<CWorkspacePane*> m_workspaceSplits;
 std::map<std::pair<CString,CString>,workspace_spawn::Preferences> m_workspacePreferences;
 workspace_spawn::Preferences WorkspaceTranscriptPreferences(const CString&,const CString&);
 bool WorkspaceShortcutAvailable(unsigned short,const CString&,const CString&,CString&){return true;}
 bool SetWorkspaceTranscriptPreferences(const CString&,const CString&,const workspace_spawn::Preferences&,CString&);
};
#include "channel-appearance-production.inc"
CHARFORMAT2W Format(HWND window,long first,long last){::SendMessageW(window,EM_SETSEL,first,last);CHARFORMAT2W value={};value.cbSize=sizeof(value);::SendMessageW(window,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&value);return value;}
class AppearanceApp:public CWinApp {
 int result_=1;
 BOOL InitInstance() override {
  AfxOleInit();AfxInitRichEdit2();LoadLibraryW(L"Msftedit.dll");std::ofstream out("channel-appearance-results.txt");out.setf(std::ios::unitbuf);int count=0,failed=0;
  auto check=[&](const char* label,bool passed){++count;if(!passed)++failed;out<<(passed?"PASS ":"FAIL ")<<label<<'\n';};
  auto* frame=new WorkspaceTestFrame;m_pMainWnd=frame;ProbePane pane,other;
  check("hidden frame and channel panes create",frame->Create(NULL,"Hidden fictional channel appearance",WS_OVERLAPPEDWINDOW,CRect(0,0,900,700)) && pane.Create(frame) && other.Create(frame,ID_WORKSPACE_BAR+1));
  workspace_spawn::Appearance world;world.face=L"Courier New";world.pointSize=14;world.foreground=RGB(24,35,46);world.background=RGB(248,246,241);
  CString queried;pane.SetAppearanceHandler([&](const CString& id,workspace_spawn::Appearance& value){queried=id;value=world;return true;});
  workspace_spawn::Appearance second=world;second.face=L"Consolas";second.background=RGB(7,11,18);other.SetAppearanceHandler([&](const CString&,workspace_spawn::Appearance& value){value=second;return true;});
  check("empty channel opens before any incoming output",pane.OpenTranscript("alpha:runtime:1","Public") && other.OpenTranscript("beta:runtime:2","Pages"));
  auto initial=Format(pane.Text(),0,0);workspace_spawn::Appearance observed;
  check("empty channel immediately inherits owning world font size and colors",queried=="alpha:runtime:1" && pane.GetAppearance("alpha:runtime:1","Public",observed) && observed==world && !wcscmp(initial.szFaceName,world.face.c_str()) && initial.yHeight==280 && initial.crTextColor==world.foreground && initial.crBackColor==world.background);
  check("native RichEdit canvas background is inherited before output",(COLORREF)::SendMessageW(pane.Text(),EM_SETBKGNDCOLOR,FALSE,world.background)==world.background);
  check("second world's channel inherits independently",other.GetAppearance("beta:runtime:2","Pages",observed) && observed==second);
  WorkspaceRecord record;record.connectionId="alpha:runtime:1";record.sequence=1;record.hasSurfaceColors=true;record.surfaceForeground=RGB(225,225,225);record.surfaceBackground=RGB(0,0,0);
  WorkspaceRun plain;plain.text=L"plain";plain.defaultForeground=plain.defaultBackground=true;plain.action="look";plain.actionType=1;record.runs.push_back(plain);
  WorkspaceRun ansi;ansi.text=L"RED";ansi.foreground=RGB(240,20,30);ansi.background=RGB(0,40,130);ansi.bold=true;record.runs.push_back(ansi);
  check("styled record appends",pane.AppendRecord("Public",record));pane.PaintRetained();auto base=Format(pane.Text(),0,5),explicitColor=Format(pane.Text(),5,8);
  check("default text uses inherited face point size and base colors",!wcscmp(base.szFaceName,L"Courier New") && base.yHeight==280 && base.crTextColor==world.foreground && base.crBackColor==world.background);
  check("explicit ANSI foreground background and bold survive inheritance",explicitColor.crTextColor==ansi.foreground && explicitColor.crBackColor==ansi.background && (explicitColor.dwEffects&CFE_BOLD));
  check("clickable link ranges survive inherited rendering",pane.LinkCount()==1 && (base.dwEffects&CFE_LINK));
  ::SendMessageW(pane.Text(),EM_SETSEL,1,4);world.face=L"Consolas";world.pointSize=16;world.foreground=RGB(10,60,80);world.background=RGB(232,240,245);pane.Tick();
  DWORD first=0,last=0;::SendMessageW(pane.Text(),EM_GETSEL,(WPARAM)&first,(LPARAM)&last);base=Format(pane.Text(),0,5);explicitColor=Format(pane.Text(),5,8);
  check("live Main changes apply without new output and preserve selection",pane.GetAppearance("alpha:runtime:1","Public",observed) && observed==world && base.yHeight==320 && base.crTextColor==world.foreground && first==1 && last==4);
  check("live updates preserve ANSI and links",explicitColor.crTextColor==ansi.foreground && explicitColor.crBackColor==ansi.background && pane.LinkCount()==1);
  workspace_spawn::Preferences preferences;preferences.appearanceOverride=true;preferences.appearance=world;preferences.appearance.face=L"Lucida Console";preferences.appearance.pointSize=11;preferences.appearance.foreground=RGB(210,220,230);preferences.appearance.background=RGB(15,19,25);
  check("explicit channel override applies",pane.SetPreferences("alpha:runtime:1","Public",preferences));world.pointSize=20;pane.Tick();
  check("explicit channel ignores later Main appearance changes",pane.GetAppearance("alpha:runtime:1","Public",observed) && observed==preferences.appearance);
  preferences.appearanceOverride=false;pane.SetPreferences("alpha:runtime:1","Public",preferences);
  check("reset to Main restores current world appearance",pane.GetAppearance("alpha:runtime:1","Public",observed) && observed==world);
  pane.Tick();check("appearance updates do not add output",::GetWindowTextLengthW(pane.Text())==10);
  pane.Frozen(true);record.sequence=2;pane.AppendRecord("Public",record);const int frozenLength=::GetWindowTextLengthW(pane.Text());world.pointSize=13;pane.Tick();
  check("live font update does not expose buffered output in a paused pane",::GetWindowTextLengthW(pane.Text())==frozenLength);
  pane.Frozen(false);pane.PaintRetained();check("paused output is still retained and reappears on resume",::GetWindowTextLengthW(pane.Text())==20 && Format(pane.Text(),0,5).yHeight==260);
  ProbePane moved;check("hidden move target creates",moved.Create(frame,ID_WORKSPACE_BAR+2));preferences.appearanceOverride=true;preferences.appearance.face=L"Lucida Console";preferences.appearance.pointSize=11;pane.SetPreferences("alpha:runtime:1","Public",preferences);
  check("moving channel preserves appearance override and styled links",pane.MoveSelectedTo(moved) && moved.GetAppearance("alpha:runtime:1","Public",observed) && observed==preferences.appearance && moved.LinkCount()==2);
  sqlite3_open(":memory:",&App.db);CMainFrame settings;CString error;preferences.appearanceOverride=true;preferences.appearance=world;preferences.priority=17;
  check("appearance and ordinary settings save atomically",settings.SetWorkspaceTranscriptPreferences("alpha:runtime:1","Public",preferences,error));settings.m_workspacePreferences.clear();
  auto saved=settings.WorkspaceTranscriptPreferences("alpha:runtime:99","Public");check("override reloads by stable world and channel across runtime identities",saved.appearanceOverride && saved.appearance==world && saved.priority==17);
  check("saved appearance is isolated from another channel",!settings.WorkspaceTranscriptPreferences("alpha:runtime:1","Pages").appearanceOverride);
  preferences.appearanceOverride=false;check("reset deletes saved override",settings.SetWorkspaceTranscriptPreferences("alpha:runtime:1","Public",preferences,error));settings.m_workspacePreferences.clear();check("reset remains inherited after reload",!settings.WorkspaceTranscriptPreferences("alpha:runtime:2","Public").appearanceOverride);
  preferences.appearanceOverride=true;preferences.appearance.pointSize=0;check("invalid override is rejected",!settings.SetWorkspaceTranscriptPreferences("alpha:runtime:1","Public",preferences,error));
  preferences.appearance=world;sqlite3_exec(App.db,"CREATE TRIGGER reject_appearance BEFORE INSERT ON workspace_spawn_appearance_v1 BEGIN SELECT RAISE(FAIL,'fictional save failure'); END",NULL,NULL,NULL);preferences.priority=42;
  check("failed appearance write is reported",!settings.SetWorkspaceTranscriptPreferences("alpha:runtime:1","Public",preferences,error));settings.m_workspacePreferences.clear();check("failed appearance write rolls back ordinary settings too",settings.WorkspaceTranscriptPreferences("alpha:runtime:1","Public").priority==17);
  sqlite3_close(App.db);App.db=NULL;frame->DestroyWindow();out<<"checks="<<count<<" failed="<<failed<<'\n';result_=failed?1:0;return FALSE;
 }
 int ExitInstance() override {CWinApp::ExitInstance();return result_;}
} app;
