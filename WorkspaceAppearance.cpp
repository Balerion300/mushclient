#include "stdafx.h"
#include "WorkspaceAppearance.h"
#include "WorkspaceStyle.h"
#include "workspace/WorkspaceNativeTabs.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxvisualmanager.h>
#include <afxdockablepane.h>
#include <afxpaneframewnd.h>
#include <afxcaptionbutton.h>
#include <afxtabctrl.h>
#include <afxtabbedpane.h>
#include <afxautohidebutton.h>
#include <afxautohidebar.h>
#pragma pop_macro("Translate")
#include <commctrl.h>
#include <string>

IMPLEMENT_DYNCREATE(CValyriaChannelTabs,CMFCTabCtrl)

namespace {
namespace style = workspace_style;
bool HighContrast() {return style::Colors().highContrast;}
bool Dark(const style::Palette& p) {return GetRValue(p.background)<128;}
// MFC/native controls own geometry and hit testing. The channel-tab subclass
// and caption-height virtual establish larger real rectangles before painting.
class CWorkspaceVisualManager : public CMFCVisualManager {
  DECLARE_DYNCREATE(CWorkspaceVisualManager)
  CBrush m_tabBackground,m_tabBorder;
  COLORREF m_background=(COLORREF)-1,m_border=(COLORREF)-1;
public:
  int GetDockingPaneCaptionExtraHeight() const override {return HighContrast()?0:workspace_style::Scale(NULL,12);}
  BOOL OnEraseMDIClientArea(CDC* dc,CRect rect) override {
    if(HighContrast())return CMFCVisualManager::OnEraseMDIClientArea(dc,rect);
    dc->FillSolidRect(rect,style::Colors().background);return TRUE;
  }
  void OnFillBarBackground(CDC* dc,CBasePane* bar,CRect rect,CRect clip,BOOL nc=FALSE) override {
    if(HighContrast() || (!bar->IsKindOf(RUNTIME_CLASS(CDockablePane)) && !bar->IsKindOf(RUNTIME_CLASS(CMFCAutoHideBar)))) {
      CMFCVisualManager::OnFillBarBackground(dc,bar,rect,clip,nc);return;
    }
    dc->FillSolidRect(clip.IsRectEmpty()?rect:clip,style::Colors().background);
  }
  COLORREF OnDrawPaneCaption(CDC* dc,CDockablePane* pane,BOOL active,CRect rect,CRect buttons) override {
    if(HighContrast())return CMFCVisualManager::OnDrawPaneCaption(dc,pane,active,rect,buttons);
    const auto p=style::Colors();dc->FillSolidRect(rect,p.surface);
    const int line=max(1,style::Scale(pane->GetSafeHwnd(),2));
    if(active)dc->FillSolidRect(rect.left,rect.top,line,rect.Height(),p.accent);
    dc->FillSolidRect(rect.left,rect.bottom-1,rect.Width(),1,p.border);
    return active?p.text:p.muted;
  }
  void OnDrawCaptionButton(CDC* dc,CMFCCaptionButton* button,BOOL active,BOOL horizontal,BOOL maximized,BOOL disabled,int image=-1) override {
    if(HighContrast()) {CMFCVisualManager::OnDrawCaptionButton(dc,button,active,horizontal,maximized,disabled,image);return;}
    const auto p=style::Colors();const CRect rect=button->GetRect();
    if(!disabled && (button->m_bFocused || button->m_bPushed || button->m_bDroppedDown))
      dc->FillSolidRect(rect,button->m_bPushed?p.pressed:p.hover);
    const CMenuImages::IMAGES_IDS id=image==-1?button->GetIconID(horizontal,maximized):(CMenuImages::IMAGES_IDS)image;
    CMenuImages::Draw(dc,id,rect,disabled?CMenuImages::ImageGray:Dark(p)?CMenuImages::ImageWhite:CMenuImages::ImageBlack);
  }
  void OnDrawPaneBorder(CDC* dc,CBasePane* pane,CRect& rect) override {
    if(HighContrast() || !pane->IsKindOf(RUNTIME_CLASS(CDockablePane))) {CMFCVisualManager::OnDrawPaneBorder(dc,pane,rect);return;}
    // MFC consumes border dimensions in this virtual. Preserve its rectangle
    // accounting, then replace only the pixels in the reserved border area.
    CRect original=rect;CMFCVisualManager::OnDrawPaneBorder(dc,pane,rect);
    const int saved=dc->SaveDC();dc->ExcludeClipRect(rect);dc->FillSolidRect(original,style::Colors().background);dc->RestoreDC(saved);
  }
  void OnDrawPaneDivider(CDC* dc,CPaneDivider* divider,CRect rect,BOOL autoHide) override {
    if(HighContrast()) {CMFCVisualManager::OnDrawPaneDivider(dc,divider,rect,autoHide);return;}
    const auto p=style::Colors();dc->FillSolidRect(rect,p.background);
    if(rect.Width()>rect.Height())dc->FillSolidRect(rect.left,rect.CenterPoint().y,rect.Width(),1,p.border);
    else dc->FillSolidRect(rect.CenterPoint().x,rect.top,1,rect.Height(),p.border);
  }
  void OnEraseTabsArea(CDC* dc,CRect rect,const CMFCBaseTabCtrl* tabs) override {
    if(HighContrast()) {CMFCVisualManager::OnEraseTabsArea(dc,rect,tabs);return;}
    dc->FillSolidRect(rect,style::Colors().background);
  }
  BOOL OnEraseTabsFrame(CDC* dc,CRect rect,const CMFCBaseTabCtrl* tabs) override {
    if(HighContrast())return CMFCVisualManager::OnEraseTabsFrame(dc,rect,tabs);
    dc->FillSolidRect(rect,style::Colors().background);return TRUE;
  }
  void GetTabFrameColors(const CMFCBaseTabCtrl* tabs,COLORREF& dark,COLORREF& black,COLORREF& highlight,
      COLORREF& face,COLORREF& shadow,COLORREF& light,CBrush*& faceBrush,CBrush*& blackBrush) override {
    if(HighContrast()){CMFCVisualManager::GetTabFrameColors(tabs,dark,black,highlight,face,shadow,light,faceBrush,blackBrush);return;}
    const auto p=style::Colors();
    if(m_background!=p.background){m_tabBackground.DeleteObject();m_tabBackground.CreateSolidBrush(p.background);m_background=p.background;}
    if(m_border!=p.border){m_tabBorder.DeleteObject();m_tabBorder.CreateSolidBrush(p.border);m_border=p.border;}
    // The native frame keeps its original reserved geometry; equal edge colors
    // remove the old raised/sunken bevel without resizing the active document.
    dark=black=highlight=face=shadow=light=p.background;
    faceBrush=&m_tabBackground;blackBrush=&m_tabBorder;
  }
  void OnDrawTab(CDC* dc,CRect rect,int index,BOOL active,const CMFCBaseTabCtrl* tabs) override {
    if(HighContrast()) {CMFCVisualManager::OnDrawTab(dc,rect,index,active,tabs);return;}
    const auto p=style::Colors();dc->FillSolidRect(rect,active?p.surface:p.background);
    if(active) {
      const int line=max(2,style::Scale(tabs->GetSafeHwnd(),2));
      dc->FillSolidRect(rect.left,tabs->GetLocation()==CMFCBaseTabCtrl::LOCATION_BOTTOM?rect.bottom-line:rect.top,rect.Width(),line,p.accent);
    } else dc->FillSolidRect(rect.right-1,rect.top+5,1,max(0,rect.Height()-10),p.border);
    // MFC still owns label/image/close-button layout and native tab accessibility.
    CFont* old=dc->SelectObject(CFont::FromHandle(style::UiFont(tabs->GetSafeHwnd())));
    CMFCVisualManager::OnDrawTabContent(dc,rect,index,active,tabs,active?p.text:p.muted);
    dc->SelectObject(old);
    if(active && (::GetFocus()==tabs->GetSafeHwnd()) && !(::SendMessage(tabs->GetSafeHwnd(),WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)) {
      rect.DeflateRect(3,4);dc->SetTextColor(p.text);dc->SetBkColor(p.surface);dc->DrawFocusRect(rect);
    }
  }
  COLORREF GetTabTextColor(const CMFCBaseTabCtrl* tabs,int index,BOOL active) override {
    if(HighContrast())return CMFCVisualManager::GetTabTextColor(tabs,index,active);
    return active?style::Colors().text:style::Colors().muted;
  }
  void OnDrawTabCloseButton(CDC* dc,CRect rect,const CMFCBaseTabCtrl* tabs,BOOL hot,BOOL pressed,BOOL disabled) override {
    if(HighContrast()){CMFCVisualManager::OnDrawTabCloseButton(dc,rect,tabs,hot,pressed,disabled);return;}
    const auto p=style::Colors();if(hot || pressed)dc->FillSolidRect(rect,pressed?p.pressed:p.hover);
    rect.DeflateRect(4,4);style::DrawIcon(dc->GetSafeHdc(),rect,style::Icon::Close,disabled?p.disabled:p.text);
  }
  void OnEraseTabsButton(CDC* dc,CRect rect,CMFCButton* button,CMFCBaseTabCtrl* tabs) override {
    if(HighContrast()){CMFCVisualManager::OnEraseTabsButton(dc,rect,button,tabs);return;}
    dc->FillSolidRect(rect,style::Colors().background);
  }
  void OnDrawTabsButtonBorder(CDC* dc,CRect& rect,CMFCButton* button,UINT state,CMFCBaseTabCtrl* tabs) override {
    if(HighContrast()){CMFCVisualManager::OnDrawTabsButtonBorder(dc,rect,button,state,tabs);return;}
    if(state&(CDIS_HOT|CDIS_SELECTED))dc->Draw3dRect(rect,style::Colors().border,style::Colors().border);
  }
  COLORREF OnFillMiniFrameCaption(CDC* dc,CRect rect,CPaneFrameWnd* frame,BOOL active) override {
    if(HighContrast())return CMFCVisualManager::OnFillMiniFrameCaption(dc,rect,frame,active);
    const auto p=style::Colors();dc->FillSolidRect(rect,p.surface);
    if(active)dc->FillSolidRect(rect.left,rect.top,max(2,style::Scale(frame->GetSafeHwnd(),2)),rect.Height(),p.accent);
    dc->FillSolidRect(rect.left,rect.bottom-1,rect.Width(),1,p.border);return active?p.text:p.muted;
  }
  void OnDrawMiniFrameBorder(CDC* dc,CPaneFrameWnd* frame,CRect rect,CRect size) override {
    if(HighContrast() || frame->GetPreDockState()!=PDS_NOTHING) {CMFCVisualManager::OnDrawMiniFrameBorder(dc,frame,rect,size);return;}
    const auto p=style::Colors();const int saved=dc->SaveDC();CRect inside=rect;
    inside.DeflateRect(size.left,size.top,size.right,size.bottom);dc->ExcludeClipRect(inside);
    dc->FillSolidRect(rect,p.background);dc->Draw3dRect(rect,p.border,p.border);dc->RestoreDC(saved);
  }
  void OnFillAutoHideButtonBackground(CDC* dc,CRect rect,CMFCAutoHideButton* button) override {
    if(HighContrast()){CMFCVisualManager::OnFillAutoHideButtonBackground(dc,rect,button);return;}
    const auto p=style::Colors();dc->FillSolidRect(rect,button->IsHighlighted()?p.hover:p.background);
  }
  void OnDrawAutoHideButtonBorder(CDC* dc,CRect rect,CRect size,CMFCAutoHideButton* button) override {
    if(HighContrast()){CMFCVisualManager::OnDrawAutoHideButtonBorder(dc,rect,size,button);return;}
    const auto p=style::Colors();dc->Draw3dRect(rect,p.border,p.border);
  }
  COLORREF GetAutoHideButtonTextColor(CMFCAutoHideButton* button) override {
    return HighContrast()?CMFCVisualManager::GetAutoHideButtonTextColor(button):style::Colors().text;
  }
};
IMPLEMENT_DYNCREATE(CWorkspaceVisualManager,CMFCVisualManager)

void PaintNativeTabs(HWND window,HDC dc) {
  const auto p=style::Colors(window);RECT client;::GetClientRect(window,&client);
  HBRUSH brush=CreateSolidBrush(p.background);FillRect(dc,&client,brush);DeleteObject(brush);
  RECT page=client;TabCtrl_AdjustRect(window,FALSE,&page);
  brush=CreateSolidBrush(p.border);FrameRect(dc,&page,brush);DeleteObject(brush);
  const int selected=TabCtrl_GetCurSel(window),count=TabCtrl_GetItemCount(window);
  HFONT old=(HFONT)SelectObject(dc,style::UiFont(window));SetBkMode(dc,TRANSPARENT);
  HIMAGELIST images=TabCtrl_GetImageList(window);
  for(int i=0;i<count;++i) {
    RECT rect;if(!TabCtrl_GetItemRect(window,i,&rect))continue;
    // Paint precisely the same item bounds used by native hit testing.
    const bool active=i==selected;brush=CreateSolidBrush(active?p.surface:p.background);FillRect(dc,&rect,brush);DeleteObject(brush);
    if(active){RECT line=rect;line.bottom=line.top+max(2,style::Scale(window,2));brush=CreateSolidBrush(p.accent);FillRect(dc,&line,brush);DeleteObject(brush);}
    wchar_t text[512]={};TCITEMW item={};item.mask=TCIF_TEXT|TCIF_IMAGE;item.pszText=text;item.cchTextMax=512;
    if(!::SendMessageW(window,TCM_GETITEMW,i,(LPARAM)&item))continue;
    RECT label=rect;label.left+=style::Scale(window,10);label.right-=style::Scale(window,10);
    if(images && item.iImage>=0){int width=0,height=0;ImageList_GetIconSize(images,&width,&height);ImageList_Draw(images,item.iImage,dc,label.left,(rect.top+rect.bottom-height)/2,ILD_TRANSPARENT);label.left+=width+style::Scale(window,6);}
    SetTextColor(dc,::IsWindowEnabled(window)?(active?p.text:p.muted):p.disabled);
    DrawTextW(dc,text,-1,&label,DT_SINGLELINE|DT_VCENTER|DT_CENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    if(active && ::GetFocus()==window && !(::SendMessageW(window,WM_QUERYUISTATE,0,0)&UISF_HIDEFOCUS)){
      InflateRect(&label,-1,-4);SetTextColor(dc,p.text);SetBkColor(dc,p.surface);DrawFocusRect(dc,&label);
    }
  }
  SelectObject(dc,old);
}
LRESULT CALLBACK TabProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR) {
  if(!HighContrast()) {
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);PaintNativeTabs(window,dc);EndPaint(window,&ps);return 0;}
    if(message==WM_PRINTCLIENT){PaintNativeTabs(window,(HDC)wp);return 0;}
  }
  if(message==WM_NCDESTROY)RemoveWindowSubclass(window,TabProc,id);
  const LRESULT result=DefSubclassProc(window,message,wp,lp);
  if(::IsWindow(window) && (message==WM_SETFOCUS || message==WM_KILLFOCUS || message==WM_KEYDOWN || message==WM_KEYUP ||
     message==WM_LBUTTONDOWN || message==WM_LBUTTONUP || message==WM_ENABLE || message==WM_THEMECHANGED || message==WM_UPDATEUISTATE ||
     message==TCM_SETCURSEL || message==TCM_SETITEMW || message==TCM_SETITEMA || message==TCM_INSERTITEMW || message==TCM_INSERTITEMA ||
     message==TCM_DELETEITEM || message==TCM_DELETEALLITEMS))InvalidateRect(window,NULL,FALSE);
  return result;
}
void PaintStatus(HWND window,HDC dc){const auto p=style::Colors(window);RECT bounds;::GetClientRect(window,&bounds);HBRUSH brush=CreateSolidBrush(p.surface);FillRect(dc,&bounds,brush);DeleteObject(brush);RECT line=bounds;line.bottom=line.top+1;brush=CreateSolidBrush(p.border);FillRect(dc,&line,brush);DeleteObject(brush);const auto old=SelectObject(dc,style::UiFont(window));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,p.muted);const int count=(int)::SendMessage(window,SB_GETPARTS,0,0);for(int i=0;i<count;++i){RECT r;if(!::SendMessage(window,SB_GETRECT,i,(LPARAM)&r))continue;const LRESULT length=::SendMessageW(window,SB_GETTEXTLENGTHW,i,0);if(HIWORD(length)&SBT_OWNERDRAW){DRAWITEMSTRUCT draw={};draw.CtlType=ODT_STATIC;draw.CtlID=::GetDlgCtrlID(window);draw.itemID=i;draw.itemAction=ODA_DRAWENTIRE;draw.hwndItem=window;draw.hDC=dc;draw.rcItem=r;draw.itemData=(ULONG_PTR)::SendMessageW(window,SB_GETTEXTW,i,0);::SendMessage(::GetParent(window),WM_DRAWITEM,draw.CtlID,(LPARAM)&draw);continue;}std::vector<wchar_t> text((size_t)LOWORD(length)+1);::SendMessageW(window,SB_GETTEXTW,i,(LPARAM)text.data());r.left+=style::Scale(window,8);r.right-=style::Scale(window,5);DrawTextW(dc,text.data(),-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);}
  if((GetWindowLongPtrW(window,GWL_STYLE)&SBARS_SIZEGRIP) && !::IsZoomed(::GetParent(window))){
    RECT grip=bounds;grip.left=max(grip.left,grip.right-GetSystemMetrics(SM_CXVSCROLL));grip.top=max(grip.top,grip.bottom-GetSystemMetrics(SM_CYHSCROLL));
    HBRUSH background=CreateSolidBrush(p.surface);FillRect(dc,&grip,background);DeleteObject(background);
    HPEN pen=CreatePen(PS_SOLID,1,p.muted);HGDIOBJ previous=SelectObject(dc,pen);
    for(int distance=4;distance<=12;distance+=4){MoveToEx(dc,grip.right-style::Scale(window,distance),grip.bottom-3,NULL);LineTo(dc,grip.right-3,grip.bottom-style::Scale(window,distance));}
    SelectObject(dc,previous);DeleteObject(pen);
  }
  SelectObject(dc,old);
}
LRESULT CALLBACK StatusProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
  if(!HighContrast()){if(message==WM_ERASEBKGND)return 1;if(message==WM_PAINT){PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);PaintStatus(window,dc);EndPaint(window,&paint);return 0;}if(message==WM_PRINTCLIENT){PaintStatus(window,(HDC)wp);return 0;}}
  if(message==WM_NCDESTROY)RemoveWindowSubclass(window,StatusProc,id);
  const LRESULT result=DefSubclassProc(window,message,wp,lp);if(::IsWindow(window)&&(message==SB_SETTEXTA||message==SB_SETTEXTW||message==SB_SETPARTS||message==WM_THEMECHANGED))::InvalidateRect(window,NULL,FALSE);return result;
}
BOOL CALLBACK RefreshFrame(HWND window,LPARAM) {
  workspace_appearance::ApplyNativeFrame(window);
  ::RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);
  return TRUE;
}
BOOL CALLBACK StyleDirectTab(HWND child,LPARAM parent) {
  if(::GetParent(child)==(HWND)parent)workspace_appearance::StyleTabs(child);
  return TRUE;
}
void AppearanceChanged(HWND window) {
  if(window) {
    if(!::IsWindow(window) || (::GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD))return;
    workspace_appearance::ApplyNativeFrame(window);
    // Tool windows opt in through ApplySurface. Inspect immediate children
    // only; game/plugin/Scintilla descendants are never traversed or enrolled.
    EnumChildWindows(window,StyleDirectTab,(LPARAM)window);
    workspace_appearance::Refresh(window);
  }
  else workspace_appearance::Refresh();
}
}

