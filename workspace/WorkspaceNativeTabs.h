#pragma once
#include "../WorkspaceStyle.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxole.h>
#include <afxtabctrl.h>
#pragma pop_macro("Translate")

// Actual native tab geometry, not an oversized paint pass over small hit boxes.
// MFC's tabbed-pane runtime-class factory constructs this real derived object.
// Native docking, accessibility, drag state, and original child HWNDs stay owned
// by MFC. Document MDI groups are a separate host and do not use this factory.
class CValyriaChannelTabs : public CMFCTabCtrl {
  DECLARE_DYNCREATE(CValyriaChannelTabs)
public:
  int GetTabsHeight() const override {
    const HWND window=GetSafeHwnd();int height=workspace_style::Scale(window,40);
    HDC dc=::GetDC(window);if(dc){const auto old=::SelectObject(dc,workspace_style::UiFont(window));TEXTMETRIC metrics={};if(::GetTextMetrics(dc,&metrics))height=max(height,metrics.tmHeight+workspace_style::Scale(window,16));::SelectObject(dc,old);::ReleaseDC(window,dc);}return height;
  }
  void RecalcLayout() override {
    m_nTabsHeight=GetTabsHeight();
    CMFCTabCtrl::RecalcLayout();
  }
protected:
  LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp) override {
    // Reapply geometry after MFC updates image/system-font measurements.
    const LRESULT result=CMFCTabCtrl::WindowProc(message,wp,lp);
    if(GetSafeHwnd() && (message==WM_THEMECHANGED || message==WM_SETTINGCHANGE || message==0x02e3))RecalcLayout();
    return result;
  }
};
