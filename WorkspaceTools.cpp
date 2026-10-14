#include "stdafx.h"
#include "MUSHclient.h"
#include "doc.h"
#include "mainfrm.h"
#include "WorkspaceIntegration.h"
#include "WorkspaceTools.h"
#include "workspace/PennEditorWindow.h"
#include "AgentBridgeHost.h"
#include "WorkspaceSceneDirector.h"
#include "workspace/AgentReviewPane.h"
#include <map>
#include <regex>

namespace {
struct GrantUi { agent_workspace::Counter session; std::vector<std::string> windows; bool drafts; std::shared_ptr<agent_workspace::BridgeHost> bridge; };
std::map<std::string,GrantUi> grants;
struct Ui { unsigned kind; HWND window,list,name,input,status; std::vector<std::string> ids; std::string scene; };
std::wstring wide(const std::string& s) {
 int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),NULL,0);std::wstring v(n,L'\0');
 if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&v[0],n);return v;
}
std::string utf8(const std::wstring& s) {
 int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),NULL,0,NULL,NULL);std::string v(n,'\0');
 if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),&v[0],n,NULL,NULL);return v;
}
std::string text(HWND w) {int n=GetWindowTextLengthW(w);std::wstring s(n+1,L'\0');GetWindowTextW(w,&s[0],n+1);s.resize(n);return utf8(s);}
HWND control(Ui& u,const wchar_t* cls,const wchar_t* caption,DWORD style,int id,int x,int y,int w,int h) {
 HWND c=CreateWindowExW(WS_EX_CLIENTEDGE,cls,caption,WS_CHILD|WS_VISIBLE|style|(!wcscmp(cls,L"STATIC")?0:WS_TABSTOP),x,y,w,h,u.window,(HMENU)(INT_PTR)id,AfxGetInstanceHandle(),NULL);
 SendMessage(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return c;
}
void status(Ui& u,const std::string& value) {SetWindowTextW(u.status,wide(value).c_str());}
std::string agent(Ui& u) {return text(u.name);}
void refresh(Ui& u) {
 SendMessage(u.list,LB_RESETCONTENT,0,0);u.ids.clear();
 if(u.kind==59002) {
  POSITION pos=App.m_pWorldDocTemplate->GetFirstDocPosition();
  while(pos) {
   CMUSHclientDoc* d=(CMUSHclientDoc*)App.m_pWorldDocTemplate->GetNextDoc(pos);
   WorkspaceConnectionId(d);
  }
  for(const auto& item:WorkspaceAgentStore().humanWindows()) {
   u.ids.push_back(item.id);std::string label=item.world+" / "+item.title;
   SendMessageW(u.list,LB_ADDSTRING,0,(LPARAM)wide(label).c_str());
  }
 } else if(u.kind==59003) {
  agent_workspace::Grant summary=WorkspaceAgentStore().humanWorkspace(agent(u));
  const auto& messages=summary.messages;
  for(size_t i=0;i<messages.size();++i) {
   std::string line=messages[i].role+": "+messages[i].text;
   SendMessageW(u.list,LB_ADDSTRING,0,(LPARAM)wide(line).c_str());
  }
 } else {
  const std::vector<agent_workspace::Draft>& drafts=WorkspaceAgentStore().drafts();
  for(size_t i=0;i<drafts.size();++i) {u.ids.push_back(drafts[i].id);std::string line=drafts[i].agent+" / "+drafts[i].window+" / "+drafts[i].id;SendMessageW(u.list,LB_ADDSTRING,0,(LPARAM)wide(line).c_str());}
 }
}
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
 Ui* u=(Ui*)GetWindowLongPtrW(w,GWLP_USERDATA);
 if(msg==WM_NCCREATE) {u=(Ui*)((CREATESTRUCTW*)lp)->lpCreateParams;u->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)u);}
 if(!u)return DefWindowProcW(w,msg,wp,lp);
 try {
 if(msg==WM_CREATE) {
  control(*u,L"STATIC",L"Agent name",0,0,12,10,110,22);
  u->name=control(*u,L"EDIT",L"Assistant",ES_AUTOHSCROLL,10,125,8,350,24);
  SendMessageW(u->name,EM_SETLIMITTEXT,64,0);
  u->list=control(*u,L"LISTBOX",L"",WS_VSCROLL|LBS_NOTIFY|(u->kind==59002?LBS_EXTENDEDSEL:0),11,12,45,740,265);
  u->input=control(*u,L"EDIT",L"",ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,12,12,320,740,90);
  SendMessageW(u->input,EM_SETLIMITTEXT,32768,0);
  control(*u,L"BUTTON",u->kind==59002?L"Enable selected output":u->kind==59003?L"Post to agent conversation":L"Review selected unsent draft",BS_PUSHBUTTON,20,12,425,215,30);
  control(*u,L"BUTTON",L"Refresh",BS_PUSHBUTTON,21,235,425,90,30);
  control(*u,L"BUTTON",L"Revoke this agent",BS_PUSHBUTTON,22,335,425,160,30);
  control(*u,L"BUTTON",L"Revoke all",BS_PUSHBUTTON,23,505,425,110,30);
  if(u->kind==59002) control(*u,L"BUTTON",L"Allow unsent drafts and reviewed edits",BS_AUTOCHECKBOX,24,12,465,480,28);
  u->status=control(*u,L"STATIC",L"Access starts disabled. Select only the output you want to share.",0,0,12,505,740,55);
  refresh(*u);SetTimer(w,1,1000,NULL);return 0;
 }
 if(msg==WM_TIMER && u->kind==59003) {refresh(*u);return 0;}
 if(msg==WM_GETMINMAXINFO) {auto limits=(MINMAXINFO*)lp;limits->ptMinTrackSize.x=340;limits->ptMinTrackSize.y=440;return 0;}
 if(msg==WM_SIZE && u->list) {
  int width=LOWORD(lp),height=HIWORD(lp);int body=max(150,width-24);
  MoveWindow(u->name,100,8,max(60,width-112),24,TRUE);
  bool compact=body<620;int footer=compact?205:145;
  int listHeight=max(40,height-footer-110),inputHeight=50;
  MoveWindow(u->list,12,40,body,listHeight,TRUE);
  MoveWindow(u->input,12,50+listHeight,body,inputHeight,TRUE);
  int y=110+listHeight;
  if(compact) {
    MoveWindow(GetDlgItem(w,20),12,y,body,28,TRUE);
    int half=(body-8)/2;
    MoveWindow(GetDlgItem(w,21),12,y+33,half,28,TRUE);
    MoveWindow(GetDlgItem(w,23),20+half,y+33,half,28,TRUE);
    MoveWindow(GetDlgItem(w,22),12,y+66,body,28,TRUE);y+=99;
  } else {
    MoveWindow(GetDlgItem(w,20),12,y,215,28,TRUE);
    MoveWindow(GetDlgItem(w,21),235,y,80,28,TRUE);
    MoveWindow(GetDlgItem(w,22),325,y,155,28,TRUE);
    MoveWindow(GetDlgItem(w,23),490,y,120,28,TRUE);y+=33;
  }
  if(u->kind==59002){MoveWindow(GetDlgItem(w,24),12,y,body,35,TRUE);y+=38;}
  MoveWindow(u->status,12,y,body,max(35,height-y-10),TRUE);return 0;
 }
 if(msg==WM_COMMAND && HIWORD(wp)==BN_CLICKED) {
  unsigned id=LOWORD(wp);std::string a=agent(*u);auto& store=WorkspaceAgentStore();
  if(id==21)refresh(*u);
  else if(id==22) {store.revoke(a);grants.erase(a);status(*u,"Agent access revoked.");}
  else if(id==23) {store.revokeAll();grants.clear();status(*u,"All agent access revoked.");}
  else if(id==20 && u->kind==59002) {
   int count=(int)SendMessage(u->list,LB_GETSELCOUNT,0,0);if(count<=0)throw std::runtime_error("Select at least one output window.");
   std::vector<int> selected(count);SendMessage(u->list,LB_GETSELITEMS,count,(LPARAM)selected.data());GrantUi g;g.drafts=SendDlgItemMessage(w,24,BM_GETCHECK,0,0)==BST_CHECKED;
   for(int i=0;i<count;++i)g.windows.push_back(u->ids.at(selected[i]));
   grants.erase(a);g.session=store.enable(a,g.windows,g.drafts);
   g.bridge=std::make_shared<agent_workspace::BridgeHost>();
   try {g.bridge->start(store,a,g.session);}catch(...) {store.revoke(a);throw;}
   grants[a]=g;
   wchar_t path[MAX_PATH];GetModuleFileNameW(NULL,path,MAX_PATH);std::wstring folder(path);folder=folder.substr(0,folder.find_last_of(L"\\/"));
   std::wstring launch=L"\""+folder+L"\\MUSHclientAgent.exe\" --endpoint \""+g.bridge->endpoint()+L"\" mcp";
   SetWindowTextW(u->input,launch.c_str());
   status(*u,"Access enabled for selected output only. Copy the launch command above into your external agent configuration. No game-send tools are exposed.");
  } else if(id==20 && u->kind==59003) {store.postHuman(a,text(u->input));SetWindowTextW(u->input,L"");refresh(*u);}
  else if(id==20) {
   int selected=(int)SendMessage(u->list,LB_GETCURSEL,0,0);if(selected<0)throw std::runtime_error("Select a draft to review.");
   const auto& drafts=store.drafts();for(size_t i=0;i<drafts.size();++i)if(drafts[i].id==u->ids.at(selected))WorkspaceOpenDraft(w,drafts[i]);
  }
  return 0;
 }
 if(msg==WM_CLOSE) {DestroyWindow(w);return 0;}
 if(msg==WM_NCDESTROY) {KillTimer(w,1);SetWindowLongPtrW(w,GWLP_USERDATA,0);delete u;}
 }catch(const std::exception& e) {if(u && IsWindow(u->status))status(*u,e.what());return 0;}
 return DefWindowProcW(w,msg,wp,lp);
}
}
void OpenWorkspaceTool(CMainFrame& frame,UINT command) {
 PennEditorSetTransport(WorkspaceSendEditorRead);
 PennEditorSetSendPrepared(WorkspaceSendPreparedEditor);
 if(command==59007){HWND pane=OpenAgentReviewPane(frame.GetSafeHwnd());if(pane)frame.HostWorkspaceTool(pane,"Agent review and captures");return;}
 if(command==59004){OpenWorkspaceSceneDirector(frame);return;}
 if(command==59001) {
  CMDIChildWnd* child=frame.MDIGetActive();CMUSHclientDoc* d=child?DYNAMIC_DOWNCAST(CMUSHclientDoc,child->GetActiveDocument()):NULL;
  OpenPennEditor(frame.GetSafeHwnd(),d?std::string((LPCTSTR)WorkspaceConnectionId(d)):"local:unsent");return;
 }
 WNDCLASSW cls={0};cls.hInstance=AfxGetInstanceHandle();cls.lpfnWndProc=proc;cls.lpszClassName=L"MUSHclientWorkspaceTools";cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&cls);
 Ui* u=new Ui();u->kind=command;
 HWND w=CreateWindowExW(0,cls.lpszClassName,command==59002?L"Agent access":command==59003?L"Agent conversations":L"Scene director - reviewed drafts",WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,CW_USEDEFAULT,790,630,frame.GetSafeHwnd(),NULL,cls.hInstance,u);
 if(w) {
  if(command==59003)frame.HostWorkspaceTool(w,"Agent conversations");
  ShowWindow(w,SW_SHOW);
 }else delete u;
}
void ShutdownWorkspaceTools() {WorkspaceAgentStore().revokeAll();grants.clear();}

