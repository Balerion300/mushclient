#include "stdafx.h"
#include "ModernFixture.h"
#include "WorkspaceStyle.h"
#include "WorkspaceAppearance.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxmdichildwndex.h>
#pragma pop_macro("Translate")
#include <fstream>
#include <string>
#include <vector>

bool Check(std::ofstream& out,const char* label,bool result){out<<(result?"PASS ":"FAIL ")<<label<<"\n";return result;}
void Pump(){MSG message;int count=0;while(count++<256 && PeekMessage(&message,NULL,0,0,PM_REMOVE)){if(message.message==WM_QUIT)continue;if(!AfxGetApp()->PreTranslateMessage(&message)){TranslateMessage(&message);DispatchMessage(&message);}}}
std::vector<std::string> activated;
std::ofstream* activeLog=NULL;
int fixtureMessages=0;
class WorldChild:public CMDIChildWndEx {
public:
 std::string identity;HWND content=NULL;bool closeAllowed=true;
 WorldChild(const char* id):identity(id){}
 LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp) override {
  const LRESULT result=CMDIChildWndEx::WindowProc(message,wp,lp);
  if(message==WM_MDIACTIVATE && (HWND)lp==m_hWnd)activated.push_back(identity);
  if(message==WM_SIZE && ::IsWindow(content)){CRect r;GetClientRect(r);::MoveWindow(content,0,0,r.Width(),r.Height(),TRUE);}
  return result;
 }
};
class GroupedFixtureFrame:public WorkspaceTestFrame {
public:
 void SettleTabs(){m_wndClientArea.UpdateTabs(TRUE);RecalcLayout();Pump();}
};
bool Exposed(HWND child,HWND client){RECT a,b;GetWindowRect(child,&a);::GetClientRect(client,&b);MapWindowPoints(NULL,client,(POINT*)&a,2);RECT overlap;if(!IntersectRect(&overlap,&a,&b))return false;POINT point={(overlap.left+overlap.right)/2,(overlap.top+overlap.bottom)/2};HWND hit=ChildWindowFromPointEx(client,point,CWP_SKIPINVISIBLE|CWP_SKIPDISABLED|CWP_SKIPTRANSPARENT);return hit==child;}
bool Screenshot(HWND window,const wchar_t* path){RECT r;GetWindowRect(window,&r);int width=r.right-r.left,height=r.bottom-r.top;HDC screen=GetDC(window),dc=CreateCompatibleDC(screen);BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* bits=NULL;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,NULL,0);HGDIOBJ old=SelectObject(dc,bitmap);BOOL printed=PrintWindow(window,dc,0);bool varied=false;if(printed && width>0 && height>0){DWORD* pixels=(DWORD*)bits;for(int i=1;i<width*height;++i)if((pixels[i]&0xffffff)!=(pixels[0]&0xffffff)){varied=true;break;}}BITMAPFILEHEADER file={};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+width*height*4;HANDLE output=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD wrote;bool ok=printed && varied && output!=INVALID_HANDLE_VALUE;if(output!=INVALID_HANDLE_VALUE){ok=WriteFile(output,&file,sizeof(file),&wrote,NULL)&&ok;ok=WriteFile(output,&info.bmiHeader,sizeof(info.bmiHeader),&wrote,NULL)&&ok;ok=WriteFile(output,bits,width*height*4,&wrote,NULL)&&ok;CloseHandle(output);}SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(window,screen);return ok;}
class GroupedMdiApp:public CWinApp {
 int result_=1;
 int DoMessageBox(LPCTSTR prompt,UINT,UINT) override {++fixtureMessages;if(activeLog)*activeLog<<"FAIL unexpected fixture MFC message: "<<prompt<<"\n";return IDOK;}
 BOOL InitInstance() override {
  AfxOleInit();AfxInitRichEdit2();std::ofstream out("grouped-mdi-results.txt");out.setf(std::ios::unitbuf);activeLog=&out;const bool legacyNotepad=strstr(GetCommandLineA(),"--legacy-notepad")!=NULL;bool ok=true;
  out<<"STAGE install appearance\n";workspace_appearance::Install();workspace_style::SetTheme(workspace_style::Theme::Dark);
  auto* frame=new GroupedFixtureFrame;ok=frame->Create(NULL,"Grouped MDI - fictional production appearance fixture",WS_OVERLAPPEDWINDOW,CRect(60,60,1280,860))!=FALSE;m_pMainWnd=frame;frame->EnableWorkspaceDocking();
  CMDITabInfo options;options.m_tabLocation=CMFCTabCtrl::LOCATION_TOP;options.m_style=CMFCTabCtrl::STYLE_3D;options.m_bTabIcons=FALSE;options.m_bTabCloseButton=FALSE;options.m_bActiveTabCloseButton=TRUE;options.m_bEnableTabSwap=TRUE;options.m_bAutoColor=FALSE;options.m_bFlatFrame=TRUE;options.m_nTabBorderSize=0;options.m_bDocumentMenu=TRUE;
  out<<"STAGE enable groups\n";{WorkspaceModernResources resources;frame->EnableMDITabbedGroups(TRUE,options);}
  frame->ShowWindow(SW_SHOWNOACTIVATE);workspace_style::ApplySurface(frame->m_hWnd);Pump();
  auto create=[&](const char* identity,const char* title,const wchar_t* text){auto* child=new WorldChild(identity);if(!child->Create(NULL,title,WS_CHILD|WS_VISIBLE|WS_OVERLAPPEDWINDOW,CRect(0,0,800,600),frame,NULL)){delete child;return (WorldChild*)NULL;}child->content=CreateWindowExW(0,L"RichEdit20W",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE|ES_READONLY,0,0,900,700,child->m_hWnd,NULL,AfxGetInstanceHandle(),NULL);::SendMessageW(child->content,EM_SETBKGNDCOLOR,0,RGB(24,26,30));CHARFORMAT2W fmt={};fmt.cbSize=sizeof(fmt);fmt.dwMask=CFM_COLOR|CFM_FACE|CFM_SIZE;fmt.crTextColor=RGB(230,233,239);fmt.yHeight=240;wcscpy_s(fmt.szFaceName,L"Consolas");::SendMessageW(child->content,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&fmt);child->ActivateFrame(SW_SHOW);Pump();frame->RecalcLayout();Pump();return child;};
  out<<"STAGE create Mira\n";auto* mira=create("fictional-harbor:mira:runtime","Silver Harbor / Mira",L"Mira's original output and composer views remain owned by this world.\r\n\r\nFictional native fixture. No game connection or command.");
  out<<"STAGE create Rowan\n";auto* rowan=create("fictional-harbor:rowan:runtime","Silver Harbor / Rowan",L"Rowan's original output remains in this distinct MDI child.\r\n\r\nChanging tabs must activate only this runtime connection.");
  if(!mira || !rowan){out<<"FAIL unable to construct native Ex children\n";frame->DestroyWindow();return FALSE;}
  frame->SettleTabs();const HWND miraWindow=mira->m_hWnd,rowanWindow=rowan->m_hWnd,miraText=mira->content,rowanText=rowan->content;::SendMessageW(miraText,EM_SETSEL,6,14);const LRESULT selection=::SendMessageW(miraText,EM_GETSEL,0,0);
  CMFCTabCtrl* tabs=mira->GetRelatedTabGroup();ok=Check(out,"actual Ex world children share native two-tab group",tabs && tabs==rowan->GetRelatedTabGroup() && tabs->GetTabsNum()==2)&&ok;
  if(!tabs){frame->DestroyWindow();return FALSE;}
  CString label;tabs->GetTabLabel(tabs->GetTabFromHwnd(miraWindow),label);ok=Check(out,"first native document tab uses original visible game and own character label",label=="Silver Harbor / Mira")&&ok;
  tabs->GetTabLabel(tabs->GetTabFromHwnd(rowanWindow),label);ok=Check(out,"second same-game tab distinguishes the other own character",label=="Silver Harbor / Rowan")&&ok;
  ok=Check(out,"tabbed children remove old caption and resize borders",!(mira->GetStyle()&WS_CAPTION) && !(rowan->GetStyle()&WS_THICKFRAME))&&ok;
  activated.clear();tabs->SetActiveTab(tabs->GetTabFromHwnd(miraWindow));Pump();ok=Check(out,"native tab selection activates only the captured original Mira child",frame->MDIGetActive()==mira && !activated.empty() && activated.back()==mira->identity)&&ok;
  activated.clear();tabs->SetActiveTab(tabs->GetTabFromHwnd(rowanWindow));Pump();ok=Check(out,"native second-tab selection activates only Rowan runtime child",frame->MDIGetActive()==rowan && !activated.empty() && activated.back()==rowan->identity)&&ok;
  ::SendMessageW(frame->m_hWndMDIClient,WM_MDINEXT,(WPARAM)rowanWindow,FALSE);Pump();ok=Check(out,"native next-document keyboard command returns to first world",frame->MDIGetActive()==mira)&&ok;
  const LONG_PTR before=(LONG_PTR)::SendMessageW(miraText,WM_GETFONT,0,0);workspace_style::SetTheme(workspace_style::Theme::Light);frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_FRAME|RDW_ALLCHILDREN);ok=Check(out,"light grouped native MDI screenshot has nonblank pixels",Screenshot(frame->m_hWnd,L"grouped-mdi-light.bmp"))&&ok;
  workspace_style::SetTheme(workspace_style::Theme::Dark);frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_FRAME|RDW_ALLCHILDREN);ok=Check(out,"dark grouped native MDI screenshot has nonblank pixels",Screenshot(frame->m_hWnd,L"grouped-mdi-dark.bmp"))&&ok;
  ok=Check(out,"group and theme changes retain content handles font and selection",::IsWindow(miraText) && ::IsWindow(rowanText) && before==(LONG_PTR)::SendMessageW(miraText,WM_GETFONT,0,0) && selection==::SendMessageW(miraText,EM_GETSEL,0,0))&&ok;
  CMDIChildWnd* notepad=legacyNotepad?(CMDIChildWnd*)new CMDIChildWnd:(CMDIChildWnd*)new WorldChild("fictional-notepad");ok=notepad->Create(NULL,"Notepad - fictional unsaved text",WS_CHILD|WS_VISIBLE|WS_OVERLAPPEDWINDOW,CRect(40,80,720,540),frame,NULL)!=FALSE&&ok;const HWND noteWindow=notepad->m_hWnd;
  HWND noteText=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"Fictional Notepad content.\r\nMust remain reachable while grouped world tabs are enabled.",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_MULTILINE,0,0,650,380,noteWindow,NULL,AfxGetInstanceHandle(),NULL);if(!legacyNotepad)((WorldChild*)notepad)->content=noteText;notepad->ActivateFrame(SW_SHOW);frame->SettleTabs();
  ok=Check(out,legacyNotepad?"legacy Notepad stays outside native Ex-world group":"migrated Ex Notepad joins native document tabs alongside both worlds",legacyNotepad?(tabs->GetTabsNum()==2 && tabs->GetTabFromHwnd(noteWindow)<0):(tabs->GetTabsNum()==3 && tabs->GetTabFromHwnd(noteWindow)>=0))&&ok;
  ok=Check(out,"Notepad native activation retains its active child identity",frame->MDIGetActive()==notepad)&&ok;
  const bool noteExposed=::IsWindowVisible(noteWindow) && Exposed(noteWindow,frame->m_hWndMDIClient);ok=Check(out,"active Notepad is visually exposed above grouped documents",noteExposed)&&ok;
  frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_FRAME|RDW_ALLCHILDREN);ok=Check(out,"Notepad and grouped worlds screenshot",Screenshot(frame->m_hWnd,legacyNotepad?L"grouped-mdi-legacy-notepad.bmp":L"grouped-mdi-native-notepad.bmp"))&&ok;
  tabs->SetActiveTab(tabs->GetTabFromHwnd(rowanWindow));Pump();ok=Check(out,"native world-tab selection returns from Notepad",frame->MDIGetActive()==rowan && Exposed(rowanWindow,frame->m_hWndMDIClient))&&ok;
  const UINT noteCommand=(UINT)::GetDlgCtrlID(noteWindow);::SendMessageW(frame->m_hWnd,WM_COMMAND,noteCommand,0);Pump();ok=Check(out,"native window-list command can return to Notepad",frame->MDIGetActive()==notepad && ::IsWindow(noteText))&&ok;
  ::SendMessageW(noteWindow,WM_CLOSE,0,0);Pump();ok=Check(out,"closing Notepad returns to surviving grouped world",!::IsWindow(noteWindow) && (frame->MDIGetActive()==mira || frame->MDIGetActive()==rowan) && tabs->GetTabsNum()==2)&&ok;
  tabs->SetActiveTab(tabs->GetTabFromHwnd(rowanWindow));Pump();{WorkspaceModernResources resources;frame->MDITabNewGroup(TRUE);}frame->SettleTabs();
  ok=Check(out,"native vertical tab-group split retains both original world surfaces",mira->GetRelatedTabGroup() && rowan->GetRelatedTabGroup() && mira->GetRelatedTabGroup()!=rowan->GetRelatedTabGroup() && mira->GetRelatedTabGroup()->GetTabsNum()==1 && rowan->GetRelatedTabGroup()->GetTabsNum()==1 && ::IsWindow(miraText) && ::IsWindow(rowanText) && Exposed(miraWindow,frame->m_hWndMDIClient) && Exposed(rowanWindow,frame->m_hWndMDIClient))&&ok;
  frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_FRAME|RDW_ALLCHILDREN);ok=Check(out,"split world-tab groups screenshot",Screenshot(frame->m_hWnd,L"grouped-mdi-split-dark.bmp"))&&ok;
  {WorkspaceModernResources resources;frame->MDITabMoveToNextGroup(FALSE);}frame->SettleTabs();tabs=mira->GetRelatedTabGroup();
  ok=Check(out,"native move-to-previous-group rejoins world tabs without replaying content",tabs && tabs==rowan->GetRelatedTabGroup() && tabs->GetTabsNum()==2 && mira->content==miraText && rowan->content==rowanText && selection==::SendMessageW(miraText,EM_GETSEL,0,0))&&ok;
  tabs->SetActiveTab(tabs->GetTabFromHwnd(miraWindow));Pump();::SendMessageW(miraWindow,WM_CLOSE,0,0);Pump();ok=Check(out,"closing active grouped world keeps other world and original content",!::IsWindow(miraWindow) && ::IsWindow(rowanWindow) && ::IsWindow(rowanText) && frame->MDIGetActive()==rowan && rowan->GetRelatedTabGroup() && rowan->GetRelatedTabGroup()->GetTabsNum()==1)&&ok;
  {WorkspaceModernResources resources;frame->EnableMDITabbedGroups(FALSE,options);}Pump();ok=Check(out,"classic window mode restores native child caption and resize style",!frame->AreMDITabs() && (rowan->GetStyle()&WS_CAPTION) && (rowan->GetStyle()&WS_THICKFRAME) && rowan->content==rowanText)&&ok;
  {WorkspaceModernResources resources;frame->EnableMDITabbedGroups(TRUE,options);}Pump();ok=Check(out,"enabling grouped mode again retains surviving world and original content",frame->AreMDITabs() && rowan->GetRelatedTabGroup() && rowan->GetRelatedTabGroup()->GetTabsNum()==1 && rowan->content==rowanText)&&ok;
  ok=Check(out,"native framework resources avoid all MFC exception dialogs",fixtureMessages==0)&&ok;
  ::SendMessageW(rowanWindow,WM_CLOSE,0,0);Pump();frame->DestroyWindow();out<<(ok?"PASS":"FAIL")<<" grouped-MDI native software fixture; no live game or physical desktop acceptance\n";activeLog=NULL;result_=ok?0:1;return FALSE;
 }
 int ExitInstance() override{return result_;}
};GroupedMdiApp app;
