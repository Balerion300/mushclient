// Production forms in fictional own windows. Only world/frame IO is adapted;
// no installed client, profiles, provider, game socket or desktop automation.
#include "stdafx.h"
#include "workspace/WorkflowUi.h"
#include "workspace/WorkspaceSpawnPolicy.h"
#include "workspace/WorkspaceInputPolicy.h"
#include "workspace/PennEditorWindow.h"
#include "workspace/AgentReviewPane.h"
#include "AgentWorkspaceCore.h"
#include <fstream>
#undef App
#undef Frame
struct CMUSHclientDoc {CString m_mush_name="Fictional Moon";} fictionalDocument;
struct FictionalDocs {
  POSITION GetFirstDocPosition(){return (POSITION)1;}
  CMUSHclientDoc* GetNextDoc(POSITION& position){position=NULL;return &fictionalDocument;}
} fictionalDocs;
struct {FictionalDocs* m_pWorldDocTemplate=&fictionalDocs;} App;
CString WorkspaceConnectionId(CMUSHclientDoc*){return "fictional-world:runtime:1";}
bool WorkspaceIsConnectionLive(const CString&){return true;}
std::string WorkspaceEndpointForWindow(const std::string& id){return id;}
struct WorkspaceRoute {
  CString connectionId,transcriptName;
  std::wstring pattern;
  bool regex=false,omitFromMain=false,followingImages=false;
  bool Compile(CString&){return !pattern.empty();}
};
class CMainFrame:public CFrameWnd {
  std::vector<WorkspaceRoute> routes;
public:
  void PostNcDestroy() override {}
  const std::vector<WorkspaceRoute>& WorkspaceRoutes(){return routes;}
  workspace_spawn::Preferences WorkspaceTranscriptPreferences(const CString&,const CString&){return {};}
  bool SetWorkspaceTranscriptPreferences(const CString&,const CString&,const workspace_spawn::Preferences&,CString&){return true;}
  std::wstring WorkspaceInputPrefix(const CString&,const CString&){return L"";}
  bool SetWorkspaceInputPrefix(const CString&,const CString&,const std::wstring&){return true;}
  bool AddWorkspaceRoute(const WorkspaceRoute& route,CString&){routes.push_back(route);return true;}
  bool ReplaceWorkspaceRoute(const WorkspaceRoute&,const WorkspaceRoute& route,CString& error){return AddWorkspaceRoute(route,error);}
  bool RemoveWorkspaceRoute(const WorkspaceRoute&){return true;}
};
agent_workspace::Store store;
agent_workspace::Store& WorkspaceAgentStore(){return store;}
HWND WorkspaceOpenDraft(HWND,const agent_workspace::Draft&){return NULL;}
// Source adapters omit app-only includes; form/window procedures are unchanged.
#include "route-form-production.inc"
#include "scene-form-production.inc"

