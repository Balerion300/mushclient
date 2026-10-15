#pragma once
#pragma push_macro("Translate")
#undef Translate
#include <afxole.h>
#include <afxmdiframewndex.h>
#include <afxdockingmanager.h>
#include <afxtabbedpane.h>
#include <afxpaneframewnd.h>
#include <functional>
#include <vector>
#pragma pop_macro("Translate")
#include "WorkspaceResources.h"
#include "workspace/WorkspaceFocus.h"
#include "workspace/WorkspaceDockActions.h"

// The enhanced docking manager owns workspace panes. Classic MUSHclient bars
// still use their original docking contexts and reserve their measured area.
class CWorkspaceFrame : public CMDIFrameWndEx
{
  bool m_workspaceLayout;
  WorkspaceFocusState m_workspaceFocus;
  std::function<int(CWnd*)> m_workspaceClose;
  static BOOL CALLBACK CollectWorkspaceChildren(HWND child,LPARAM data)
  {((std::vector<HWND>*)data)->push_back(child);return TRUE;}

protected:
  virtual void AdjustClientArea()
  {
    CMDIFrameWndEx::AdjustClientArea();
    if(m_hWndMDIClient && !AreMDITabs())
    {
      CRect bounds=GetDockingManager()->GetClientAreaBounds();
      ::SetWindowPos(m_hWndMDIClient,NULL,bounds.left,bounds.top,max(1,bounds.Width()),max(1,bounds.Height()),SWP_NOZORDER|SWP_NOACTIVATE);
    }
  }
  virtual BOOL OnCloseDockingPane(CDockablePane* pane)
  {
    CWnd* target=pane;CBaseTabbedPane* group=DYNAMIC_DOWNCAST(CBaseTabbedPane,pane);
    if(group && group->GetUnderlyingWindow())target=group->GetUnderlyingWindow()->GetActiveWnd();
    return !m_workspaceClose || m_workspaceClose(target)==0;
  }
  virtual BOOL OnCloseMiniFrame(CPaneFrameWnd* frame)
  {
    if(!m_workspaceClose)return TRUE;
    std::vector<HWND> children;EnumChildWindows(frame->GetSafeHwnd(),CollectWorkspaceChildren,(LPARAM)&children);
    bool veto=false;for(size_t i=0;i<children.size();++i)if(::IsWindow(children[i])){CWnd* window=CWnd::FromHandlePermanent(children[i]);if(window && m_workspaceClose(window)<0)veto=true;}
    return !veto;
  }
  virtual LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp)
  {
    if(message==AFX_WM_ON_PRESS_CLOSE_BUTTON && m_workspaceClose && m_workspaceClose((CWnd*)lp)!=0)return 1;
    m_workspaceFocus.Before(m_hWnd,message,wp);
    const LRESULT result=CMDIFrameWndEx::WindowProc(message,wp,lp);
    m_workspaceFocus.After(message,wp);
    return result;
  }
  void ReserveLegacyBars()
  {
    if(!GetSafeHwnd() || m_workspaceLayout)return;
    m_workspaceLayout=true;
    CRect remaining;GetClientRect(remaining);
    RepositionBars(0,0xffff,0,reposQuery,&remaining);
    RepositionBars(0,0xffff,0);
    GetDockingManager()->m_rectInPlace=remaining;
    m_workspaceLayout=false;
  }
public:
  CWorkspaceFrame():m_workspaceLayout(false){}
  void SetWorkspaceCloseHandler(const std::function<int(CWnd*)>& close) {m_workspaceClose=close;}
  virtual void AdjustDockingLayout(HDWP positions=NULL)
  { ReserveLegacyBars();CMDIFrameWndEx::AdjustDockingLayout(positions); }
  virtual void RecalcLayout(BOOL notify=TRUE)
  { ReserveLegacyBars();CMDIFrameWndEx::RecalcLayout(notify); }
  void EnableWorkspaceDocking()
  {
    CMDIFrameWnd::EnableDocking(CBRS_ALIGN_ANY);
    CMDIFrameWndEx::EnableDocking(CBRS_ALIGN_ANY);
    CMDIFrameWndEx::EnableAutoHidePanes(CBRS_ALIGN_ANY);
    CDockingManager::SetDockingMode(DT_SMART);
  }
  // FloatPane alone reparents a tabbed leaf without removing its old tab.
  // Use the tab host's native detach path so recent docking information and
  // the original transcript HWND remain intact for caption double-click.
  BOOL FloatWorkspacePane(CDockablePane& pane,const CRect& bounds)
  {
    return FloatWorkspacePaneVisible(pane,bounds);
  }
};
