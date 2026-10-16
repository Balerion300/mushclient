// Executes the extracted production keyboard router against fictional native
// controls on a verified private desktop. No key injection or game traffic.
#include "stdafx.h"
#include "workspace/WorkspaceShell.h"
#include <fstream>
#include <vector>
#include <algorithm>
struct WorkspacePaneLayout {CString connectionId,name;};
class CMUSHclientDoc:public CDocument {DECLARE_DYNCREATE(CMUSHclientDoc) public:CMUSHclientDoc(){m_bAutoDelete=FALSE;}CString identity;};
IMPLEMENT_DYNCREATE(CMUSHclientDoc,CDocument)
class CSendView:public CView {DECLARE_DYNCREATE(CSendView) public:void OnDraw(CDC*)override{};void PostNcDestroy()override{};};
IMPLEMENT_DYNCREATE(CSendView,CView)
class CMUSHView:public CView {DECLARE_DYNCREATE(CMUSHView) public:void OnDraw(CDC*)override{};void PostNcDestroy()override{};};
IMPLEMENT_DYNCREATE(CMUSHView,CView)
CString WorkspaceConnectionId(CMUSHclientDoc* doc){return doc->identity;}
namespace workspace_composer {bool composing=false;bool CompositionActive(HWND){return composing;}}
class CWorkspacePane:public CWnd {
public:bool visible=true;HWND input=NULL;CString identity,name;std::vector<WorkspacePaneLayout> layouts;
 bool IsWindowVisible()const{return visible;} HWND SharedInputWindow()const{return input;}
 bool SharedInputIdentity(CString& id,CString& channel)const{id=identity;channel=name;return !identity.IsEmpty();}
 void GetLayouts(std::vector<WorkspacePaneLayout>& result,bool)const{result=layouts;}
};
class CMainFrame:public CWnd {
public:CWorkspacePane m_workspacePane;std::vector<CWorkspacePane*> m_workspaceSplits;int calls=0;std::string lastWorld,lastName;
 bool CycleWorkspaceInput(MSG* message);
 void WorkspaceShellAction(int,const std::string& world,const std::string& name){++calls;lastWorld=world;lastName=name;}
};
static bool control=false,alt=false,shift=false;
SHORT FixtureKeyState(int key){return ((key==VK_CONTROL&&control)||(key==VK_MENU&&alt)||(key==VK_SHIFT&&shift))?(SHORT)0x8000:0;}
#define GetKeyState FixtureKeyState
#include "cycle-input-production.inc"
#undef GetKeyState
class CycleProbe:public CWinApp {
 int result_=1;
 BOOL InitInstance()override {
  std::ofstream out("cycle-input-results.txt");bool ok=true;auto check=[&](const char* label,bool value){out<<(value?"PASS ":"FAIL ")<<label<<"\n";out.flush();ok=value&&ok;};
  CMainFrame frame;frame.CreateEx(0,AfxRegisterWndClass(0),"Fictional keyboard router",WS_POPUP,0,0,500,500,NULL,NULL);m_pMainWnd=&frame;
  auto& first=frame.m_workspacePane;first.CreateEx(0,AfxRegisterWndClass(0),"Channels A",WS_CHILD,0,0,200,200,frame.m_hWnd,(HMENU)1);first.identity="A:runtime:1";first.name="Public";first.layouts={{first.identity,"Public"},{first.identity,"Roleplay"}};
  CWorkspacePane second;second.CreateEx(0,AfxRegisterWndClass(0),"Channels B",WS_CHILD,200,0,200,200,frame.m_hWnd,(HMENU)2);second.identity="B:runtime:2";second.name="Public";second.layouts={{second.identity,"Public"}};frame.m_workspaceSplits.push_back(&second);
  CEdit borrowed,search;borrowed.Create(WS_CHILD|ES_MULTILINE,CRect(0,300,300,350),&frame,3);first.input=borrowed.m_hWnd;search.Create(WS_CHILD,CRect(0,0,100,20),&first,4);
  MSG message={};message.hwnd=borrowed.m_hWnd;message.message=WM_KEYDOWN;message.wParam=VK_TAB;control=true;
  check("borrowed reparented input cycles to same character's next channel",frame.CycleWorkspaceInput(&message)&&frame.lastWorld=="A:runtime:1"&&frame.lastName=="Roleplay");
  shift=true;check("Ctrl Shift Tab goes backwards from first channel to Main",frame.CycleWorkspaceInput(&message)&&frame.lastName.empty());shift=false;
  first.name="Roleplay";check("forward cycle wraps to Main",frame.CycleWorkspaceInput(&message)&&frame.lastName.empty());first.name="Public";
  control=false;alt=true;message.message=WM_SYSKEYDOWN;message.wParam=VK_NEXT;check("Alt PageDown shares forward pane switching",frame.CycleWorkspaceInput(&message)&&frame.lastName=="Roleplay");
  message.wParam=VK_PRIOR;check("Alt PageUp shares backward pane switching",frame.CycleWorkspaceInput(&message)&&frame.lastName.empty());
  control=true;message.wParam=VK_TAB;check("AltGr combinations remain untouched",!frame.CycleWorkspaceInput(&message));alt=false;message.message=WM_KEYDOWN;
  message.lParam=1L<<30;check("held shortcut repeats do not churn focus",!frame.CycleWorkspaceInput(&message));message.lParam=0;
  workspace_composer::composing=true;check("active IME composition is not interrupted",!frame.CycleWorkspaceInput(&message));workspace_composer::composing=false;
  frame.EnableWindow(FALSE);check("disabled owner during modal dialog does not cycle",!frame.CycleWorkspaceInput(&message));frame.EnableWindow(TRUE);
  message.hwnd=search.m_hWnd;check("channel search edit retains its own keyboard behavior",!frame.CycleWorkspaceInput(&message));message.hwnd=borrowed.m_hWnd;
  first.visible=false;check("hidden panes do not become cycle destinations or cross characters",!frame.CycleWorkspaceInput(&message));first.visible=true;
  CMUSHclientDoc doc;doc.identity="A:runtime:1";CSendView mainView;CCreateContext context={};context.m_pCurrentDoc=&doc;mainView.Create(NULL,NULL,WS_CHILD,CRect(0,350,400,400),&frame,7,&context);message.hwnd=mainView.m_hWnd;
  check("original Main input participates before legacy world Ctrl Tab",frame.CycleWorkspaceInput(&message)&&frame.lastWorld=="A:runtime:1"&&frame.lastName=="Public");
  first.visible=false;check("Main falls through to legacy world navigation with no visible channels",!frame.CycleWorkspaceInput(&message));
  mainView.DestroyWindow();borrowed.DestroyWindow();search.DestroyWindow();second.DestroyWindow();first.DestroyWindow();frame.DestroyWindow();m_pMainWnd=NULL;result_=ok?0:1;return FALSE;
 }
 int ExitInstance()override{return result_;}
};CycleProbe app;
