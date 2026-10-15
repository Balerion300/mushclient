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
  m_pMainWnd=a;a->ShowWindow(SW_SHOWNOACTIVATE);b->ShowWindow(SW_SHOWNOACTIVATE);a->RecalcLayout();b->RecalcLayout();
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
  WorkspaceViewLease live(a->input);check("lease accepts current original view",live.IsLive());
  // Two views can share a document; submission ownership is wider than an HWND.
  const CDocument* fictionalOwner=reinterpret_cast<const CDocument*>(a);
  {WorkspaceMainSubmission first(fictionalOwner);WorkspaceMainSubmission nested(fictionalOwner);
   WorkspaceMainSubmission otherOwner(reinterpret_cast<const CDocument*>(b));
   check("document-wide nested submission rejects second view but permits different owner",first.Acquired()&&!nested.Acquired()&&otherOwner.Acquired());}
  check("submission scope releases without dereferencing a possibly closed document",!WorkspaceMainSubmission::IsActive(fictionalOwner));
  ::SetPropW(original,L"Valyria.OriginalViewLifetime",(HANDLE)0x12345678);check("lease rejects changed native lifetime",!live.IsLive());
  WorkspaceViewLease closed(b->input);b->DestroyWindow();check("lease rejects closed original view without dereferencing it",!closed.IsLive());
  a->DestroyWindow();DeleteObject(custom);out<<(ok?"PASS":"FAIL")<<" composer geometry and original-view lifetime; native API fixture, no transport or physical IME claim\n";result_=ok?0:1;return FALSE;
 }
 int ExitInstance()override{CWinApp::ExitInstance();return result_;}
} app;
