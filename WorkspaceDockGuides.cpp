#include "stdafx.h"
#include "WorkspaceDockGuides.h"
#include "WorkspaceStyle.h"
#include "WorkspaceResources.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxole.h>
#include <afxmdiframewndex.h>
#include <afxsmartdockingmanager.h>
#pragma pop_macro("Translate")

namespace {
typedef CSmartDockingStandaloneGuide::SDMarkerPlace Place;
const Place None=CSmartDockingStandaloneGuide::sdNONE;
const Place Left=CSmartDockingStandaloneGuide::sdCLEFT;
const Place Right=CSmartDockingStandaloneGuide::sdCRIGHT;
const Place Top=CSmartDockingStandaloneGuide::sdCTOP;
const Place Bottom=CSmartDockingStandaloneGuide::sdCBOTTOM;
const Place Middle=CSmartDockingStandaloneGuide::sdCMIDDLE;
const COLORREF Transparent=RGB(1,2,3);
thread_local CSmartDockingManager* creatingGuides=NULL;
class NativeGuideAccess:public CSmartDockingManager {
public:
  static bool Bounds(CSmartDockingManager& engine,CRect& rect){
    auto& window=engine.*&NativeGuideAccess::m_wndPlaceMarker;
    if(!window.GetSafeHwnd() || !window.IsWindowVisible())return false;
    window.GetWindowRect(rect);return true;
  }
};
class DropGuides;

class DropPreview:public CWnd {
public:
  DropGuides* guides=NULL;
protected:
  afx_msg void OnPaint();
  afx_msg LRESULT OnPrintClient(WPARAM dc,LPARAM);
  afx_msg BOOL OnEraseBkgnd(CDC*){return TRUE;}
  afx_msg LRESULT OnNcHitTest(CPoint){return HTTRANSPARENT;}
  DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(DropPreview,CWnd)
  ON_WM_PAINT()
  ON_MESSAGE(WM_PRINTCLIENT,OnPrintClient)
  ON_WM_ERASEBKGND()
  ON_WM_NCHITTEST()
END_MESSAGE_MAP()

class DropGuide:public CSmartDockingGroupGuide {
  DropGuides* owner=NULL;
public:
  void Bind(DropGuides* value,Place place){owner=value;m_nSideNo=place;}
  BOOL IsPtIn(CPoint point) const override;
  void Highlight(BOOL value=TRUE) override;
  void Show(BOOL=TRUE) override {}
  void AdjustPos(CRect) override {}
  void Destroy() override {}
};

class DropGuides:public CSmartDockingGroupGuidesManager {
  DECLARE_DYNCREATE(DropGuides)
  DropGuide own[5];
  DropPreview preview;
  CRect host;
  bool shown=false;
  Place selected=None;
  unsigned visibleMask=0;
  CSmartDockingManager* engine;
public:
  DropGuides():engine(creatingGuides){}
  bool Native()const{return workspace_style::Colors().highContrast || host.Width()<300 || host.Height()<200 || !preview.GetSafeHwnd();}
  CSmartDockingGroupGuide* Original(Place place){return CSmartDockingGroupGuidesManager::GetGuide(place);}
  void SyncNative(){for(int i=0;i<5;++i)Original((Place)(Left+i))->SetVisible(own[i].IsVisible(),FALSE);}
  bool Enabled(Place place)const {
    return place>=Left && place<=Middle && own[place-Left].IsVisible() && (place!=Middle || m_bMiddleIsOn);
  }
  void Create(CWnd* owner) override {
    CSmartDockingGroupGuidesManager::Create(owner);
    for(int i=0;i<5;++i)own[i].Bind(this,(Place)(Left+i));
    preview.guides=this;
    const CString name=AfxRegisterWndClass(0);
    if(preview.CreateEx(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_LAYERED,name,"Workspace drop preview",WS_POPUP,CRect(0,0,1,1),owner,0))
      ::SetLayeredWindowAttributes(preview.GetSafeHwnd(),Transparent,220,LWA_COLORKEY|LWA_ALPHA);
  }
  void Destroy() override {
    shown=false;
    if(preview.GetSafeHwnd())preview.DestroyWindow();
    CSmartDockingGroupGuidesManager::Destroy();
  }
  CSmartDockingGroupGuide* GetGuide(Place place) override {
    return place>=Left && place<=Middle?&own[place-Left]:NULL;
  }
  void Show(BOOL value=TRUE) override {
    shown=value!=FALSE;selected=None;SyncNative();
    CSmartDockingGroupGuidesManager::Show(value && Native());
    if(preview.GetSafeHwnd())preview.SetWindowPos(&CWnd::wndTopMost,host.left,host.top,max(1,host.Width()),max(1,host.Height()),SWP_NOACTIVATE|(shown&&!Native()?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
    if(shown && !Native())preview.Invalidate(FALSE);
  }
  BOOL AdjustPos(CRect rect,int middle=-1) override {
    SyncNative();
    const BOOL changed=CSmartDockingGroupGuidesManager::AdjustPos(rect,middle);
    unsigned mask=0;for(int i=0;i<5;++i)if(Enabled((Place)(Left+i)))mask|=1u<<i;
    const bool different=host!=rect || mask!=visibleMask;host=rect;visibleMask=mask;
    if(shown){CSmartDockingGroupGuidesManager::Show(Native());
      if(preview.GetSafeHwnd())preview.SetWindowPos(&CWnd::wndTopMost,host.left,host.top,max(1,host.Width()),max(1,host.Height()),SWP_NOACTIVATE|(!Native()?SWP_SHOWWINDOW:SWP_HIDEWINDOW));
      if(different){if(!Native())preview.Invalidate(FALSE);else m_Wnd.Invalidate(FALSE);}}
    return changed || different;
  }
  Place At(CPoint point)const {
    if(!shown || !host.PtInRect(point))return None;
    const int x=point.x-host.left,y=point.y-host.top,w=host.Width(),h=host.Height();
    const double left=(double)x/w,right=(double)(w-x)/w,top=(double)y/h,bottom=(double)(h-y)/h;
    Place place=Middle;
    if(min(min(left,right),min(top,bottom))<.25){
      const double closest=min(min(left,right),min(top,bottom));
      place=closest==left?Left:closest==right?Right:closest==top?Top:Bottom;
    }
    return Enabled(place)?place:None;
  }
  BOOL Hit(Place place,CPoint point)const{return At(point)==place;}
  void Select(Place place,BOOL value) {
    const Place next=value?place:(selected==place?None:selected);
    if(next!=selected){selected=next;if(preview.GetSafeHwnd())preview.Invalidate(FALSE);}
  }
  void Paint(CDC& dc,CRect bounds) {
    dc.FillSolidRect(bounds,Transparent);
    const auto palette=workspace_style::Colors();
    CFont* font=dc.SelectObject(CFont::FromHandle(workspace_style::UiFont(preview.GetSafeHwnd())));
    dc.SetBkMode(TRANSPARENT);
    const int inset=workspace_style::Scale(preview.GetSafeHwnd(),5);
    if(selected!=None){CRect area=bounds;
      if(selected==Left)area.right=area.CenterPoint().x;
      if(selected==Right)area.left=area.CenterPoint().x;
      if(selected==Top)area.bottom=area.CenterPoint().y;
      if(selected==Bottom)area.top=area.CenterPoint().y;
      // Use MFC's final projected rectangle when available, including minimum
      // pane sizes, rather than promising an unconstrained 50/50 split.
      CRect projected;if(engine && NativeGuideAccess::Bounds(*engine,projected)){projected.OffsetRect(-host.left,-host.top);area.IntersectRect(bounds,projected);}
      if(!area.IsRectEmpty()){area.DeflateRect(inset,inset);dc.FillSolidRect(area,palette.hover);dc.Draw3dRect(area,palette.accent,palette.accent);}
    }
    const CPoint center=bounds.CenterPoint();
    const int height=workspace_style::Scale(preview.GetSafeHwnd(),34),gap=workspace_style::Scale(preview.GetSafeHwnd(),6);
    const int width=min((bounds.Width()-gap*4)/3,workspace_style::Scale(preview.GetSafeHwnd(),138));
    const wchar_t* labels[]={L"Place left",L"Place right",L"Place above",L"Place below",L"Join tabs"};
    for(int i=0;i<5;++i){const Place place=(Place)(Left+i);if(!Enabled(place))continue;
      CRect chip(center.x-width/2,center.y-height/2,center.x+width/2,center.y+height/2);
      if(place==Left)chip.OffsetRect(-width-gap,0);
      if(place==Right)chip.OffsetRect(width+gap,0);
      if(place==Top)chip.OffsetRect(0,-height-gap);
      if(place==Bottom)chip.OffsetRect(0,height+gap);
      const bool active=selected==place;dc.FillSolidRect(chip,active?palette.accent:palette.surface);dc.Draw3dRect(chip,palette.border,palette.border);
      dc.SetTextColor(active?palette.accentText:palette.text);::DrawTextW(dc.GetSafeHdc(),labels[i],-1,chip,DT_SINGLELINE|DT_CENTER|DT_VCENTER|DT_NOPREFIX);
    }
    dc.SelectObject(font);
  }
};
IMPLEMENT_DYNCREATE(DropGuides,CSmartDockingGroupGuidesManager)
BOOL DropGuide::IsPtIn(CPoint point)const {
  return !owner || !owner->Enabled(m_nSideNo)?FALSE:owner->Native()?owner->Original(m_nSideNo)->IsPtIn(point):owner->Hit(m_nSideNo,point);
}
void DropGuide::Highlight(BOOL value) {
  if(!owner)return;
  if(owner->Native())owner->Original(m_nSideNo)->Highlight(value);else owner->Select(m_nSideNo,value);
}
void DropPreview::OnPaint(){CPaintDC dc(this);CRect bounds;GetClientRect(bounds);SetWindowPos(&CWnd::wndTopMost,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);if(guides)guides->Paint(dc,bounds);}
LRESULT DropPreview::OnPrintClient(WPARAM dc,LPARAM){CRect bounds;GetClientRect(bounds);if(guides && dc)guides->Paint(*CDC::FromHandle((HDC)dc),bounds);return 0;}
class DockingManagerAccess:public CDockingManager {
public:
  static CSmartDockingManager*& Guides(CDockingManager& manager){return manager.*&DockingManagerAccess::m_pSDManager;}
};
}

void InstallWorkspaceDockGuides(CMDIFrameWndEx& frame,CDockingManager& docking)
{
  WorkspaceModernResources resources;
  // Consume pending native parameter changes before replacing the owned manager.
  // Recreating it in place leaves its highlighter HWND alive (Debug assertion).
  docking.GetSmartDockingManager();
  CSmartDockingManager*& manager=DockingManagerAccess::Guides(docking);
  delete manager;manager=NULL;
  manager=new CSmartDockingManager;
  struct CreationScope {CSmartDockingManager* previous;CreationScope(CSmartDockingManager* value):previous(creatingGuides){creatingGuides=value;}~CreationScope(){creatingGuides=previous;}} scope(manager);
  manager->Create(&frame,NULL,RUNTIME_CLASS(DropGuides));
}