namespace {
std::ofstream receipt;
unsigned checks=0,failures=0;
void Check(bool ok,const char* label){++checks;if(!ok){++failures;receipt<<"FAIL "<<label<<"\n";receipt.flush();}}
RECT Relative(HWND parent,HWND child){RECT r={};GetWindowRect(child,&r);MapWindowPoints(NULL,parent,(POINT*)&r,2);return r;}
bool Inside(HWND parent,HWND child){RECT r=Relative(parent,child),client;GetClientRect(parent,&client);return r.left>=0 && r.top>=0 && r.right<=client.right && r.bottom<=client.bottom;}
void Pump(){MSG m;while(PeekMessage(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}}
void Snapshot(HWND window,const std::wstring& path){
  RECT r;GetWindowRect(window,&r);int width=r.right-r.left,height=r.bottom-r.top;
  HDC dc=GetDC(window),memory=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,width,height);auto old=SelectObject(memory,bitmap);PrintWindow(window,memory,0);SelectObject(memory,old);
  BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  std::vector<unsigned char> pixels(width*height*4);GetDIBits(memory,bitmap,0,height,pixels.data(),&info,DIB_RGB_COLORS);BITMAPFILEHEADER header={};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+(DWORD)pixels.size();
  std::ofstream output(path,std::ios::binary);output.write((char*)&header,sizeof(header));output.write((char*)&info.bmiHeader,sizeof(info.bmiHeader));output.write((char*)pixels.data(),pixels.size());DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(window,dc);
}
void CaptionFits(HWND button){wchar_t text[256]={};GetWindowTextW(button,text,256);std::wstring label=text;label.erase(std::remove(label.begin(),label.end(),L'&'),label.end());HDC dc=GetDC(button);auto font=(HFONT)SendMessageW(button,WM_GETFONT,0,0);auto old=SelectObject(dc,font);SIZE size={};GetTextExtentPoint32W(dc,label.data(),(int)label.size(),&size);RECT r;GetClientRect(button,&r);Check(size.cx+workspace_style::Scale(button,44)<=r.right,"icon/button caption fits");SelectObject(dc,old);ReleaseDC(button,dc);}
}
class FormsApp:public CWinApp {
public:
  BOOL InitInstance() override {
    AfxOleInit();INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_WIN95_CLASSES};InitCommonControlsEx(&controls);
    wchar_t exe[32768];GetModuleFileNameW(NULL,exe,32768);std::wstring folder=exe;folder.resize(folder.find_last_of(L"\\/")+1);receipt.open(folder+L"workflow-forms-results.txt");
    auto* frame=new CMainFrame;m_pMainWnd=frame;Check(frame->Create(NULL,"Fictional workflow forms",WS_OVERLAPPEDWINDOW,CRect(20,20,1000,760))!=FALSE,"frame");
    auto* route=new CWorkspaceRouteDialog(frame,"fictional-world:runtime:1");Check(route->Open()!=FALSE,"route form");route->ShowWindow(SW_SHOWNOACTIVATE);HWND routeWindow=route->GetSafeHwnd();
    route->SetWindowPos(NULL,20,20,400,420,SWP_NOZORDER);Pump();
    for(int id:{11,12,8})CaptionFits(GetDlgItem(routeWindow,id));
    Check(!IsWindowVisible(GetDlgItem(routeWindow,3)),"route advanced options initially collapsed");
    SendDlgItemMessage(routeWindow,15,BM_CLICK,0,0);Check(IsWindowVisible(GetDlgItem(routeWindow,3))!=FALSE,"route advanced options explicit expansion");
    SetActiveWindow(routeWindow);SetFocus(GetDlgItem(routeWindow,11));bool saveReached=false,advancedReached=false;
    for(int i=0;i<36;++i){MSG key={};key.hwnd=GetFocus();key.message=WM_KEYDOWN;key.wParam=VK_TAB;route->PreTranslateMessage(&key);Pump();HWND focused=GetFocus();Check(Inside(routeWindow,focused),"route keyboard focus stays inside scrolled viewport");saveReached=saveReached||focused==GetDlgItem(routeWindow,5);advancedReached=advancedReached||focused==GetDlgItem(routeWindow,14);}
    Check(saveReached && advancedReached,"route Tab reaches advanced checkbox and Save");Snapshot(routeWindow,folder+L"channels-narrow.bmp");
    workspace_style::SetTheme(workspace_style::Theme::Dark);Pump();Snapshot(routeWindow,folder+L"channels-dark.bmp");workspace_style::SetTheme(workspace_style::Theme::Light);
    route->DestroyWindow();

    HWND sceneWindow=OpenWorkspaceSceneDirector(*frame);Check(sceneWindow!=NULL,"scene form");
    SetWindowPos(sceneWindow,NULL,20,20,440,420,SWP_NOZORDER);Pump();
    RECT constrained;GetWindowRect(sceneWindow,&constrained);Check(constrained.right-constrained.left<=440 && constrained.bottom-constrained.top<=420,"scene minimum permits constrained scroll viewport");
    auto* director=(Director*)GetWindowLongPtrW(sceneWindow,GWLP_USERDATA);page(*director,1);RECT client;GetClientRect(sceneWindow,&client);layout(*director,client.right,client.bottom);
    Check(director->virtualHeight>=workspace_style::Scale(sceneWindow,660),"scene constrained viewport keeps natural form height");
    RECT lastField=Relative(sceneWindow,GetDlgItem(sceneWindow,318)),save=Relative(sceneWindow,GetDlgItem(sceneWindow,320));Check(lastField.bottom<=save.top,"scene ninth NPC field does not overlap Save");
    Check(!IsWindowEnabled(GetDlgItem(sceneWindow,320)),"scene assignment disabled without scene/shared destination");
    director->scrollY=director->virtualHeight;director->scrollX=director->virtualWidth;layout(*director,client.right,client.bottom);RECT action=Relative(sceneWindow,GetDlgItem(sceneWindow,323));const int actionCenter=(action.left+action.right)/2;Check(action.top>=0 && action.bottom<=client.bottom && actionCenter>=0 && actionCenter<client.right,"scene bottom action/caption reachable by viewport scroll");Snapshot(sceneWindow,folder+L"scene-constrained.bmp");DestroyWindow(sceneWindow);

