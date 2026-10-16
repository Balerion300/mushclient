#pragma once
#include "../WorkspaceResources.h"
#include "WorkspaceFocus.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxole.h>
#include <afxdockablepane.h>
#include <afxbasetabbedpane.h>
#include <afxpaneframewnd.h>
#pragma pop_macro("Translate")

inline BOOL FloatWorkspacePaneVisible(CDockablePane& pane,CRect bounds)
{
  const HWND original=pane.GetSafeHwnd();
  CWnd* dock=pane.GetDockSiteFrameWnd();const HWND dockWindow=dock?dock->GetSafeHwnd():NULL;
  if(!::IsWindow(original))return FALSE;
  WorkspaceModernResources resources;
  if(pane.IsAutoHideMode())pane.SetAutoHideMode(FALSE,CBRS_ALIGN_ANY);
  if(auto* group=pane.GetParentTabbedPane())if(!group->DetachPane(&pane,TRUE))return FALSE;
  if(!::IsWindow(original) || CWnd::FromHandlePermanent(original)!=&pane)return FALSE;
  const BOOL floated=pane.FloatPane(bounds,DM_SHOW);
  WorkspaceFocusState::Trace("float-return",original,WM_USER,floated);
  if(!floated || !::IsWindow(original) || CWnd::FromHandlePermanent(original)!=&pane)return FALSE;
  auto* host=pane.GetParentMiniFrame();if(!host || !::IsWindow(host->GetSafeHwnd()))return FALSE;
  // DetachPane(TRUE) has already floated into a hidden host. FloatPane then
  // returns TRUE without moving or showing it. Complete both operations here.
  MONITORINFO monitor={sizeof(monitor)};
  if(GetMonitorInfo(MonitorFromRect(bounds,MONITOR_DEFAULTTONEAREST),&monitor)){
    const CRect work=monitor.rcWork;const int width=max(1,min(bounds.Width(),work.Width())),height=max(1,min(bounds.Height(),work.Height()));
    const int left=max(work.left,min(bounds.left,work.right-width)),top=max(work.top,min(bounds.top,work.bottom-height));
    bounds=CRect(left,top,left+width,top+height);
  }
  host->SetWindowPos(NULL,bounds.left,bounds.top,bounds.Width(),bounds.Height(),SWP_NOZORDER|SWP_NOACTIVATE);
  pane.ShowPane(TRUE,FALSE,FALSE);host->AdjustLayout();host->ShowWindow(SW_SHOWNOACTIVATE);
  if(::IsWindow(dockWindow)){
    if(auto* frame=DYNAMIC_DOWNCAST(CFrameWnd,CWnd::FromHandlePermanent(dockWindow)))frame->RecalcLayout();
    ::RedrawWindow(dockWindow,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN|RDW_UPDATENOW);
  }
  WorkspaceFocusState::Trace("float-visible-host",host->GetSafeHwnd(),WM_USER,pane.IsWindowVisible());
  return host->IsWindowVisible() && pane.IsWindowVisible();
}