namespace workspace_appearance {
void Install() {
  static bool installed=false;
  if(!installed){CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CWorkspaceVisualManager));CTabbedPane::m_pTabWndRTC=RUNTIME_CLASS(CValyriaChannelTabs);installed=true;}
  style::SetAppearanceChangedHandler(AppearanceChanged);
}
bool ApplyNativeFrame(HWND window) {
  if(!::IsWindow(window) || (::GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD))return false;
  typedef HRESULT (WINAPI *DwmSetFn)(HWND,DWORD,LPCVOID,DWORD);
  static HMODULE module=LoadLibraryW(L"dwmapi.dll");
  static DwmSetFn set=module?(DwmSetFn)GetProcAddress(module,"DwmSetWindowAttribute"):NULL;
  if(!set)return false;
  const auto p=style::Colors(window);const BOOL dark=!p.highContrast && Dark(p);
  // Documented DWMWA values, dynamically resolved for Windows 7 compatibility.
  // No fallback to the undocumented Windows 10 attribute 19 or private ordinals.
  const HRESULT darkResult=set(window,20,&dark,sizeof(dark));
  const COLORREF caption=p.highContrast?(COLORREF)0xffffffff:p.background;
  const COLORREF text=p.highContrast?(COLORREF)0xffffffff:p.text;
  const COLORREF border=p.highContrast?(COLORREF)0xffffffff:p.border;
  const HRESULT captionResult=set(window,35,&caption,sizeof(caption));
  set(window,36,&text,sizeof(text));set(window,34,&border,sizeof(border));
  return SUCCEEDED(darkResult) || SUCCEEDED(captionResult);
}
void Refresh(HWND root) {
  if(root)RefreshFrame(root,0);
  else EnumThreadWindows(GetCurrentThreadId(),RefreshFrame,0);
}
bool StyleTabs(HWND tabs) {
  wchar_t name[64]={};if(!::IsWindow(tabs) || !GetClassNameW(tabs,name,64) || _wcsicmp(name,WC_TABCONTROLW))return false;
  const DWORD flags=(DWORD)GetWindowLongPtrW(tabs,GWL_STYLE);
  if(flags&(TCS_VERTICAL|TCS_MULTILINE|TCS_BUTTONS|TCS_OWNERDRAWFIXED|TCS_BOTTOM))return false;
  if(!SetWindowSubclass(tabs,TabProc,(UINT_PTR)&TabProc,0))return false;
  ::SendMessageW(tabs,WM_SETFONT,(WPARAM)style::UiFont(tabs),TRUE);
  InvalidateRect(tabs,NULL,TRUE);return true;
}
bool StyleStatusBar(HWND status){if(!::IsWindow(status))return false;if(!SetWindowSubclass(status,StatusProc,(UINT_PTR)&StatusProc,0))return false;style::StyleControl(status);::SendMessage(status,SB_SETMINHEIGHT,style::Scale(status,27),0);::SendMessage(status,WM_SIZE,0,0);return true;}
}