BOOL WorkspaceTranslateMessage(MSG* message)
{
 if(message->message<WM_KEYFIRST || message->message>WM_KEYLAST)return FALSE;
 HWND owner=NULL;CWorkspacePane* pane=NULL;
 for(HWND cursor=message->hwnd;cursor;cursor=::GetParent(cursor)) {
  wchar_t name[80]={0};::GetClassNameW(cursor,name,80);
  if(::GetPropW(cursor,L"MUSHclientWorkspace") || !wcscmp(name,L"MUSHclientWorkspaceTools") || !wcscmp(name,L"MUSHclientSceneDirector") || !wcscmp(name,L"MUSHclientPennIDE") || !wcscmp(name,L"MUSHclientAgentReview")) {owner=cursor;break;}
  CWnd* native=CWnd::FromHandlePermanent(cursor);
  if(native && native->IsKindOf(RUNTIME_CLASS(CWorkspacePane))){pane=static_cast<CWorkspacePane*>(native);owner=cursor;break;}
 }
 if(!owner)return FALSE;
 const bool control=(GetKeyState(VK_CONTROL)&0x8000)!=0;
 if(message->message==WM_KEYDOWN && control && (GetKeyState(VK_SHIFT)&0x8000) && !(GetKeyState(VK_MENU)&0x8000) && message->wParam>=VK_F1 && message->wParam<=VK_F9 && App.m_pMainWnd)
  {if(Frame.FocusWorkspaceTranscript((size_t)(message->wParam-VK_F1)))return TRUE;}
 if(pane && !pane->IsToolHost() && pane->PreTranslateMessage(message))return TRUE;
 wchar_t focusedClass[80]={0};::GetClassNameW(message->hwnd,focusedClass,80);
 if(message->message==WM_KEYDOWN && control && message->wParam=='A' && (!_wcsicmp(focusedClass,L"EDIT") || !_wcsicmp(focusedClass,L"RICHEDIT50W")))
  {::SendMessageW(message->hwnd,EM_SETSEL,0,-1);return TRUE;}
 if(message->message==WM_KEYDOWN && message->wParam==VK_TAB && _wcsicmp(focusedClass,L"Scintilla") && ::IsDialogMessageW(owner,message))return TRUE;
 // Deliver native control/subclass shortcuts before the legacy game accelerators.
 ::TranslateMessage(message);::DispatchMessageW(message);return TRUE;
}
