// This fixture creates native windows. Build-only while user desktop is in use.
#include "stdafx.h"
#include "workspace/WorkspaceNativeControl.h"
#include "WorkspaceStyle.h"
#include <fstream>

class NativeControlProbe:public CWinApp {
 int result_=1;
 BOOL InitInstance()override{
#ifdef _DEBUG
  _CrtSetReportHook([](int type,char* message,int*)->int{if(type==_CRT_ASSERT){FILE* f=NULL;fopen_s(&f,"native-control-assert.txt","w");if(f){fputs(message,f);fclose(f);}ExitProcess(91);}return FALSE;});
#endif
  AfxOleInit();LoadLibraryW(L"Msftedit.dll");std::ofstream out("native-control-results.txt");out.setf(std::ios::unitbuf);bool ok=true;
  auto check=[&](const char* label,bool value){out<<(value?"PASS ":"FAIL ")<<label<<"\n";ok=value&&ok;};
  CWnd parent;check("native test parent creates",parent.CreateEx(0,AfxRegisterWndClass(0),"Fictional control lifetime",WS_POPUP,0,0,600,400,NULL,NULL)!=FALSE);
  CEdit input,search;CRichEditCtrl output;
  auto create=[&](CWnd& wrapper,LPCWSTR type,int id){HWND window=::CreateWindowExW(0,type,L"",WS_CHILD|ES_MULTILINE,0,0,100,100,parent.m_hWnd,(HMENU)(UINT_PTR)id,AfxGetInstanceHandle(),NULL);return window&&workspace_native_control::Attach(wrapper,window);};
  check("native Unicode Edit and RichEdit attach with lifetime observer",create(input,L"EDIT",1)&&create(search,L"EDIT",2)&&create(output,L"RICHEDIT50W",3));
  workspace_style::StyleControl(input.m_hWnd);workspace_style::StyleControl(search.m_hWnd);
  const wchar_t* sample=L"Lambda \x03bb dragon \xd83d\xdc09";bool unicode=true;
  for(CWnd* control:{static_cast<CWnd*>(&input),static_cast<CWnd*>(&search),static_cast<CWnd*>(&output)}){
   ::SetWindowTextW(control->m_hWnd,sample);wchar_t text[100]={};::GetWindowTextW(control->m_hWnd,text,100);
   unicode=unicode&&::IsWindowUnicode(control->m_hWnd)&&wcscmp(text,sample)==0&&CWnd::FromHandlePermanent(control->m_hWnd)==control;
  }
  check("Unicode supplementary text and exact permanent wrappers survive shared styling",unicode);
  HWND oldOutput=output.m_hWnd;check("exact channel close DestroyWindow completes without assertion",output.DestroyWindow()!=FALSE);
  check("explicit destroy clears original HWND and permanent map",output.GetSafeHwnd()==NULL&&!::IsWindow(oldOutput)&&CWnd::FromHandlePermanent(oldOutput)==NULL);
  bool repeated=true;for(int i=0;i<64;++i){repeated=repeated&&create(output,L"RICHEDIT50W",3);HWND old=output.GetSafeHwnd();repeated=repeated&&output.DestroyWindow()&&output.GetSafeHwnd()==NULL&&CWnd::FromHandlePermanent(old)==NULL;}
  check("repeated channel close and reopen never retain stale map entries",repeated);
  check("RichEdit reopens beside still-live input and search",create(output,L"RICHEDIT50W",3));
  const HWND oldInput=input.m_hWnd,oldSearch=search.m_hWnd,oldReopened=output.m_hWnd;parent.DestroyWindow();
  check("parent destruction detaches all three original wrappers",!input.GetSafeHwnd()&&!search.GetSafeHwnd()&&!output.GetSafeHwnd());
  check("parent destruction clears every native permanent-map identity",!CWnd::FromHandlePermanent(oldInput)&&!CWnd::FromHandlePermanent(oldSearch)&&!CWnd::FromHandlePermanent(oldReopened));
  out<<(ok?"PASS":"FAIL")<<" native control lifetime; native API fixture, no physical UI acceptance\n";result_=ok?0:1;return FALSE;
 }
 int ExitInstance()override{CWinApp::ExitInstance();return result_;}
} app;
