#pragma once
#include "WorkspaceFrame.h"
class WorkspaceTestFrame:public CWorkspaceFrame
{
protected:
 virtual BOOL OnCreateClient(LPCREATESTRUCT cs,CCreateContext*)
 {if(!CreateClient(cs,NULL))return FALSE;return m_wndClientArea.SubclassWindow(m_hWndMDIClient);}
public:
 using CMDIFrameWnd::DockControlBar;
 using CMDIFrameWnd::ShowControlBar;
 using CMDIFrameWnd::FloatControlBar;
 void EnableDocking(DWORD) {EnableWorkspaceDocking();}
 void DockControlBar(CDockablePane* pane,UINT dock=AFX_IDW_DOCKBAR_RIGHT){DockPane(pane,dock);}
 void ShowControlBar(CDockablePane* pane,BOOL show,BOOL delay){pane->ShowPane(show,delay,show);}
 void FloatControlBar(CDockablePane* pane,CPoint point,DWORD){FloatWorkspacePane(*pane,CRect(point,CSize(500,350)));}
};