    Check(PennEditorConfigureStorage(folder+L"fictional-state"),"editor storage isolated");
    HWND editor=OpenPennEditor(frame->GetSafeHwnd(),"fictional-world:runtime:1","&TEST #1=fictional","Fictional Moon");Check(editor!=NULL,"editor form");
    SetWindowPos(editor,NULL,20,20,520,580,SWP_NOZORDER);Pump();Snapshot(editor,folder+L"editor-narrow.bmp");
    SetActiveWindow(editor);SetFocus(GetDlgItem(editor,101));MSG enter={};enter.hwnd=GetFocus();enter.message=WM_KEYDOWN;enter.wParam=VK_RETURN;
    const auto before=PennEditorListSnapshots().size();Check(workflow_ui::Keyboard(editor,&enter),"focused native editor button accepts Enter");Check(PennEditorListSnapshots().size()==before+1,"button Enter invokes New exactly once");
    enter.message=WM_KEYUP;Check(!workflow_ui::Keyboard(editor,&enter),"button key-up does not invoke twice");
    HWND objectLabel=NULL;for(HWND c=GetWindow(editor,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){wchar_t text[64]={};GetWindowTextW(c,text,64);if(!wcscmp(text,L"&Object"))objectLabel=c;}
    Check(objectLabel!=NULL,"persistent object field label");MSG mnemonic={};mnemonic.hwnd=GetFocus();mnemonic.message=WM_SYSCHAR;mnemonic.wParam='o';Check(workflow_ui::Keyboard(editor,&mnemonic),"editor Object mnemonic handled");wchar_t kind[64]={};GetClassNameW(GetFocus(),kind,64);Check(!_wcsicmp(kind,L"EDIT"),"editor mnemonic focuses target input");
    enter.hwnd=GetFocus();enter.message=WM_KEYDOWN;Check(!workflow_ui::Keyboard(editor,&enter),"ordinary field Enter not consumed by button handler");
    store.addWindow("fictional-world:runtime:1","Fictional Moon","Main");
    HWND review=OpenAgentReviewPane(frame->GetSafeHwnd());Check(review!=NULL,"agent review form");
    SetWindowPos(review,NULL,20,20,620,600,SWP_NOZORDER);Pump();HWND list=GetDlgItem(review,203);SendMessage(list,LB_SETCURSEL,0,0);SendMessage(review,WM_COMMAND,MAKEWPARAM(203,LBN_SELCHANGE),(LPARAM)list);
    Check(!IsWindowEnabled(GetDlgItem(review,208)),"selected editor sharing disabled while agent access is off");
    store.enable("Assistant",{"fictional-world:runtime:1"},true);SendMessage(review,WM_TIMER,1,0);SendMessage(list,WM_KEYDOWN,VK_DOWN,0);SendMessage(review,WM_COMMAND,MAKEWPARAM(203,LBN_SELCHANGE),(LPARAM)list);
    Check(IsWindowEnabled(GetDlgItem(review,208))!=FALSE,"keyboard-selected editor enables sharing with explicit grant");
    wchar_t accessStatus[1024]={};GetDlgItemTextW(review,206,accessStatus,1024);Check(std::wstring(accessStatus).find(L"disabled")==std::wstring::npos,"grant transition clears disabled status");
    SendMessage(list,LB_SETCURSEL,0,0);SendMessage(review,WM_COMMAND,MAKEWPARAM(203,LBN_SELCHANGE),(LPARAM)list);
    SendDlgItemMessage(review,204,EM_SETSEL,1,4);SendMessage(review,WM_TIMER,1,0);DWORD start=0,end=0;SendDlgItemMessage(review,204,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);Check(start==1 && end==4,"review refresh preserves preview selection");
    CaptionFits(GetDlgItem(review,208));Snapshot(review,folder+L"agent-review-narrow.bmp");workspace_style::SetTheme(workspace_style::Theme::Dark);Pump();Snapshot(review,folder+L"agent-review-dark.bmp");Snapshot(editor,folder+L"editor-dark.bmp");workspace_style::SetTheme(workspace_style::Theme::Light);DestroyWindow(review);DestroyWindow(editor);
    frame->DestroyWindow();delete frame;
    receipt<<"checks="<<checks<<" failures="<<failures<<"\n";receipt.close();PostQuitMessage(failures?1:0);return FALSE;
  }
} application;
