#include "stdafx.h"
#include "WorkspaceStyle.h"
#include <commctrl.h>
#include <oleacc.h>
#include <fstream>
#include <vector>
#include <cmath>
extern bool StyleFixtureHighContrast,StyleFixtureLargeFont;

namespace {
unsigned clicked = 0;
LRESULT CALLBACK Host(HWND window,UINT message,WPARAM wp,LPARAM lp) {
  if(message==WM_COMMAND && LOWORD(wp)==101 && HIWORD(wp)==BN_CLICKED){++clicked;return 0;}
  return DefWindowProcW(window,message,wp,lp);
}
double Luminance(COLORREF color) {
  auto channel=[](double c){c/=255.;return c<=0.04045?c/12.92:pow((c+0.055)/1.055,2.4);};
  return channel(GetRValue(color))*0.2126+channel(GetGValue(color))*0.7152+channel(GetBValue(color))*0.0722;
}
double Contrast(COLORREF a,COLORREF b){double x=Luminance(a),y=Luminance(b);return (max(x,y)+0.05)/(min(x,y)+0.05);}
void SaveBitmap(const std::wstring& file,HDC dc,HBITMAP image,int width,int height) {
  BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  std::vector<BYTE> pixels(width*height*4);GetDIBits(dc,image,0,height,pixels.data(),&info,DIB_RGB_COLORS);
  BITMAPFILEHEADER header={};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+(DWORD)pixels.size();
  std::ofstream out(file,std::ios::binary);out.write((char*)&header,sizeof(header));out.write((char*)&info.bmiHeader,sizeof(info.bmiHeader));out.write((char*)pixels.data(),pixels.size());
}
HWND Child(HWND parent,const wchar_t* kind,const wchar_t* text,DWORD style,int id,int x,int y,int width,int height) {
  return CreateWindowExW(!_wcsicmp(kind,L"EDIT")?WS_EX_CLIENTEDGE:0,kind,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,parent,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
}
void RenderChild(HDC target,HWND child) {
  RECT bounds;GetWindowRect(child,&bounds);MapWindowPoints(NULL,GetParent(child),(POINT*)&bounds,2);
  const int saved=SaveDC(target);OffsetViewportOrgEx(target,bounds.left,bounds.top,NULL);
  SendMessageW(child,WM_PRINT,(WPARAM)target,PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND|PRF_CHILDREN);RestoreDC(target,saved);
}
}
class StyleProbe:public CWinApp {
  int result=1;
public:
  BOOL InitInstance() override {
    CoInitialize(NULL);INITCOMMONCONTROLSEX init={sizeof(init),ICC_WIN95_CLASSES};InitCommonControlsEx(&init);
    wchar_t executable[32768];GetModuleFileNameW(NULL,executable,32768);std::wstring directory=executable;directory=directory.substr(0,directory.find_last_of(L"\\/")+1);
    std::ofstream log(directory+L"style-results.txt");unsigned checks=0,failed=0;
    auto check=[&](bool ok,const char* label){++checks;if(!ok){++failed;log<<"FAIL "<<label<<"\n";}};
    WNDCLASSW kind={};kind.lpfnWndProc=Host;kind.hInstance=GetModuleHandle(NULL);kind.lpszClassName=L"FictionalWorkspaceStyleProbe";RegisterClassW(&kind);
    HWND host=CreateWindowExW(0,kind.lpszClassName,L"Fictional native style fixture",WS_POPUP,50,50,1040,620,NULL,NULL,kind.hInstance,NULL);
    check(host!=NULL,"hidden host created");workspace_style::ApplySurface(host);
    HWND content=Child(host,L"EDIT",L"Content font preserved",ES_MULTILINE,200,10,540,400,40);
    HFONT contentFont=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,FIXED_PITCH,L"Consolas");SendMessageW(content,WM_SETFONT,(WPARAM)contentFont,FALSE);
    HWND title=Child(host,L"STATIC",L"Workspace chrome | original native components",0,1,20,15,1000,30);workspace_style::StyleControl(title);
    HWND button=Child(host,L"BUTTON",L"&Connect",WS_TABSTOP|BS_DEFPUSHBUTTON,101,20,70,152,38);workspace_style::StyleButton(button,workspace_style::Icon::Connect,true,L"Connect the selected fictional world");
    HWND other=Child(host,L"BUTTON",L"Channels",WS_TABSTOP|BS_PUSHBUTTON,102,184,70,150,38);workspace_style::StyleButton(other,workspace_style::Icon::Channels,false,L"Review channel routing");
    HWND disabled=Child(host,L"BUTTON",L"Disconnect",WS_TABSTOP|BS_PUSHBUTTON,103,346,70,160,38);workspace_style::StyleButton(disabled,workspace_style::Icon::Disconnect);EnableWindow(disabled,FALSE);
    HWND field=Child(host,L"EDIT",L"Original world: Fictional Harbor",ES_AUTOHSCROLL|WS_TABSTOP,104,20,132,486,32);workspace_style::StyleControl(field);
    HWND checkBox=Child(host,L"BUTTON",L"Share selected output (explicit opt-in)",BS_AUTOCHECKBOX|WS_TABSTOP,105,20,184,486,28);workspace_style::StyleButton(checkBox,workspace_style::Icon::None);
    check((HFONT)SendMessageW(content,WM_GETFONT,0,0)==contentFont,"surface styling preserves unenrolled content font");
    check((HFONT)SendMessageW(field,WM_GETFONT,0,0)==workspace_style::UiFont(field),"explicit chrome UI font");
    check(workspace_style::Scale(host,24)==MulDiv(24,workspace_style::Dpi(host),96),"DPI spacing");
    check(SendMessageW(button,WM_GETDLGCODE,0,0)&DLGC_BUTTON,"native keyboard button behavior");
    SendMessageW(button,BM_CLICK,0,0);check(clicked==1,"native command dispatch exactly once");
    SetFocus(button);check(GetFocus()==button,"native keyboard focus retained");
    SendMessageW(checkBox,BM_CLICK,0,0);check(SendMessageW(checkBox,BM_GETCHECK,0,0)==BST_CHECKED,"checkbox retains native toggle behavior");
    IAccessible* accessible=NULL;bool named=false,role=false,unavailable=false;
    if(SUCCEEDED(AccessibleObjectFromWindow(button,OBJID_CLIENT,IID_IAccessible,(void**)&accessible))){VARIANT self;VariantInit(&self);self.vt=VT_I4;self.lVal=CHILDID_SELF;BSTR name=NULL;VARIANT value;VariantInit(&value);named=SUCCEEDED(accessible->get_accName(self,&name))&&name&&wcsstr(name,L"Connect");SysFreeString(name);role=SUCCEEDED(accessible->get_accRole(self,&value))&&value.vt==VT_I4&&value.lVal==ROLE_SYSTEM_PUSHBUTTON;VariantClear(&value);accessible->Release();}
    check(named && role,"native accessibility name and pushbutton role");
    if(SUCCEEDED(AccessibleObjectFromWindow(disabled,OBJID_CLIENT,IID_IAccessible,(void**)&accessible))){VARIANT self,value;VariantInit(&self);VariantInit(&value);self.vt=VT_I4;self.lVal=CHILDID_SELF;unavailable=SUCCEEDED(accessible->get_accState(self,&value))&&value.vt==VT_I4&&(value.lVal&STATE_SYSTEM_UNAVAILABLE);VariantClear(&value);accessible->Release();}check(unavailable,"native disabled accessibility state");
    HDC screen=GetDC(NULL),imageDC=CreateCompatibleDC(screen);HBITMAP image=CreateCompatibleBitmap(screen,1040,620);HGDIOBJ old=SelectObject(imageDC,image);
    workspace_style::SetTheme(workspace_style::Theme::Light);SendMessageW(button,WM_PRINTCLIENT,(WPARAM)imageDC,PRF_CLIENT);COLORREF normal=GetPixel(imageDC,140,28);SendMessageW(button,WM_MOUSEMOVE,0,0);SendMessageW(button,WM_PRINTCLIENT,(WPARAM)imageDC,PRF_CLIENT);check(GetPixel(imageDC,140,28)!=normal,"primary hover feedback");SendMessageW(button,WM_MOUSELEAVE,0,0);SendMessageW(button,BM_SETSTATE,TRUE,0);SendMessageW(button,WM_PRINTCLIENT,(WPARAM)imageDC,PRF_CLIENT);check(GetPixel(imageDC,140,28)!=normal,"primary pressed feedback");SendMessageW(button,BM_SETSTATE,FALSE,0);
    for(unsigned theme=0;theme<2;++theme){workspace_style::SetTheme(theme?workspace_style::Theme::Dark:workspace_style::Theme::Light);const auto palette=workspace_style::Colors(host);check(Contrast(palette.text,palette.background)>=4.5,"body text contrast");check(Contrast(palette.accentText,palette.accent)>=4.5,"primary action text contrast");check((HFONT)SendMessageW(content,WM_GETFONT,0,0)==contentFont,"theme change preserves content font");
      const int saved=SaveDC(imageDC);SetViewportOrgEx(imageDC,theme*520,0,NULL);RECT panel={0,0,520,620};HBRUSH background=CreateSolidBrush(palette.background);FillRect(imageDC,&panel,background);DeleteObject(background);for(HWND child=GetWindow(host,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))RenderChild(imageDC,child);
      SetBkMode(imageDC,TRANSPARENT);SetTextColor(imageDC,palette.text);SelectObject(imageDC,workspace_style::UiFont(host));RECT name={20,238,500,264};DrawTextW(imageDC,theme?L"Dark chrome":L"Light chrome",-1,&name,DT_SINGLELINE);
      for(int icon=1;icon<=static_cast<int>(workspace_style::Icon::Pause);++icon){int col=(icon-1)%7,row=(icon-1)/7;RECT bounds={24+col*68,286+row*58,48+col*68,310+row*58};workspace_style::DrawIcon(imageDC,bounds,static_cast<workspace_style::Icon>(icon),palette.text);}RestoreDC(imageDC,saved);
    }
    SelectObject(imageDC,old);SaveBitmap(directory+L"style-components.bmp",imageDC,image,1040,620);DeleteObject(image);DeleteDC(imageDC);ReleaseDC(NULL,screen);
    HFONT previousFont=workspace_style::UiFont(button);StyleFixtureLargeFont=true;SendMessageW(button,WM_SETTINGCHANGE,SPI_SETNONCLIENTMETRICS,0);HFONT largerFont=workspace_style::UiFont(button);LOGFONTW oldMetrics={},newMetrics={};
    check(largerFont!=previousFont && GetObjectW(previousFont,sizeof(oldMetrics),&oldMetrics) && GetObjectW(largerFont,sizeof(newMetrics),&newMetrics) && newMetrics.lfHeight<oldMetrics.lfHeight,"system font change creates new generation and preserves borrowed old font");
    SendMessageW(other,WM_SETTINGCHANGE,SPI_SETNONCLIENTMETRICS,0);check(workspace_style::UiFont(other)==largerFont,"same settings broadcast reuses font generation");StyleFixtureLargeFont=false;SendMessageW(button,WM_SETTINGCHANGE,SPI_SETNONCLIENTMETRICS,0);
    StyleFixtureHighContrast=true;workspace_style::SetTheme(workspace_style::Theme::Dark);const auto high=workspace_style::Colors();check(high.highContrast && high.background==GetSysColor(COLOR_WINDOW) && high.accentText==GetSysColor(COLOR_HIGHLIGHTTEXT),"simulated high contrast overrides selected dark palette");SendMessageW(button,BM_SETSTATE,TRUE,0);
    HDC highDC=CreateCompatibleDC(NULL);BITMAPINFO highInfo={};highInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);highInfo.bmiHeader.biWidth=152;highInfo.bmiHeader.biHeight=-38;highInfo.bmiHeader.biPlanes=1;highInfo.bmiHeader.biBitCount=32;void* highPixels=NULL;HBITMAP highImage=CreateDIBSection(highDC,&highInfo,DIB_RGB_COLORS,&highPixels,NULL,0);HGDIOBJ highOld=SelectObject(highDC,highImage);SendMessageW(button,WM_PRINTCLIENT,(WPARAM)highDC,PRF_CLIENT);check(GetTextColor(highDC)==GetSysColor(COLOR_HIGHLIGHTTEXT),"high contrast pressed action uses system highlight text");SelectObject(highDC,highOld);DeleteObject(highImage);DeleteDC(highDC);SendMessageW(button,BM_SETSTATE,FALSE,0);StyleFixtureHighContrast=false;
    workspace_style::SetTheme(workspace_style::Theme::System);check(workspace_style::GetTheme()==workspace_style::Theme::System,"system theme selection");
    HIGHCONTRAST contrast={sizeof(contrast)};SystemParametersInfo(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);check(workspace_style::Colors().highContrast==!!(contrast.dwFlags&HCF_HIGHCONTRASTON),"system high contrast preference respected");
    BOOL animation=TRUE;SystemParametersInfo(SPI_GETCLIENTAREAANIMATION,0,&animation,0);check(workspace_style::ReducedMotion()==!animation,"system reduced motion preference respected");
    workspace_style::FitToWorkArea(host);RECT bounds;GetWindowRect(host,&bounds);MONITORINFO monitor={sizeof(monitor)};GetMonitorInfo(MonitorFromWindow(host,MONITOR_DEFAULTTONEAREST),&monitor);check(bounds.left>=monitor.rcWork.left&&bounds.top>=monitor.rcWork.top&&bounds.right<=monitor.rcWork.right&&bounds.bottom<=monitor.rcWork.bottom,"work area bounds");
    DestroyWindow(host);DeleteObject(contentFont);check(!IsWindow(button),"native button/tooltip lifetime cleanup");
    log<<"checks="<<checks<<" failed="<<failed<<"\n";CoUninitialize();result=failed?1:0;return FALSE;
  }
  int ExitInstance() override{return result;}
};
StyleProbe styleProbe;
