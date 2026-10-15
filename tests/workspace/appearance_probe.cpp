#include "stdafx.h"
#include "ModernFixture.h"
#include "WorkspaceAppearance.h"
#include "WorkspaceStyle.h"
#include <afxvisualmanager.h>
#include <oleacc.h>
#include <fstream>
extern bool StyleFixtureHighContrast;
namespace style=workspace_style;
bool Check(std::ofstream& out,const char* name,bool value){out<<(value?"PASS ":"FAIL ")<<name<<"\n";return value;}
bool Screenshot(HWND window,const wchar_t* path){RECT r;GetWindowRect(window,&r);const int width=r.right-r.left,height=r.bottom-r.top;HDC screen=GetDC(window),dc=CreateCompatibleDC(screen);BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* bits=NULL;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,NULL,0);HGDIOBJ old=SelectObject(dc,bitmap);const BOOL printed=PrintWindow(window,dc,0);bool nonblank=false;if(printed && width>0 && height>0){const DWORD* pixels=(DWORD*)bits;const DWORD first=pixels[0]&0x00ffffff;for(int i=1;i<width*height;++i)if((pixels[i]&0x00ffffff)!=first){nonblank=true;break;}}BITMAPFILEHEADER file={};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+width*height*4;HANDLE output=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD written;bool ok=printed && nonblank && output!=INVALID_HANDLE_VALUE;if(output!=INVALID_HANDLE_VALUE){ok=WriteFile(output,&file,sizeof(file),&written,NULL)&&ok;ok=WriteFile(output,&info.bmiHeader,sizeof(info.bmiHeader),&written,NULL)&&ok;ok=WriteFile(output,bits,width*height*4,&written,NULL)&&ok;CloseHandle(output);}SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(window,screen);return ok;}
COLORREF Pixel(HWND window,int x,int y){RECT r;GetClientRect(window,&r);HDC screen=GetDC(window),dc=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,r.right,r.bottom);HGDIOBJ old=SelectObject(dc,bitmap);SendMessageW(window,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);COLORREF color=GetPixel(dc,x,y);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(window,screen);return color;}
class ProbePane:public CDockablePane {
public:
 HWND text=NULL;
 bool CreatePane(CFrameWnd* parent,UINT id,const char* title,const wchar_t* content,HFONT font){
   if(!Create(title,parent,CRect(0,0,640,400),TRUE,id,WS_CHILD|WS_VISIBLE|CBRS_RIGHT|CBRS_FLOAT_MULTI))return false;
   text=CreateWindowExW(0,L"RichEdit20W",content,WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_READONLY|WS_VSCROLL,0,0,620,370,m_hWnd,NULL,AfxGetInstanceHandle(),NULL);
   ::SendMessageW(text,WM_SETFONT,(WPARAM)font,0);::SendMessageW(text,EM_SETBKGNDCOLOR,0,RGB(24,26,30));
   CHARFORMAT2W fmt={};fmt.cbSize=sizeof(fmt);fmt.dwMask=CFM_COLOR;fmt.crTextColor=RGB(230,233,239);::SendMessageW(text,EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&fmt);return text!=NULL;
 }
 LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp) override {const LRESULT value=CDockablePane::WindowProc(message,wp,lp);if(message==WM_SIZE && ::IsWindow(text)){CRect r;GetClientRect(r);::MoveWindow(text,0,0,r.Width(),r.Height(),TRUE);}return value;}
};
int tabChanges=0;
LRESULT CALLBACK ToolProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){if(msg==WM_NOTIFY && ((NMHDR*)lp)->code==TCN_SELCHANGE)++tabChanges;return DefWindowProcW(h,msg,wp,lp);}
class AppearanceApp:public CWinApp {
 int result=1;
 BOOL InitInstance() override {
  AfxOleInit();AfxInitRichEdit2();INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_TAB_CLASSES};InitCommonControlsEx(&controls);
  std::ofstream out("appearance-results.txt");out.setf(std::ios::unitbuf);bool ok=true;
  workspace_appearance::Install();style::SetTheme(style::Theme::Light);
  auto* frame=new WorkspaceTestFrame;ok=frame->Create(NULL,"MUSHclient - native chrome exploration / fictional fixture",WS_OVERLAPPEDWINDOW,CRect(40,40,1240,840))!=FALSE;m_pMainWnd=frame;frame->EnableWorkspaceDocking();
  HFONT contentFont=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH,L"Consolas");
  const wchar_t* sample=L"Silver Harbor / Mira\r\n\r\nThe watch bell rings over the harbor.\r\nMira folds the map and looks toward the eastern gate.\r\n\r\n[Chat] Rowan says, \"We are ready when you are.\"\r\n\r\nFictional native fixture. No game connection or command.\r\n\r\nOriginal content font and colors remain independent of chrome.";
  ProbePane first,second,side;ok=first.CreatePane(frame,0xe921,"Silver Harbor / Mira - Main",sample,contentFont)&&ok;ok=second.CreatePane(frame,0xe922,"Silver Harbor / Mira - Chat",L"Chat\r\n\r\n[Chat] Rowan: At the eastern gate.\r\n[Chat] Mira: Bringing the maps.",contentFont)&&ok;
  ok=side.CreatePane(frame,0xe923,"Worlds and characters",L"SILVER HARBOR\r\n  Mira - connected\r\n    Main\r\n    Chat (2)\r\n    Tells\r\n\r\nASH HILL\r\n  Rowan - offline",contentFont)&&ok;
  for(auto* pane:{&first,&second,&side}){pane->EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(pane);pane->ShowPane(TRUE,FALSE,FALSE);}
  second.AttachToTabWnd(&first,DM_SHOW,FALSE);frame->DockPane(&side,AFX_IDW_DOCKBAR_LEFT);frame->ShowWindow(SW_SHOWNOACTIVATE);frame->RecalcLayout();frame->UpdateWindow();
  auto* group=first.GetParentTabbedPane();const HWND originalText=first.text;const HFONT originalFont=(HFONT)::SendMessageW(first.text,WM_GETFONT,0,0);::SendMessageW(first.text,EM_SETSEL,5,18);const LRESULT originalSelection=::SendMessageW(first.text,EM_GETSEL,0,0);
  ok=Check(out,"production visual manager installed",!strcmp(CMFCVisualManager::GetInstance()->GetRuntimeClass()->m_lpszClassName,"CWorkspaceVisualManager"))&&ok;
  ok=Check(out,"native tab group has both original panes",group && group->GetTabsNum()==2)&&ok;
  workspace_appearance::Refresh(frame->m_hWnd);frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_FRAME|RDW_ALLCHILDREN);
  ok=Check(out,"light production dock chrome screenshot",Screenshot(frame->m_hWnd,L"appearance-dock-light.bmp"))&&ok;
  style::SetTheme(style::Theme::Dark);frame->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_FRAME|RDW_ALLCHILDREN);
  ok=Check(out,"dark production dock chrome screenshot",Screenshot(frame->m_hWnd,L"appearance-dock-dark.bmp"))&&ok;
  ok=Check(out,"theme switch keeps original content HWND font and selection",first.text==originalText && (HFONT)::SendMessageW(first.text,WM_GETFONT,0,0)==originalFont && ::SendMessageW(first.text,EM_GETSEL,0,0)==originalSelection)&&ok;
  ok=Check(out,"float original native pane",frame->FloatWorkspacePane(first,CRect(130,150,890,630))!=FALSE)&&ok;
  ok=Check(out,"float removes old tab and retains original content",first.IsFloating() && (!second.IsTabbed() || (second.GetParentTabbedPane() && second.GetParentTabbedPane()->GetTabsNum()==1)) && first.text==originalText)&&ok;
  first.ShowPane(TRUE,FALSE,TRUE);
  if(first.GetParentMiniFrame()){first.GetParentMiniFrame()->ShowWindow(SW_SHOWNOACTIVATE);first.GetParentMiniFrame()->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_UPDATENOW|RDW_FRAME|RDW_ALLCHILDREN);ok=Check(out,"visible floating production chrome screenshot has nonblank pixels",::IsWindowVisible(first.GetParentMiniFrame()->m_hWnd) && Screenshot(first.GetParentMiniFrame()->m_hWnd,L"appearance-float-dark.bmp"))&&ok;}
  ok=Check(out,"native recent redock restores original two-tab group",first.DockToRecentPos()!=FALSE && first.GetParentTabbedPane() && first.GetParentTabbedPane()->GetTabsNum()==2 && first.text==originalText)&&ok;
  const LONG_PTR originalStyle=GetWindowLongPtrW(frame->m_hWnd,GWL_STYLE),originalExStyle=GetWindowLongPtrW(frame->m_hWnd,GWL_EXSTYLE);const HMENU systemMenu=::GetSystemMenu(frame->m_hWnd,FALSE);
  workspace_appearance::ApplyNativeFrame(frame->m_hWnd);
  ok=Check(out,"DWM title appearance preserves native frame styles and system menu",GetWindowLongPtrW(frame->m_hWnd,GWL_STYLE)==originalStyle && GetWindowLongPtrW(frame->m_hWnd,GWL_EXSTYLE)==originalExStyle && systemMenu==::GetSystemMenu(frame->m_hWnd,FALSE) && (originalStyle&WS_THICKFRAME) && (originalStyle&WS_MAXIMIZEBOX))&&ok;
  RECT bounds;GetWindowRect(frame->m_hWnd,&bounds);const LRESULT hit=::SendMessageW(frame->m_hWnd,WM_NCHITTEST,0,MAKELPARAM((bounds.left+bounds.right)/2,bounds.top+12));ok=Check(out,"native caption hit test retained",hit==HTCAPTION || hit==HTTOP)&&ok;
  {CClientDC source(frame);CDC area;area.CreateCompatibleDC(&source);CBitmap bitmap;bitmap.CreateCompatibleBitmap(&source,64,32);CBitmap* old=area.SelectObject(&bitmap);const BOOL filled=CMFCVisualManager::GetInstance()->OnEraseMDIClientArea(&area,CRect(0,0,64,32));ok=Check(out,"unused MDI work area follows selected chrome palette",filled && area.GetPixel(20,15)==style::Colors().background)&&ok;area.SelectObject(old);}

  WNDCLASSW cls={};cls.lpfnWndProc=ToolProc;cls.hInstance=AfxGetInstanceHandle();cls.lpszClassName=L"WorkspaceAppearanceProbeTool";RegisterClassW(&cls);
  HWND tool=CreateWindowExW(0,cls.lpszClassName,L"Local draft editor - fictional native fixture",WS_OVERLAPPEDWINDOW,160,160,900,620,frame->m_hWnd,NULL,AfxGetInstanceHandle(),NULL);
  HWND tabs=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP,14,16,850,44,tool,(HMENU)101,AfxGetInstanceHandle(),NULL);
  TCITEMW item={};item.mask=TCIF_TEXT;item.pszText=L"Opening scene";::SendMessageW(tabs,TCM_INSERTITEMW,0,(LPARAM)&item);item.pszText=L"Room description";::SendMessageW(tabs,TCM_INSERTITEMW,1,(LPARAM)&item);item.pszText=L"Review commands";::SendMessageW(tabs,TCM_INSERTITEMW,2,(LPARAM)&item);
  HWND editor=CreateWindowExW(0,L"RichEdit20W",L"@@ Fictional unsent draft\r\n@desc #123=Lanterns glow along the harbor wall.\r\n\r\nNative editor tabs retain their original Windows selection messages.",WS_CHILD|WS_VISIBLE|ES_MULTILINE|WS_TABSTOP,14,72,850,450,tool,NULL,AfxGetInstanceHandle(),NULL);::SendMessageW(editor,WM_SETFONT,(WPARAM)contentFont,0);
  const LONG_PTR tabStyle=GetWindowLongPtrW(tabs,GWL_STYLE);const int tabCount=TabCtrl_GetItemCount(tabs);const HWND tabParent=GetParent(tabs);
  style::ApplySurface(tool);ShowWindow(tool,SW_SHOWNOACTIVATE);UpdateWindow(tool);RECT tabRect;TabCtrl_GetItemRect(tabs,0,&tabRect);
  ok=Check(out,"top-level surface explicitly enrolls its immediate native editor tab",Pixel(tabs,tabRect.left+3,tabRect.top)==style::Colors(tabs).accent)&&ok;
  ok=Check(out,"native tab style parent and item count retained",GetWindowLongPtrW(tabs,GWL_STYLE)==tabStyle && TabCtrl_GetItemCount(tabs)==tabCount && GetParent(tabs)==tabParent)&&ok;
  ::SetFocus(tabs);::SendMessageW(tabs,WM_KEYDOWN,VK_RIGHT,0);::SendMessageW(tabs,WM_KEYUP,VK_RIGHT,0);ok=Check(out,"native Right key changes selected tab with native notification",TabCtrl_GetCurSel(tabs)==1 && tabChanges>0 && ::GetFocus()==tabs)&&ok;
  TabCtrl_SetCurSel(tabs,2);ok=Check(out,"programmatic native tab selection retained",TabCtrl_GetCurSel(tabs)==2)&&ok;
  IAccessible* accessible=NULL;LONG count=0;const HRESULT hr=AccessibleObjectFromWindow(tabs,OBJID_CLIENT,IID_IAccessible,(void**)&accessible);if(accessible){accessible->get_accChildCount(&count);accessible->Release();}
  ok=Check(out,"native tab accessibility provider retains its three items",SUCCEEDED(hr) && count>=3)&&ok;
  const HFONT beforeEditor=(HFONT)::SendMessageW(editor,WM_GETFONT,0,0);::SetFocus(editor);::SendMessageW(editor,EM_SETSEL,3,14);const LRESULT editorSel=::SendMessageW(editor,EM_GETSEL,0,0);
  style::SetTheme(style::Theme::Light);RedrawWindow(tool,NULL,NULL,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);ok=Check(out,"light native editor tab screenshot",Screenshot(tool,L"appearance-editor-light.bmp"))&&ok;
  style::SetTheme(style::Theme::Dark);RedrawWindow(tool,NULL,NULL,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);ok=Check(out,"dark native editor tab screenshot",Screenshot(tool,L"appearance-editor-dark.bmp"))&&ok;
  ok=Check(out,"tab theme leaves editor content font selection and focus intact",(HFONT)::SendMessageW(editor,WM_GETFONT,0,0)==beforeEditor && ::SendMessageW(editor,EM_GETSEL,0,0)==editorSel && ::GetFocus()==editor)&&ok;
  HWND vertical=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|TCS_VERTICAL|TCS_MULTILINE,0,0,80,80,tool,NULL,AfxGetInstanceHandle(),NULL);ok=Check(out,"custom vertical tabs stay under native rendering",!workspace_appearance::StyleTabs(vertical))&&ok;
  HWND nested=CreateWindowExW(0,L"STATIC",L"",WS_CHILD,0,0,1,1,tool,NULL,AfxGetInstanceHandle(),NULL);HWND unowned=CreateWindowExW(0,WC_TABCONTROLW,L"",WS_CHILD|WS_TABSTOP,0,0,80,40,nested,NULL,AfxGetInstanceHandle(),NULL);HFONT fixed=(HFONT)GetStockObject(ANSI_FIXED_FONT);::SendMessageW(unowned,WM_SETFONT,(WPARAM)fixed,0);style::ApplySurface(tool);ok=Check(out,"nested content/plugin tabs are not enrolled",(HFONT)::SendMessageW(unowned,WM_GETFONT,0,0)==fixed)&&ok;
  StyleFixtureHighContrast=true;style::SetTheme(style::Theme::Dark);ok=Check(out,"high contrast takes precedence over selected dark palette",style::Colors().highContrast)&&ok;
  TabCtrl_GetItemRect(tabs,2,&tabRect);const COLORREF highPixel=Pixel(tabs,tabRect.left+3,tabRect.top);ok=Check(out,"native tabs yield to Windows high contrast painting",highPixel!=style::Colors().accent)&&ok;
  CClientDC frameDc(frame);CDC memory;memory.CreateCompatibleDC(&frameDc);CBitmap bitmap;bitmap.CreateCompatibleBitmap(&frameDc,64,32);CBitmap* old=memory.SelectObject(&bitmap);CMFCVisualManager::GetInstance()->OnDrawPaneCaption(&memory,&first,TRUE,CRect(0,0,64,32),CRect());ok=Check(out,"high contrast docking caption uses native system active caption",memory.GetPixel(20,15)==GetSysColor(COLOR_ACTIVECAPTION))&&ok;memory.SelectObject(old);
  ok=Check(out,"high contrast unused MDI area returns to native Windows rendering",!CMFCVisualManager::GetInstance()->OnEraseMDIClientArea(&memory,CRect(0,0,64,32)))&&ok;
  StyleFixtureHighContrast=false;style::SetTheme(style::Theme::Light);
  DestroyWindow(tool);first.DestroyWindow();second.DestroyWindow();side.DestroyWindow();frame->DestroyWindow();DeleteObject(contentFont);
  out<<(ok?"PASS":"FAIL")<<" production native appearance fixture; software messages and rendering only, no physical-device or screen-reader acceptance\n";result=ok?0:1;return FALSE;
 }
 int ExitInstance() override{return result;}
};AppearanceApp app;
