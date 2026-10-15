#include "stdafx.h"
#include "workspace/WorkspaceComposerSplitter.h"
#include "workspace/WorkspaceViewLease.h"
#include <fstream>

class ComposerFrame:public CFrameWnd {
public:
 CWorkspaceComposerSplitter splitter;
 CEditView* input=NULL;int sent=0;
 BOOL OnCreateClient(LPCREATESTRUCT,CCreateContext*)override{
  CCreateContext context={};
  if(!splitter.CreateStatic(this,2,1)||!splitter.CreateView(0,0,RUNTIME_CLASS(CEditView),CSize(400,400),&context)||!splitter.CreateView(1,0,RUNTIME_CLASS(CEditView),CSize(400,100),&context))return FALSE;
  input=static_cast<CEditView*>(splitter.GetPane(1,0));
  return splitter.AttachComposer(1,input->m_hWnd,[this](){++sent;});
 }
};
class ComposerApp:public CWinApp{
 int result_=1;
 BOOL InitInstance()override{
#ifdef _DEBUG
  _CrtSetReportHook([](int type,char* message,int*)->int{if(type==_CRT_ASSERT){FILE* f=NULL;fopen_s(&f,"composer-debug-assert.txt","w");if(f){fputs(message,f);fclose(f);}ExitProcess(91);}return FALSE;});
#endif
  AfxOleInit();std::ofstream out("composer-results.txt");out.setf(std::ios::unitbuf);bool ok=true;
  auto check=[&](const char* label,bool value){out<<(value?"PASS ":"FAIL ")<<label<<"\n";ok=value&&ok;};
  auto* a=new ComposerFrame;auto* b=new ComposerFrame;
  check("two original native splitter frames create",a->Create(NULL,"Fictional Alpha",WS_OVERLAPPEDWINDOW,CRect(80,80,1000,800))&&b->Create(NULL,"Fictional Beta",WS_OVERLAPPEDWINDOW,CRect(100,100,1020,820)));
  const bool hidden=strstr(m_lpCmdLine,"--hidden")!=NULL;
  m_pMainWnd=a;if(!hidden){a->ShowWindow(SW_SHOWNOACTIVATE);b->ShowWindow(SW_SHOWNOACTIVATE);}a->RecalcLayout();b->RecalcLayout();
  HWND original=a->input->m_hWnd;HWND other=b->input->m_hWnd;
  const HFONT custom=CreateFontW(-23,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,0,0,0,FIXED_PITCH,L"Consolas");
  ::SendMessage(original,WM_SETFONT,(WPARAM)custom,0);::SetWindowText(original,"Alpha retained draft");::SendMessage(original,EM_SETSEL,2,8);
  ::SetWindowText(other,"Beta independent draft");::SendMessage(other,EM_SETSEL,1,4);
  const LRESULT selection=::SendMessage(original,EM_GETSEL,0,0),otherSelection=::SendMessage(other,EM_GETSEL,0,0);
  a->splitter.SetComposerDestination(L"Harbor",L"Alpha",true);b->splitter.SetComposerDestination(L"Harbor",L"Beta",true);
  check("original HWND remains direct GetPane and original pane ID",a->splitter.GetPane(1,0)->m_hWnd==original && ::GetDlgCtrlID(original)==AFX_IDW_PANE_FIRST+16 && ::GetParent(original)==a->splitter.m_hWnd);
  ::SendMessage(::GetDlgItem(a->splitter.m_hWnd,0x7b20),BM_CLICK,0,0);
  check("Send callback stays with owning character while another frame is active",a->sent==1 && b->sent==0);
  a->splitter.SetComposerDestination(L"Harbor",L"Alpha",false);::SendMessage(::GetDlgItem(a->splitter.m_hWnd,0x7b20),BM_CLICK,0,0);
  check("offline Send does not dispatch",a->sent==1);a->splitter.SetComposerDestination(L"Harbor",L"Alpha",true);
  bool sizes=true;
  for(int inputHeight:{24,48,120,240,64}){
   CRect client;a->splitter.GetClientRect(client);a->splitter.SetRowInfo(0,client.Height()-11-a->splitter.RowHeightForInput(inputHeight),10);a->splitter.SetRowInfo(1,a->splitter.RowHeightForInput(inputHeight),0);a->splitter.RecalcLayout();
   int total,minimum;a->splitter.GetRowInfo(1,total,minimum);CRect edit;a->input->GetWindowRect(edit);
   out<<"HEIGHT "<<inputHeight<<" row="<<total<<" edit="<<edit.Height()<<" overhead="<<a->splitter.ComposerOverhead()<<"\n";
   sizes=sizes && a->splitter.InputHeightFromRow(total)==inputHeight && edit.Height()==inputHeight;
  }
  check("automatic and manual height round trips exclude chrome without accumulating growth",sizes);
  check("zero requested input height does not add a visible strip",a->splitter.RowHeightForInput(0)==0);
  check("early construction uses fallback without caching uninitialized font geometry",a->splitter.RestoreInputHeight(39,false)==39);
  check("first ready frame height initializes from original body units after font measurement",a->splitter.RestoreInputHeight(41)==41);
  a->splitter.RememberInputHeight(120);
  a->SetWindowPos(NULL,0,0,800,550,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);a->RecalcLayout();
  check("native resize retains requested body height instead of reloading older preference",a->splitter.RestoreInputHeight(41)==120);
  a->splitter.RememberInputHeight(64);
  check("manual or automatic new height supersedes prior request without persistent writes",a->splitter.RestoreInputHeight(41)==64);
  bool retained=true,inside=true;
  for(auto theme:{workspace_style::Theme::Light,workspace_style::Theme::Dark,workspace_style::Theme::System}){
   workspace_style::SetTheme(theme);
   for(int width:{600,1400,920}){
    a->SetWindowPos(NULL,0,0,width,800,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);a->RecalcLayout();
    char text[100]={};::GetWindowText(original,text,100);char text2[100]={};::GetWindowText(other,text2,100);
    retained=retained && a->splitter.GetPane(1,0)->m_hWnd==original && strcmp(text,"Alpha retained draft")==0 && strcmp(text2,"Beta independent draft")==0 && ::SendMessage(original,WM_GETFONT,0,0)==(LRESULT)custom && ::SendMessage(original,EM_GETSEL,0,0)==selection && ::SendMessage(other,EM_GETSEL,0,0)==otherSelection;
    RECT editor,send;::GetWindowRect(original,&editor);::GetWindowRect(::GetDlgItem(a->splitter.m_hWnd,0x7b20),&send);inside=inside && send.bottom<=editor.top;
   }
  }
  check("theme resize and independent characters retain drafts selections custom font and original HWND",retained);
  check("native destination and Send remain above original input at narrow and expanded widths",inside);
  CWnd channelHome;check("hidden channel owner creates",channelHome.CreateEx(0,AfxRegisterWndClass(0),"Fictional channel owner",WS_CHILD,0,0,200,200,a->m_hWnd,NULL)!=FALSE);
  HWND channel=::CreateWindowExW(0,L"EDIT",L"Unicode \x03bb \xd83d\xdc09 draft",WS_CHILD|ES_AUTOHSCROLL,0,0,1,1,channelHome.m_hWnd,(HMENU)2,AfxGetInstanceHandle(),NULL);
  ::SendMessageW(channel,EM_SETSEL,2,5);const LRESULT channelSelection=::SendMessageW(channel,EM_GETSEL,0,0);
  int channelSent=0,channelKeys=0;
  const auto submitChannel=[&](){++channelSent;};
  const auto keys=[&](MSG* message)->BOOL{if(message->wParam==VK_UP){++channelKeys;return TRUE;}return FALSE;};
  check("channel Unicode editor borrows shared Main slot",a->splitter.UseChannelEditor(channel,channelHome.m_hWnd,L"Input target: Alpha / Public",L"+pub ",submitChannel,keys) && ::GetParent(channel)==a->splitter.m_hWnd && a->splitter.CurrentInput()==channel);
  wchar_t channelText[100]={},prefix[100]={};::GetWindowTextW(channel,channelText,100);::GetWindowTextW(::GetDlgItem(a->splitter.m_hWnd,0x7b22),prefix,100);
  check("borrow retains Unicode draft selection and exact trailing-space prefix",::IsWindowUnicode(channel) && wcscmp(channelText,L"Unicode \x03bb \xd83d\xdc09 draft")==0 && ::SendMessageW(channel,EM_GETSEL,0,0)==channelSelection && wcscmp(prefix,L"Prefix: \x201c+pub \x201d")==0);
  const int mainBefore=a->sent;::SendMessage(::GetDlgItem(a->splitter.m_hWnd,0x7b20),BM_CLICK,0,0);
  check("shared Send dispatches channel only, never Main or another character",channelSent==1 && a->sent==mainBefore && b->sent==0);
  MSG key={};key.hwnd=channel;key.message=WM_SYSKEYDOWN;key.wParam=VK_UP;
  check("shared input dispatches Alt history keys to owning channel",workspace_composer::PreTranslateInput(&key) && channelKeys==1);
  a->splitter.RouteEditCommand(ID_EDIT_SELECT_ALL,CN_COMMAND,NULL,NULL);
  DWORD editFirst=0,editLast=0;::SendMessageW(channel,EM_GETSEL,(WPARAM)&editFirst,(LPARAM)&editLast);
  check("Edit Select All targets shared Unicode input instead of hidden Main",editFirst==0 && editLast==(DWORD)::GetWindowTextLengthW(channel) && ::SendMessage(original,EM_GETSEL,0,0)==selection);
  a->splitter.RouteEditCommand(ID_EDIT_CLEAR,CN_COMMAND,NULL,NULL);
  char untouchedMain[100]={};::GetWindowText(original,untouchedMain,100);
  check("Edit Delete changes only selected channel draft",::GetWindowTextLengthW(channel)==0 && strcmp(untouchedMain,"Alpha retained draft")==0);
  ::SetWindowTextW(channel,L"independent channel draft");
  HWND search=::CreateWindowExW(0,L"EDIT",L"search query",WS_CHILD,0,0,30,20,channelHome.m_hWnd,NULL,AfxGetInstanceHandle(),NULL);
  a->splitter.RouteEditCommand(ID_EDIT_SELECT_ALL,CN_COMMAND,NULL,NULL,search);
  a->splitter.RouteEditCommand(ID_EDIT_CLEAR,CN_COMMAND,NULL,NULL,search);
  check("contextual Edit command preserves shared draft while editing search",!::GetWindowTextLengthW(search) && ::GetWindowTextLengthW(channel)==25 && strcmp(untouchedMain,"Alpha retained draft")==0);
  const int historyBefore=channelKeys;
  check("legacy history accelerator routes to channel",a->splitter.RouteEditCommand(ID_ALT_UPARROW,CN_COMMAND,NULL,NULL) && channelKeys==historyBefore+1);
  check("unsupported Main completion accelerator is consumed",a->splitter.RouteEditCommand(ID_KEYS_TAB,CN_COMMAND,NULL,NULL));
  ::SetWindowTextW(channel,L"retained channel draft");
  CRect collapseBounds;a->splitter.GetClientRect(collapseBounds);
  a->splitter.SetRowInfo(0,collapseBounds.Height(),0);a->splitter.SetRowInfo(1,0,0);a->splitter.RecalcLayout();
  check("collapsed input cannot leave borrowed editor over transcript",!(::GetWindowLong(channel,GWL_STYLE)&WS_VISIBLE));
  a->splitter.SetRowInfo(0,collapseBounds.Height()-11-a->splitter.RowHeightForInput(64),0);a->splitter.SetRowInfo(1,a->splitter.RowHeightForInput(64),0);a->splitter.RecalcLayout();
  check("expanded input restores channel editor",(::GetWindowLong(channel,GWL_STYLE)&WS_VISIBLE)!=0);
  workspace_composer::ReleaseEditor(channel);
  char retainedMain[100]={};::GetWindowText(original,retainedMain,100);
  check("Main restoration returns borrowed editor home and retains original draft selection and font",::GetParent(channel)==channelHome.m_hWnd && a->splitter.CurrentInput()==original && strcmp(retainedMain,"Alpha retained draft")==0 && ::SendMessage(original,EM_GETSEL,0,0)==selection && ::SendMessage(original,WM_GETFONT,0,0)==(LRESULT)custom);
  check("released channel keys cannot address stale destination",!workspace_composer::PreTranslateInput(&key));
  WorkspaceViewLease live(a->input);check("lease accepts current original view",live.IsLive());
  // Two views can share a document; submission ownership is wider than an HWND.
  const CDocument* fictionalOwner=reinterpret_cast<const CDocument*>(a);
  {WorkspaceMainSubmission first(fictionalOwner);WorkspaceMainSubmission nested(fictionalOwner);
   WorkspaceMainSubmission otherOwner(reinterpret_cast<const CDocument*>(b));
   check("document-wide nested submission rejects second view but permits different owner",first.Acquired()&&!nested.Acquired()&&otherOwner.Acquired());}
  check("submission scope releases without dereferencing a possibly closed document",!WorkspaceMainSubmission::IsActive(fictionalOwner));
  ::SetPropW(original,L"Valyria.OriginalViewLifetime",(HANDLE)0x12345678);check("lease rejects changed native lifetime",!live.IsLive());
  WorkspaceViewLease closed(b->input);
  check("second host can borrow same editor after explicit release",b->splitter.UseChannelEditor(channel,channelHome.m_hWnd,L"Input target: Beta / Test",L"page Mira=",submitChannel,keys));
  b->DestroyWindow();check("lease rejects closed original view without dereferencing it",!closed.IsLive());
  check("closing host returns live channel editor before destroying original views",::IsWindow(channel) && ::GetParent(channel)==channelHome.m_hWnd && !workspace_composer::BorrowedEditors().count(channel));
  a->DestroyWindow();DeleteObject(custom);out<<(ok?"PASS":"FAIL")<<" composer geometry and original-view lifetime; native API fixture, no transport or physical IME claim\n";result_=ok?0:1;return FALSE;
 }
 int ExitInstance()override{CWinApp::ExitInstance();return result_;}
} app;
