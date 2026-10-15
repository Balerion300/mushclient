// mainfrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "MUSHclient.h"

#include "doc.h"
#include "mainfrm.h"
#include "WorkspaceIntegration.h"
#include "WorkspaceTools.h"
#include "WorkspaceResources.h"
#include "workspace/WorkspaceDockLayout.h"
#include "workspace/WorkspaceShell.h"
#include "workspace/CharacterLabelsDialog.h"
#include "WorkspaceStyle.h"
#include "WorkspaceAppearance.h"
#include "WorkspaceDockGuides.h"
#include "workspace/LogStartSetup.h"
#include "scripting/errors.h"
#include "MUSHview.h"
#include "sendvw.h"

#include "ActivityDoc.h"
#include "ActivityView.h"
#include "childfrm.h"

#include "winplace.h"

#include "dialogs\Splash.h"


#include "dialogs\TipDlg.h"
#include "dialogs\SendToAllDlg.h"
#include "dialogs\UniqueIDDlg.h"
#include "dialogs\WinsockInfoDlg.h"

#include "MakeWindowTransparent.h"

#ifdef _DEBUG
#define new DEBUG_NEW 
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#ifdef _DEBUG
// for documenting menus, accelerators
void ListAccelerators (CDocument * pDoc, const int iType);
#endif 

static TCHAR BASED_CODE szCtrlBars[] = _T("CtrlBars");

// we need two timers, because processing a tick shouldn't reset timer processing
// nor vice-versa

// for normal timers
LARGE_INTEGER hp_timeLastTimerFired;    // if we have high-resolution timer
CTime         timeLastTimerFired;       // otherwise

// for OnPluginTick
LARGE_INTEGER hp_timeLastTickFired;     // if we have high-resolution timer
CTime         timeLastTickFired;        // otherwise                       

int ActivityToolBarResourceNames [6] = {
  IDB_ACTIVITY_TOOLBAR_0,
  IDB_ACTIVITY_TOOLBAR_1,
  IDB_ACTIVITY_TOOLBAR_2,
  IDB_ACTIVITY_TOOLBAR_3,
  IDB_ACTIVITY_TOOLBAR_4,
  IDB_ACTIVITY_TOOLBAR_5,
  };

//////////////////////////////////////////////////////
// CMyToolBar

// A window preview follows the same DPI virtualization as the toolbar frame.
// The traditional MFC outline draws logical coordinates onto a physical
// desktop DC, which places it above/left of the actual drop on scaled displays.
class CToolbarDragPreview : public CWnd
{
public:
  BOOL Create(CWnd* owner)
    {
    return CreateEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
                    WS_EX_TOOLWINDOW,
                    AfxRegisterWndClass(0), "", WS_POPUP,
                    CRect(0, 0, 1, 1), owner, 0);
    }
  void Show(const CRect& rect)
    {
    SetLayeredWindowAttributes(RGB(1, 2, 3), 255, LWA_COLORKEY);
    SetWindowPos(&wndTopMost, rect.left, rect.top, rect.Width(), rect.Height(),
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }
protected:
  afx_msg void OnPaint()
    {
    CPaintDC dc(this);
    CRect rect; GetClientRect(rect);
    dc.FillSolidRect(rect, RGB(1, 2, 3));
    // Leave the center transparent and show the proposed outer frame.
    for (int i = 0; i < 2 && !rect.IsRectEmpty(); ++i)
      {
      dc.Draw3dRect(rect, GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHT));
      rect.DeflateRect(1, 1);
      }
    }
  DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CToolbarDragPreview, CWnd)
  ON_WM_PAINT()
END_MESSAGE_MAP()

class CToolbarDockContext : public CDockContext
{
public:
  explicit CToolbarDockContext(CControlBar* bar) : CDockContext(bar) {}
  virtual void StartDrag(CPoint point)
    {
    // All three application toolbars use dynamic layout. Keep the framework
    // path available for any future fixed-size control bar.
    if (!(m_pBar->m_dwStyle & CBRS_SIZE_DYNAMIC))
      { CDockContext::StartDrag(point); return; }
    if (::GetCapture() != NULL) return;
    CToolbarDragPreview preview;
    if (!preview.Create(m_pDockSite))
      { CDockContext::StartDrag(point); return; }

    m_bDragging = TRUE;
    m_dwDockStyle = m_pBar->m_dwDockStyle;
    m_dwStyle = m_pBar->m_dwStyle & CBRS_ALIGN_ANY;
    m_bFlip = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    m_bForceFrame = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    CRect bar; m_pBar->GetWindowRect(bar);
    m_rectDragHorz = CRect(bar.TopLeft(), m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_HORZDOCK));
    m_rectDragVert = CRect(bar.TopLeft(), m_pBar->CalcDynamicLayout(0, LM_VERTDOCK));
    m_rectFrameDragHorz = CRect(bar.TopLeft(), m_pBar->CalcDynamicLayout(0, LM_HORZ | LM_MRUWIDTH));
    CMiniFrameWnd::CalcBorders(&m_rectFrameDragHorz);
    m_rectFrameDragHorz.InflateRect(-2 * GetSystemMetrics(SM_CXBORDER),
                                   -2 * GetSystemMetrics(SM_CYBORDER));
    m_rectFrameDragVert = m_rectFrameDragHorz;
    KeepPointInside(m_rectDragHorz, point);
    KeepPointInside(m_rectDragVert, point);
    KeepPointInside(m_rectFrameDragHorz, point);
    KeepPointInside(m_rectFrameDragVert, point);
    m_ptLast = point;
    UpdatePreview(preview);
    m_pBar->SetCapture();
    bool dropped = false;
    while (::GetCapture() == m_pBar->m_hWnd)
      {
      MSG msg;
      const BOOL result = ::GetMessage(&msg, NULL, 0, 0);
      if (result <= 0)
        { if (!result) AfxPostQuitMessage((int) msg.wParam); break; }
      if (msg.message == WM_LBUTTONUP) { dropped = true; break; }
      if (msg.message == WM_RBUTTONDOWN ||
          (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)) break;
      if (msg.message == WM_MOUSEMOVE)
        {
        CPoint next(msg.pt);
        const CPoint delta = next - m_ptLast;
        m_rectDragHorz.OffsetRect(delta);
        m_rectDragVert.OffsetRect(delta);
        m_rectFrameDragHorz.OffsetRect(delta);
        m_rectFrameDragVert.OffsetRect(delta);
        m_ptLast = next;
        UpdatePreview(preview);
        }
      else if (msg.message == WM_KEYDOWN || msg.message == WM_KEYUP)
        {
        if (msg.wParam == VK_SHIFT) m_bFlip = msg.message == WM_KEYDOWN;
        if (msg.wParam == VK_CONTROL) m_bForceFrame = msg.message == WM_KEYDOWN;
        UpdatePreview(preview);
        }
      else ::DispatchMessage(&msg);
      }
    preview.DestroyWindow();
    if (::GetCapture() == m_pBar->m_hWnd) ::ReleaseCapture();
    if (!dropped) return;
    if (m_dwOverDockStyle)
      {
      CDockBar* dock = GetDockBar(m_dwOverDockStyle);
      if (!dock) return;
      CRect rect = m_dwOverDockStyle & CBRS_ORIENT_VERT ? m_rectDragVert : m_rectDragHorz;
      m_uMRUDockID = dock->GetDlgCtrlID();
      m_rectMRUDockPos = rect;
      dock->ScreenToClient(&m_rectMRUDockPos);
      m_pDockSite->DockControlBar(m_pBar, dock, &rect);
      m_pDockSite->RecalcLayout();
      }
    else
      {
      const bool horizontal = ((m_dwStyle & CBRS_ORIENT_HORZ) != 0) != (m_bFlip != FALSE);
      m_dwMRUFloatStyle = (horizontal ? CBRS_ALIGN_TOP : CBRS_ALIGN_LEFT) |
                          (m_dwDockStyle & CBRS_FLOAT_MULTI);
      m_ptMRUFloatPos = (horizontal ? m_rectFrameDragHorz : m_rectFrameDragVert).TopLeft();
      m_pDockSite->FloatControlBar(m_pBar, m_ptMRUFloatPos, m_dwMRUFloatStyle);
      }
    }
private:
  static void KeepPointInside(CRect& rect, CPoint point)
    {
    const int dx = point.x < rect.left ? point.x - rect.left :
                   point.x >= rect.right ? point.x - rect.right + 1 : 0;
    const int dy = point.y < rect.top ? point.y - rect.top :
                   point.y >= rect.bottom ? point.y - rect.bottom + 1 : 0;
    rect.OffsetRect(dx, dy);
    }
  void UpdatePreview(CToolbarDragPreview& preview)
    {
    m_dwOverDockStyle = m_bForceFrame ? 0 : CanDock();
    const bool horizontal = ((m_dwStyle & CBRS_ORIENT_HORZ) != 0) != (m_bFlip != FALSE);
    const CRect rect = m_dwOverDockStyle ?
      (m_dwOverDockStyle & CBRS_ORIENT_VERT ? m_rectDragVert : m_rectDragHorz) :
      (horizontal ? m_rectFrameDragHorz : m_rectFrameDragVert);
    preview.Show(rect);
    }
};

void CMyToolBar::EnableDocking(DWORD style)
{
  // Install before LoadBarState restores the saved MRU docking positions.
  if (!m_pDockContext) m_pDockContext = new CToolbarDockContext(this);
  CToolBar::EnableDocking(style);
}

BEGIN_MESSAGE_MAP(CMyToolBar, CToolBar)
	//{{AFX_MSG_MAP(CMyToolBar)
	ON_WM_NCPAINT()
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CMyToolBar::OnNcPaint() 
{
	EraseNonClient();
}
void CMyToolBar::EraseNonClient()
{

	// Get window DC that is clipped to the non-client area.
	CWindowDC dc(this);
	CRect rectClient;
	GetClientRect(rectClient);
	CRect rectWindow;
	GetWindowRect(rectWindow);
	ScreenToClient(rectWindow);
	rectClient.OffsetRect(-rectWindow.left, -rectWindow.top);
	dc.ExcludeClipRect(rectClient);

	// Draw the borders in the non-client area.
	rectWindow.OffsetRect(-rectWindow.left, -rectWindow.top);
	DrawBorders(&dc, rectWindow);

	// Erase the parts that are not drawn.
	dc.IntersectClipRect(rectWindow);
	SendMessage(WM_ERASEBKGND, (WPARAM)dc.m_hDC);

	// Draw the gripper in the non-client area.
	DrawGripper(&dc, rectWindow);
}

void CMyToolBar::DoPaint(CDC* pDC)
{
	ASSERT_VALID(this);
	ASSERT_VALID(pDC);

	// Paint inside the client area.
	CRect rect;
	GetClientRect(rect);
	DrawBorders(pDC, rect);
	DrawGripper(pDC, rect);
}

void CMyToolBar::DrawGripper(CDC* pDC, const CRect& rect)
{
	pDC->FillSolidRect( &rect, ::GetSysColor(COLOR_BTNFACE)); // Fill in the background.
	CToolBar::DrawGripper(pDC,rect);
}
/////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CMainFrame

namespace {
LRESULT CALLBACK WorkspaceBackgroundHook(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data)
{
  CMainFrame* frame=(CMainFrame*)data;
  if(message==WM_ERASEBKGND && frame->m_backgroundColour!=0xffffffff)
    {RECT area;GetClientRect(window,&area);HBRUSH brush=CreateSolidBrush(frame->m_backgroundColour);FillRect((HDC)wp,&area,brush);DeleteObject(brush);return TRUE;}
  if(message==WM_NCDESTROY)RemoveWindowSubclass(window,WorkspaceBackgroundHook,id);
  return DefSubclassProc(window,message,wp,lp);
}
}

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWndEx)
  ON_MESSAGE(WM_APP+0x3a0,OnInitializeWorkspaceShell)
  ON_COMMAND_RANGE(59030,59038,OnWorkspaceShellCommand)
  ON_UPDATE_COMMAND_UI_RANGE(59030,59038,OnUpdateWorkspaceShellCommand)
  ON_COMMAND(ID_WORKSPACE_TRANSCRIPT, OnWorkspaceTranscript)
  ON_COMMAND(59005, OnWorkspaceFloatTranscript)
  ON_COMMAND(59006, OnWorkspaceRoute)
  ON_COMMAND(59007, OnWorkspaceAgentReview)
  ON_COMMAND(59008, OnWorkspaceLogStartSetup)
  ON_UPDATE_COMMAND_UI(59008, OnUpdateWorkspaceLogStartSetup)
  ON_COMMAND_RANGE(59010,59018,OnWorkspaceFocus)
  ON_UPDATE_COMMAND_UI_RANGE(59010,59018,OnUpdateWorkspaceFocus)
  ON_COMMAND_RANGE(59001, 59004, OnWorkspaceTool)
	ON_WM_CONTEXTMENU()
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_COMMAND(ID_DISPLAY_ACTIVITYLIST, OnDisplayActivitylist)
	ON_COMMAND_EX(ID_WORLDS_WORLD0, OnWorldSwitch)
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_UPDATE_COMMAND_UI(ID_STATUSLINE_FREEZE, OnUpdateStatuslineFreeze)
	ON_UPDATE_COMMAND_UI(ID_STATUSLINE_MUSHNAME, OnUpdateStatuslineMushname)
	ON_UPDATE_COMMAND_UI(ID_STATUSLINE_LINES, OnUpdateStatuslineLines)
	ON_UPDATE_COMMAND_UI(ID_STATUSLINE_LOG, OnUpdateStatuslineLog)
	ON_UPDATE_COMMAND_UI(ID_STATUSLINE_TIME, OnUpdateStatuslineTime)
	ON_COMMAND(ID_HELP_TIPOFTHEDAY, OnHelpTipoftheday)
	ON_UPDATE_COMMAND_UI(ID_DISPLAY_ACTIVITYLIST, OnUpdateDisplayActivitylist)
	ON_COMMAND(ID_FILE_PREFERENCES, OnFilePreferences)
	ON_WM_DRAWITEM()
	ON_COMMAND(ID_CONNECTION_AUTOCONNECT, OnConnectionAutoconnect)
	ON_UPDATE_COMMAND_UI(ID_CONNECTION_AUTOCONNECT, OnUpdateConnectionAutoconnect)
	ON_COMMAND(ID_CONNECTION_RECONNECTONDISCONNECT, OnConnectionReconnectondisconnect)
	ON_UPDATE_COMMAND_UI(ID_CONNECTION_RECONNECTONDISCONNECT, OnUpdateConnectionReconnectondisconnect)
	ON_COMMAND(ID_HELP_CONTENTS, OnHelpContents)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD0, OnUpdateBtnWorldsWorld0)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD1, OnUpdateBtnWorldsWorld1)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD2, OnUpdateBtnWorldsWorld2)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD3, OnUpdateBtnWorldsWorld3)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD4, OnUpdateBtnWorldsWorld4)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD5, OnUpdateBtnWorldsWorld5)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD6, OnUpdateBtnWorldsWorld6)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD7, OnUpdateBtnWorldsWorld7)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD8, OnUpdateBtnWorldsWorld8)
	ON_UPDATE_COMMAND_UI(ID_BTN_WORLDS_WORLD9, OnUpdateBtnWorldsWorld9)
	ON_COMMAND(ID_WEB_PAGE, OnWebPage)
	ON_COMMAND(ID_VIEW_RESET_TOOLBARS, OnViewResetToolbars)
	ON_COMMAND(ID_HELP_MUDLISTS, OnHelpMudlists)
	ON_WM_SIZE()
	ON_WM_ACTIVATE()
	ON_COMMAND(ID_DISPLAY_STOPSOUNDPLAYING, OnDisplayStopsoundplaying)
	ON_UPDATE_COMMAND_UI(ID_DISPLAY_STOPSOUNDPLAYING, OnUpdateDisplayStopsoundplaying)
	ON_COMMAND(ID_HELP_BUGREPORTSUGGESTION, OnHelpBugreportsuggestion)
	ON_COMMAND(ID_VIEW_ALWAYSONTOP, OnViewAlwaysontop)
	ON_UPDATE_COMMAND_UI(ID_VIEW_ALWAYSONTOP, OnUpdateViewAlwaysontop)
	ON_COMMAND(ID_GAME_SENDTOALLWORLDS, OnGameSendtoallworlds)
	ON_COMMAND(ID_EDIT_RELOADNAMESFILE, OnEditReloadnamesfile)
	ON_COMMAND(ID_VIEW_FULLSCREENMODE, OnViewFullscreenmode)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FULLSCREENMODE, OnUpdateViewFullscreenmode)
	ON_COMMAND(ID_EDIT_NOTESWORKAREA, OnEditNotesworkarea)
	ON_COMMAND(ID_WINDOW_CLOSEALLNOTEPADWINDOWS, OnWindowCloseallnotepadwindows)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_OPENWORLDSINSTARTUPLIST, OnFileOpenworldsinstartuplist)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPENWORLDSINSTARTUPLIST, OnUpdateFileOpenworldsinstartuplist)
	ON_COMMAND(ID_FILE_WINSOCK, OnFileWinsock)
	ON_COMMAND(ID_CONNECTION_CONNECTTOALLOPENWORLDS, OnConnectionConnecttoallopenworlds)
	ON_COMMAND(ID_CONNECTION_CONNECTTOWORLDSINSTARTUPLIST, OnConnectionConnecttoworldsinstartuplist)
	ON_UPDATE_COMMAND_UI(ID_CONNECTION_CONNECTTOWORLDSINSTARTUPLIST, OnUpdateConnectionConnecttoworldsinstartuplist)
	ON_COMMAND(ID_HELP_FORUM, OnHelpForum)
	ON_COMMAND(ID_HELP_FUNCTIONSWEBPAGE, OnHelpFunctionswebpage)
	ON_COMMAND(ID_HELP_DOCUMENTATIONWEBPAGE, OnHelpDocumentationwebpage)
	ON_COMMAND(ID_HELP_REGULAREXPRESSIONSWEBPAGE, OnHelpRegularexpressionswebpage)
	ON_COMMAND(ID_HELP_PLUGINSLIST, OnHelpPluginslist)
	ON_COMMAND(ID_EDIT_GENERATEUNIQUEID, OnEditGenerateuniqueid)
	ON_COMMAND(ID_GAME_FUNCTIONSLIST, OnGameFunctionslist)
	ON_COMMAND(ID_EDIT_CONVERTCLIPBOARDFORUMCODES, OnEditConvertclipboardforumcodes)
	ON_COMMAND_EX(ID_WORLDS_WORLD1, OnWorldSwitch)
	ON_COMMAND_EX(ID_WORLDS_WORLD2, OnWorldSwitch)
	ON_COMMAND_EX(ID_WORLDS_WORLD3, OnWorldSwitch)
	ON_COMMAND_EX(ID_WORLDS_WORLD4, OnWorldSwitch)
	ON_COMMAND_EX(ID_WORLDS_WORLD5, OnWorldSwitch)
	ON_COMMAND_EX(ID_WORLDS_WORLD6, OnWorldSwitch)
	ON_COMMAND_EX(ID_WORLDS_WORLD7, OnWorldSwitch)
	ON_COMMAND_EX(ID_WORLDS_WORLD8, OnWorldSwitch)
	ON_COMMAND_EX(ID_WORLDS_WORLD9, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD0, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD1, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD2, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD3, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD4, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD5, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD6, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD7, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD8, OnWorldSwitch)
	ON_COMMAND_EX(ID_BTN_WORLDS_WORLD9, OnWorldSwitch)
	ON_WM_SYSCOMMAND()
	//}}AFX_MSG_MAP
  ON_MESSAGE(MM_MCINOTIFY, OnMCINotify)
  ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR, CMDIFrameWnd::OnUpdateControlBarMenu)
  ON_UPDATE_COMMAND_UI(ID_VIEW_STATUS_BAR, CMDIFrameWnd::OnUpdateControlBarMenu)
  ON_UPDATE_COMMAND_UI(ID_VIEW_GAME_TOOLBAR, CMDIFrameWnd::OnUpdateControlBarMenu)
  ON_UPDATE_COMMAND_UI(ID_VIEW_ACTIVITYTOOLBAR, CMDIFrameWnd::OnUpdateControlBarMenu)
  ON_UPDATE_COMMAND_UI(ID_VIEW_INFOBAR, OnUpdateInfoBar)
	ON_COMMAND_EX(ID_VIEW_GAME_TOOLBAR, CMDIFrameWnd::OnBarCheck)
	ON_COMMAND_EX(ID_VIEW_ACTIVITYTOOLBAR, CMDIFrameWnd::OnBarCheck)
	ON_COMMAND_EX(ID_VIEW_INFOBAR, CMDIFrameWnd::OnBarCheck)
		// Global help commands
	ON_COMMAND(ID_HELP_INDEX, CMDIFrameWndEx::OnHelpFinder)
	ON_COMMAND(ID_HELP_USING, CMDIFrameWndEx::OnHelpUsing)
//	ON_COMMAND(ID_HELP, CMDIFrameWndEx::OnHelp)
	ON_COMMAND(ID_HELP, OnF1Test)
	ON_COMMAND(ID_CONTEXT_HELP, CMDIFrameWndEx::OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, CMDIFrameWndEx::OnHelpIndex)
  // tooltip stuff
  ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTW, 0, 0xFFFF, OnDynamicTipText)
  ON_NOTIFY_EX_RANGE(TTN_NEEDTEXTA, 0, 0xFFFF, OnDynamicTipText) 

  // MXP pop-up menu
  ON_COMMAND_RANGE(MXP_FIRST_MENU, TRAY_FIRST_MENU + TRAY_MENU_COUNT - 1, OnTrayMenu)

  // fix up menus
	ON_UPDATE_COMMAND_UI_RANGE(0, 0xFFFF, OnFixMenus)

END_MESSAGE_MAP()

static UINT BASED_CODE indicators[] =
{
	ID_SEPARATOR,           // status line indicator
  ID_STATUSLINE_FREEZE,
  ID_STATUSLINE_MUSHNAME,
  ID_STATUSLINE_TIME,
  ID_STATUSLINE_LINES,
  ID_STATUSLINE_LOG,
	ID_INDICATOR_CAPS,
};

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
m_workspaceDockSession=new workspace_dock::Session;
m_workspaceShell=new CWorkspaceShell;
m_workspaceShellReady=false;m_workspaceLegacyMask=7;m_workspaceTabbedWorlds=true;

m_timer = 0;
m_ticktimer = 0;
m_workspaceRenderBatchDepth = 0;
m_bActive = false;
m_wDeviceID = 0;    // no MCI device yet
m_bFullScreen = false;	
m_bFlashingWindow = false;
m_iTabsCount = 0;
m_backgroundColour = 0xFFFFFFFF;  // use default background colour

ZeroMemory(&m_niData,sizeof(NOTIFYICONDATA));
m_niData.cbSize = sizeof(NOTIFYICONDATA);
	
}


CMainFrame::~CMainFrame()
{
  delete m_workspaceShell;delete m_workspaceDockSession;
  for (size_t i=0;i<m_workspaceSplits.size();++i) delete m_workspaceSplits[i];
  for (size_t i=0;i<m_workspaceTools.size();++i) delete m_workspaceTools[i];
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  workspace_appearance::Install();

  int iToolbarFlags = 0;

  if (App.m_bFlatToolbars)
    iToolbarFlags = TBSTYLE_FLAT;

  int workspaceFrameCreated;
  { WorkspaceModernResources resources;workspaceFrameCreated=CMDIFrameWndEx::OnCreate(lpCreateStruct); }
  if (workspaceFrameCreated == -1)
    {
    ::TMessageBox ("Failed to create MDI Frame Window", MB_ICONSTOP);
		return -1;
    }

  SetWindowSubclass(m_hWndMDIClient,WorkspaceBackgroundHook,1,(DWORD_PTR)this);
  SetWorkspaceCloseHandler([this](CWnd* window){CWorkspacePane* pane=DYNAMIC_DOWNCAST(CWorkspacePane,window);if(!pane || !pane->IsToolHost())return 0;SaveWorkspaceLayouts();return pane->CloseHostedTool();});

	if (!m_wndToolBar.CreateEx(this, iToolbarFlags, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
    ::TMessageBox ("Failed to create toolbar", MB_ICONSTOP);
		return -1;      // fail to create
	}


	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
    ::TMessageBox ("Failed to create status bar", MB_ICONSTOP);
		return -1;      // fail to create
	}

	if (!m_wndGameToolBar.CreateEx(this, iToolbarFlags, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, 
    CRect(0, 0, 0, 0), ID_VIEW_GAME_TOOLBAR) ||
		!m_wndGameToolBar.LoadToolBar(IDR_GAME_TOOLBAR))
	{
		TRACE0("Failed to create game toolbar\n");
    ::TMessageBox ("Failed to create game toolbar", MB_ICONSTOP);
		return -1;      // fail to create
	}

	if (!m_wndActivityToolBar.CreateEx(this, iToolbarFlags, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, 
    CRect(0, 0, 0, 0), ID_VIEW_ACTIVITYTOOLBAR) ||
		!m_wndActivityToolBar.LoadToolBar(IDR_ACTIVITY_TOOLBAR))
	{
		TRACE0("Failed to create activity toolbar\n");
    ::TMessageBox ("Failed to create activity toolbar", MB_ICONSTOP);
		return -1;      // fail to create
	}

  m_wndInfoBar.m_hWnd = NULL;

  // new in 3.29 - Info Bar
  if (!m_wndInfoBar.Create (this, IDD_INFO, CBRS_BOTTOM | CBRS_SIZE_DYNAMIC, ID_VIEW_INFOBAR))
	{
		//TRACE0("Failed to create info bar\n");
    //::TMessageBox ("Failed to create info bar", MB_ICONSTOP);
    m_wndInfoBar.m_hWnd = NULL;   // we will use this to flag that it wasn't created
	}
  
  if (m_wndInfoBar.m_hWnd)
    {
    CRichEditCtrl * pRichEdit = (CRichEditCtrl *) m_wndInfoBar.GetDlgItem (IDC_INFOTEXT);
    pRichEdit->SetWindowPos (NULL, 0, 0, 3000, 20, 
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // remember default format

    memset (&m_defaultInfoBarFormat, 0, sizeof (m_defaultInfoBarFormat));
    m_defaultInfoBarFormat.cbSize = sizeof (m_defaultInfoBarFormat);
    m_defaultInfoBarFormat.dwMask = CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_ITALIC | CFM_STRIKEOUT | CFM_UNDERLINE;
    pRichEdit->GetDefaultCharFormat (m_defaultInfoBarFormat);

    // grey background
    pRichEdit->SetBackgroundColor (FALSE, RGB(192, 192, 192));

    // select all
    pRichEdit->SetSel (0, -1);

    // delete everything
    pRichEdit->ReplaceSel ("");
    }


  m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle()|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC);
	m_wndToolBar.SetWindowText(CString(MAKEINTRESOURCE(IDS_TOOLBAR)));

	m_wndGameToolBar.SetBarStyle(m_wndGameToolBar.GetBarStyle()|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC);
	m_wndGameToolBar.SetWindowText(CString(MAKEINTRESOURCE(IDS_GAME_TOOLBAR)));

	m_wndActivityToolBar.SetBarStyle(m_wndActivityToolBar.GetBarStyle()|CBRS_TOOLTIPS|CBRS_FLYBY|CBRS_SIZE_DYNAMIC);
	m_wndActivityToolBar.SetWindowText(CString(MAKEINTRESOURCE(IDS_ACTIVITY_TOOLBAR)));

  // load appropriate bitmap for activity buttons
  if (App.m_iActivityButtonBarStyle < 0 || App.m_iActivityButtonBarStyle >= NUMITEMS (ActivityToolBarResourceNames))
    App.m_iActivityButtonBarStyle = 0;
  
  m_wndActivityToolBar.LoadBitmap (ActivityToolBarResourceNames [App.m_iActivityButtonBarStyle]);
  m_wndActivityToolBar.Invalidate (); // redraw it

  m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	m_wndGameToolBar.EnableDocking(CBRS_ALIGN_ANY);
	m_wndActivityToolBar.EnableDocking(CBRS_ALIGN_ANY);

  EnableWorkspaceDocking();
  // Create the native drop guides before a mouse drag enters its modal loop.
  InstallWorkspaceDockGuides(*this,*GetDockingManager());
  
	DockControlBar(&m_wndToolBar);
	DockControlBar(&m_wndGameToolBar);
	DockControlBar(&m_wndActivityToolBar);

  if (m_wndInfoBar.m_hWnd)
    {
  	m_wndInfoBar.EnableDocking(CBRS_ALIGN_ANY);
  	DockControlBar(&m_wndInfoBar);
    m_wndInfoBar.SetWindowText ("Info");
    }


  if (!m_workspacePane.Create(this)) return -1;
  m_workspacePane.EnableDocking(CBRS_ALIGN_ANY);
  m_workspacePane.SetActionHandler(ActivateWorkspaceAction);
  m_workspacePane.SetInputHandler(WorkspaceSendHumanInput);
  m_workspacePane.SetSharedInputHandler(WorkspaceSelectSharedInput);
  m_workspacePane.SetPruneHandler(WorkspacePanePruned);
  m_workspacePane.SetConnectionHandler(WorkspaceIsConnectionLive);m_workspacePane.SetInputErrorHandler(WorkspaceHumanInputError);
  DockControlBar(&m_workspacePane, AFX_IDW_DOCKBAR_RIGHT);
  ShowControlBar(&m_workspacePane, FALSE, FALSE);

  // set timer for activity window updates, and world timers

  if (App.m_nTimerInterval > 0)
    m_timer = SetTimer(ACTIVITY_TIMER_ID, App.m_nTimerInterval * 1000, NULL );
  else
    m_timer = SetTimer(ACTIVITY_TIMER_ID, MIN_TIMER_INTERVAL, NULL );

  // set tick timer
  m_ticktimer = SetTimer(TICK_TIMER_ID, 40, NULL );       // 25 ticks a second

	// CG: The following line was added by the Splash Screen component.
	CSplashWnd::ShowSplashScreen(this, IDB_SPLASH);

  FixUpTitleBar ();

  m_nPauseItem =  m_wndStatusBar.CommandToIndex (ID_STATUSLINE_FREEZE);
  workspace_appearance::StyleStatusBar(m_wndStatusBar.GetSafeHwnd());
  // Locale DLLs can still carry historical MUSHclient icons. Load the approved
  // application identity explicitly from this executable, not Afx's locale handle.
  SetIcon((HICON)::LoadImage(::GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MAINFRAME),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED),TRUE);
  SetIcon((HICON)::LoadImage(::GetModuleHandle(NULL),MAKEINTRESOURCE(IDR_MAINFRAME),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED),FALSE);

  // Tabbed Windows - if wanted

  bool tabbedWorlds=false; // Tree-first workspace; keep an explicit saved tab preference.
  if(App.db && sqlite3_exec(App.db,"CREATE TABLE IF NOT EXISTS workspace_window_mode_v1(slot INTEGER PRIMARY KEY CHECK(slot=1),tabbed INTEGER NOT NULL)",NULL,NULL,NULL)==SQLITE_OK){sqlite3_stmt* query=NULL;if(sqlite3_prepare_v2(App.db,"SELECT tabbed FROM workspace_window_mode_v1 WHERE slot=1",-1,&query,NULL)==SQLITE_OK){if(sqlite3_step(query)==SQLITE_ROW)tabbedWorlds=sqlite3_column_int(query,0)!=0;sqlite3_finalize(query);}}
  SetWorkspaceTabbedWorlds(tabbedWorlds);

  // initialize timer fallback
  if (App.m_iCounterFrequency)
    {
    QueryPerformanceCounter (&hp_timeLastTimerFired);
    QueryPerformanceCounter (&hp_timeLastTickFired);
    }
  else
    {
    timeLastTimerFired = CTime::GetCurrentTime();
    timeLastTickFired  = CTime::GetCurrentTime();
    }

  EnsureWorkspaceMenu();
  {WorkspaceModernResources resources;if(!m_workspaceShell->Create(*this,[this](workspace_shell::Action action,const std::string& world,const std::string& name){WorkspaceShellAction((int)action,world,name);} ))return -1;}
  workspace_style::ApplySurface(GetSafeHwnd());
  workspace_appearance::Refresh(GetSafeHwnd());
  PostMessage(WM_APP+0x3a0);
	return 0;
}  // CMainFrame::OnCreate

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMDIFrameWndEx::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMDIFrameWndEx::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	cs.style &= ~FWS_ADDTOTITLE;
//  cs.dwExStyle |= WS_EX_TOOLWINDOW;
		
	return CMDIFrameWndEx::PreCreateWindow(cs);
}


// For setting the text on the main (left-hand) status bar

void CMainFrame::SetStatusMessage(const CString& msg)
{
  if (App.m_pMainWnd)
    m_wndStatusBar.SetWindowText(msg);
}   // end of CWinFormsFrame::SetStatusMessage

void CMainFrame::SetStatusMessageNow(const CString& msg)
{
  if (App.m_pMainWnd)
    {
    m_wndStatusBar.SetWindowText(msg);
    m_wndStatusBar.UpdateWindow ();   // draw now

    }
}   // end of CWinFormsFrame::SetStatusMessageNow

bool bShowDebugStatus = false;

void CMainFrame::DelayDebugStatus(const CString& msg)
{
  if (bShowDebugStatus)
     SetStatusMessageNow (msg);
}   // end of CWinFormsFrame::DelayDebugStatus

void CMainFrame::SetStatusNormal (void)
{
  SetStatusMessageNow (Translate ("Ready"));
}   // end of CWinFormsFrame::SetStatusNormal

// For returning the size of the status bar. Used in calculating the centre of the window

void CMainFrame::ReturnStatusRect (CRect & rect)
    {
    if (m_wndStatusBar.GetStyle() & WS_VISIBLE)
        m_wndStatusBar.GetWindowRect (rect);
    else
        rect = CRect (0, 0, 0, 0);
    }  // end of CWinFormsFrame::ReturnStatusRect 

// For returning the size of the tool bar. Used in calculating the centre of the window

void CMainFrame::ReturnToolbarRect(CRect & rect)
    {
    if (m_wndToolBar.GetStyle() & WS_VISIBLE)
        m_wndToolBar.GetWindowRect (rect);
    else
        rect = CRect (0, 0, 0, 0);
    }  // end of CWinFormsFrame::ReturnToolbarRect 


void CMainFrame::OnClose() 
{

  CancelSound ();
  // close any outstanding MCI devices
  mciSendCommand(MCI_ALL_DEVICE_ID, MCI_CLOSE, MCI_WAIT, NULL);

  SaveWorkspaceLayouts();
  SaveWorkspaceShell();
  SaveBarState(szCtrlBars);

   // put window back where it was if necessary
  if (m_bFullScreen)
    {
    OnViewFullscreenmode   ();
    }

  CWindowPlacement wp;
  wp.Save ("Main window", this);
    	
	CMDIFrameWndEx::OnClose();
}

void CMainFrame::OnDisplayActivitylist() 
{

  if (App.m_pActivityView)
    {
    if (App.m_pActivityView->GetParent ()->IsIconic ())
      App.m_pActivityView->GetParent ()->ShowWindow (SW_RESTORE);

    App.m_pActivityView->GetParentFrame ()->ActivateFrame ();
    }
  else
	  App.m_pActivityDocTemplate->OpenDocumentFile(NULL);
	
}


CMUSHclientDoc * FindWorld (const UINT nIDC, int & iWorld)
  {

  	switch (nIDC)
	    {
      case ID_BTN_WORLDS_WORLD0:
      case ID_WORLDS_WORLD0: iWorld = 10; break;
      case ID_BTN_WORLDS_WORLD1:
      case ID_WORLDS_WORLD1: iWorld = 1; break;
      case ID_BTN_WORLDS_WORLD2:
      case ID_WORLDS_WORLD2: iWorld = 2; break;
      case ID_BTN_WORLDS_WORLD3:
      case ID_WORLDS_WORLD3: iWorld = 3; break;
      case ID_BTN_WORLDS_WORLD4:
      case ID_WORLDS_WORLD4: iWorld = 4; break;
      case ID_BTN_WORLDS_WORLD5:
      case ID_WORLDS_WORLD5: iWorld = 5; break;
      case ID_BTN_WORLDS_WORLD6:
      case ID_WORLDS_WORLD6: iWorld = 6; break;
      case ID_BTN_WORLDS_WORLD7:
      case ID_WORLDS_WORLD7: iWorld = 7; break;
      case ID_BTN_WORLDS_WORLD8:
      case ID_WORLDS_WORLD8: iWorld = 8; break;
      case ID_BTN_WORLDS_WORLD9:
      case ID_WORLDS_WORLD9: iWorld = 9; break;
      default: iWorld = 0; break;
      } // end of switch


 	POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();
  CMUSHclientDoc* pDoc = NULL;
  int nItem = 1;

	while (pos)
	{
     pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);

    // number the worlds in case there isn't an activity window
    if (0 == pDoc->m_view_number)
      pDoc->m_view_number = nItem;

    if (pDoc->m_view_number == iWorld)
      break;
    else
      pDoc = NULL;

   nItem++;
   }

  return pDoc;

  }

BOOL CMainFrame::OnWorldSwitch (UINT nIDC) 
  {

  int iWorld; 

  CMUSHclientDoc * pDoc= FindWorld (nIDC, iWorld);

  if (!pDoc)   // didn't find 
    return TRUE;

  for(POSITION pos=pDoc->GetFirstViewPosition();pos!=NULL;)
    {
    CView* pView = pDoc->GetNextView(pos);

    if (pView->IsKindOf(RUNTIME_CLASS(CSendView)))
      {
      CSendView* pmyView = (CSendView*)pView;

      if (pmyView->GetParentFrame ()->IsIconic ())
        pmyView->GetParentFrame ()->ShowWindow (SW_RESTORE);

      pmyView->GetParentFrame ()->ActivateFrame ();
      pmyView->m_owner_frame->SetActiveView(pmyView);

      break;

      }	
    }

  return TRUE;
  }   // end of CMainFrame::OnWorldSwitch

void CMainFrame::OnUpdateBtnWorlds (int iWorld, CCmdUI* pCmdUI) 
  {

 	POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();
  CMUSHclientDoc* pDoc = NULL;
  int nItem = 1;

	while (pos)
	{
     pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);

    // number the worlds in case there isn't an activity window
    if (0 == pDoc->m_view_number)
      pDoc->m_view_number = nItem;

    if (pDoc->m_view_number == iWorld)
      break;
    else
      pDoc = NULL;

    nItem++;
   }

  UINT nID,
       nStyle;
  int  iImage,
       iNewImage;

  m_wndActivityToolBar.GetButtonInfo  (iWorld - 1, nID, nStyle, iImage);

  // check the button if the world is active - make sure we can see button
  if (pDoc)
    {
    if (pCmdUI)
      {
      pCmdUI->Enable (TRUE);
      pCmdUI->SetCheck (pDoc->m_pActiveCommandView != NULL ||
                        pDoc->m_pActiveOutputView != NULL);
      }

    // greyed out = no world
    // black = normal (open but no new messages)  0 -  9
    // red = new messages                        10 - 19
    // green = world closed                      20 - 29

    iNewImage = pDoc->m_new_lines ? iWorld + 9 : iWorld - 1;
    if (pDoc->m_iConnectPhase != eConnectConnectedToMud || !pDoc->m_pSocket)
       iNewImage = iWorld + 19;

    if (iNewImage != iImage)
      m_wndActivityToolBar.SetButtonInfo  (iWorld - 1, nID, nStyle, iNewImage);
    }
  else
    // hide the button if the world does not exist
    {
    if (pCmdUI)
      {
      pCmdUI->Enable (FALSE);
      pCmdUI->SetCheck (FALSE);
      }
    }

  }  // end of CMainFrame::OnUpdateBtnWorlds

void CMainFrame::OnDestroy() 
{

  if (m_timer)
      KillTimer (m_timer);

  if (m_ticktimer)
      KillTimer (m_ticktimer);

  ShutdownWorkspaceTools();
  CMDIFrameWndEx::OnDestroy();
	
  if (m_niData.uID)
    Shell_NotifyIcon (NIM_DELETE ,&m_niData);
	
}  // end of CMainFrame::OnDestroy

void CMainFrame::ProcessTimers (void)
  {

  // so we know when we prccessed this one
  if (App.m_iCounterFrequency)
    QueryPerformanceCounter (&hp_timeLastTimerFired);
  else
    timeLastTimerFired = CTime::GetCurrentTime();

// update activity window anyway every required inverval, to update durations
// provided activity update type is not "on activity" only
                                                          
  if (App.m_pActivityDoc && 
      App.m_nActivityWindowRefreshType != App.eRefreshOnActivity)
    {
    CTime tNow = CTime::GetCurrentTime();
    CTimeSpan tsActivityIntervalSeconds (0, 0, 0, App.m_nActivityWindowRefreshInterval);
    CTime tNextUpdate = App.m_timeLastActivityUpdate + tsActivityIntervalSeconds;

    if (tNextUpdate < tNow)
      App.m_bUpdateActivity = TRUE;

  // update activity window if required (every interval required by the user)

    if (App.m_bUpdateActivity)
      App.m_pActivityDoc->UpdateAllViews (NULL);
    }

//  TRACE ("Timer fired.\n");

// tell each document to process its timer events

  bool bNewActivity = false,
       bFlashIcon = false;

	for (POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition(); pos;)
    {
    CMUSHclientDoc * pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);
    if (pDoc->m_new_lines)
      {
      bNewActivity = true;
      if (pDoc->m_bFlashIcon)
       bFlashIcon = true;    // see if new activity
      }
    pDoc->CheckTimers ();
    }

  // as CTime has a 1-second granularity, we will see if the time has changed
  // in order to test for when a second has elapsed.

  CTime tNow = CTime::GetCurrentTime();

  // if we have new activity, and we are minimised, flash the icon
  if (bFlashIcon && !m_bActive)
    {
    /*    Another day, maybe - this requires WINVER == 0x0500 ie. Windows 2000
    if (bWin95)
      FlashWindow (TRUE);
    else
      {
      FLASHWINFO fw;
      fw.cbSize = sizeof fw;
      fw.hwnd = m_hWnd;
      fw.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
      fw.uCount = 0;
      fw.dwTimeout = 0;
      FlashWindowEx (&fw);
      }
    */

    // little test to stop flashing it 10 times a second

    if (tNow != m_timeFlashed)
      {
      FlashWindow (TRUE);
      m_bFlashingWindow = true; // remember we did it
      }

    }

  // every 5 seconds, update the tabs in case text windows change, etc.
  // or update every second if we know we have new activity

  if (tNow != m_timeFlashed)
    {
    if (m_iTabsCount++ >= 5 || bNewActivity)
      {
      m_iTabsCount = 0;
      if (m_wndMDITabs.InUse ())
        m_wndMDITabs.Update ();
      }
    }

  m_timeFlashed = tNow;
  UpdateWorkspaceShell();


  }  // end of CMainFrame::ProcessTimers

// timers are pretty unreliable, so we'll just use them as a hint now to check the timer times
void CMainFrame::OnTimer(UINT nIDEvent) 
  {

  CheckTimerFallback ();
	CMDIFrameWndEx::OnTimer(nIDEvent);
  }  // end of CMainFrame::OnTimer

void CMainFrame::OnUpdateStatuslineFreeze(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  pCmdUI->SetText (NULL);
}


void CMainFrame::OnUpdateStatuslineMushname(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  pCmdUI->SetText ("(No world active)");
}

void CMainFrame::OnUpdateStatuslineLines(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  pCmdUI->SetText (NULL);
}

void CMainFrame::OnUpdateStatuslineLog(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  pCmdUI->SetText (NULL);
}

void CMainFrame::OnUpdateStatuslineTime(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  pCmdUI->SetText (NULL);
}


void CMainFrame::OnHelpTipoftheday() 
{
	CTipDlg dlg;

  dlg.DoModal ();
	
}


void CMainFrame::FixUpTitleBar (void)
  {

//  if (AfxGetMainWnd () && AfxGetMainWnd ()->IsIconic ())
//    return; 
  
  CString strTitle;
  CString strName;
  CString strMainTitle;

  int nDocCount = 0;

 	POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();

	while (pos)
	{
    CMUSHclientDoc* pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);

    // find the active world
    if (pDoc->m_pActiveCommandView != NULL || pDoc->m_pActiveOutputView != NULL)
      {
      WINDOWPLACEMENT wp;
      wp.length = sizeof (wp);
      if (pDoc->m_pActiveCommandView)
        pDoc->m_pActiveCommandView->GetOwner ()->GetOwner ()->GetWindowPlacement (&wp);
      else
        pDoc->m_pActiveOutputView->GetOwner ()->GetOwner ()->GetWindowPlacement (&wp);

      strName = pDoc->m_mush_name;    // remember world name
      strMainTitle = pDoc->m_strMainWindowTitle;

      // don't show name again if maximized
      if (wp.showCmd == SW_MAXIMIZE)
        strName.Empty ();
      }

    nDocCount++;
   }

  if (m_bActive)
    {

    strTitle = "MUSHclient";

    if (!strName.IsEmpty ())
      {
      strTitle += " - [";
      strTitle += strName;
      strTitle += "]";
      }

    } // end of window active (ie. frontmost window)
  else
    { // not active - just show active world, if only one
    if (nDocCount == 1 && !strName.IsEmpty ())
      strTitle = strName;
    else
      strTitle = "MUSHclient";
    }


  CString strOldTitle;

  GetWindowText (strOldTitle);

  // use the title from SetMainTitle if the active world had one
  if (!strMainTitle.IsEmpty ())
    strTitle = strMainTitle;

  // only change title if necessary, to avoid flicker
  if (strTitle != strOldTitle)
    SetWindowText (strTitle);

  } // end of FixUpTitleBar

// should get a WM_USER when a host name lookup completes

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) 
{
  if(workspace_composer::BorrowedEditors().count(pMsg->hwnd) && workspace_composer::CompositionActive(pMsg->hwnd))return FALSE;
  if(workspace_composer::PreTranslateInput(pMsg))return TRUE;
  if(HandleWorkspaceShortcut(pMsg))return TRUE;
  if(m_workspaceShell && m_workspaceShell->PreTranslate(pMsg))return TRUE;
  if(pMsg->message==WM_KEYDOWN && pMsg->wParam>=VK_F1 && pMsg->wParam<=VK_F9 &&
     (GetKeyState(VK_CONTROL)&0x8000) && (GetKeyState(VK_SHIFT)&0x8000) && !(GetKeyState(VK_MENU)&0x8000))
    { if(FocusWorkspaceTranscript((size_t)(pMsg->wParam-VK_F1)))return TRUE; }


// ******************* DOMAIN NAME resolution ********************

  if (pMsg->message == WM_USER_HOST_NAME_RESOLVED && pMsg->wParam != 0)
    {

// find which world this message was for

 	  POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();
    CMUSHclientDoc* pDoc = NULL;

	  while (pos)
	  {
       pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);

      if (pDoc->m_hNameLookup == (char *) pMsg->wParam)
        break;

      for (POSITION chatpos = pDoc->m_ChatList.GetHeadPosition (); chatpos; )
        {
        CChatSocket * pSocket = pDoc->m_ChatList.GetNext (chatpos);
        if (pSocket->m_hNameLookup == (char *) pMsg->wParam)
          {
          pSocket->HostNameResolved (pMsg->wParam, pMsg->lParam);
          return TRUE;    // message handled
          }   // end of found a chat socket that was resolving a host name
        }  // end of checking chat sockets

      pDoc = NULL;

     }  // end of checking documents

    if (pDoc)
      pDoc->HostNameResolved (pMsg->wParam, pMsg->lParam);

    return TRUE;    // message was handled
    }

// ******************* SCRIPT FILE contents have changed ********************

  if (pMsg->message == WM_USER_SCRIPT_FILE_CONTENTS_CHANGED && pMsg->wParam != 0)
    {

// find which world this message was for

 	  POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();
    CMUSHclientDoc* pDoc = NULL;

	  while (pos)
	  {
       pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);

      if (pDoc == (CMUSHclientDoc *) pMsg->wParam)
        break;
      else
        pDoc = NULL;

     }

    if (pDoc)
      pDoc->OnScriptFileChanged ();

    return TRUE;    // message was handled
    }


 // ******************* Tips dialog after unregistered delay ********************

 if (pMsg->message == WM_USER_SHOW_TIPS)
   {
   OnHelpTipoftheday ();
   return TRUE;   // message was handled
   }

// ******************* TLS fallback prompt (deferred from socket callback) ********************

 if (pMsg->message == WM_USER_SSL_FALLBACK_PROMPT && pMsg->wParam != 0)
   {
   // find which world this message was for (verify pointer is still valid)
   POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();
   CMUSHclientDoc* pDoc = NULL;

   while (pos)
     {
     pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);
     if (pDoc == (CMUSHclientDoc *) pMsg->wParam)
       break;
     else
       pDoc = NULL;
     }

   if (pDoc)
     {
     CString strPrompt;
     strPrompt.Format ("TLS connection to \"%s\" failed.\n\n%s\n\n"
                       "Connect without encryption?",
                       (LPCTSTR) pDoc->m_mush_name,
                       (LPCTSTR) pDoc->m_strSSLLastError);

     if (UMessageBox (strPrompt, MB_YESNO | MB_ICONQUESTION) == IDYES)
       {
       pDoc->m_bUseSSL = false;
       pDoc->ConnectSocket ();
       }
     }

   return TRUE;   // message was handled
   }


 return CMDIFrameWndEx::PreTranslateMessage(pMsg);

 }

void CMainFrame::OnContextMenu(CWnd*, CPoint point)
{

	// CG: This block was added by the Pop-up Menu component
	{
		if (point.x == -1 && point.y == -1){
			//keystroke invocation
			CRect rect;
			GetClientRect(rect);
			ClientToScreen(rect);

			point = rect.TopLeft();
			point.Offset(5, 5);
		}

		CMenu menu;
		VERIFY(menu.LoadMenu(CG_IDR_POPUP_MAIN_FRAME));

		CMenu* pPopup = menu.GetSubMenu(0);
		ASSERT(pPopup != NULL);
		CWnd* pWndPopupOwner = this;
    CMenu mainmenu;

    // in full-screen mode, give access to all menu items
    if (Frame.IsFullScreen ())
      {
		  VERIFY(mainmenu.LoadMenu(IDR_MUSHCLTYPE));

      pPopup->AppendMenu (MF_SEPARATOR, 0, ""); 
      pPopup->AppendMenu (MF_POPUP | MF_ENABLED, (UINT ) mainmenu.m_hMenu, 
                          "Main Menus");     

      }

		while (pWndPopupOwner->GetStyle() & WS_CHILD &&
          pWndPopupOwner != pWndPopupOwner->GetParent())
			pWndPopupOwner = pWndPopupOwner->GetParent();

    pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
			pWndPopupOwner);
	}
}

void CMainFrame::OnUpdateDisplayActivitylist(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  pCmdUI->SetCheck (App.m_pActivityView != NULL);
  pCmdUI->Enable (TRUE);
}

void DoGlobalPrefs (CMUSHclientDoc * pCurrentDoc);

void CMainFrame::OnFilePreferences()
{
  DoGlobalPrefs (NULL);
}


// this is for the owner-draw status-line control that draws the word
// "MORE" in inverse (or indeed, any word supplied in itemData)

void CMainFrame::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct) 
{

int iContext = SaveDC (lpDrawItemStruct->hDC);

SetTextColor(lpDrawItemStruct->hDC, RGB (255,255,255) );  // text is white

SetBkColor(lpDrawItemStruct->hDC, RGB (0,0,0) );    // background is black

FillRect(lpDrawItemStruct->hDC, 
              &lpDrawItemStruct->rcItem, 
              (HBRUSH) GetStockObject(BLACK_BRUSH));   // fill with black

SetBkMode (lpDrawItemStruct->hDC, TRANSPARENT);

TextOut(lpDrawItemStruct->hDC, 
        lpDrawItemStruct->rcItem.left, 
        lpDrawItemStruct->rcItem.top, 
        (LPCTSTR) lpDrawItemStruct->itemData, 
        strlen ((LPCTSTR) lpDrawItemStruct->itemData));


RestoreDC (lpDrawItemStruct->hDC, iContext);
  
}


void CMainFrame::OnConnectionAutoconnect() 
{
App.m_bAutoConnectWorlds = !App.m_bAutoConnectWorlds;
App.db_write_int ("prefs", "AutoConnectWorlds", App.m_bAutoConnectWorlds);   
}

void CMainFrame::OnUpdateConnectionAutoconnect(CCmdUI* pCmdUI) 
{
DoFixMenus (pCmdUI);  // remove accelerators from menus
pCmdUI->SetCheck (App.m_bAutoConnectWorlds);
}

void CMainFrame::OnConnectionReconnectondisconnect() 
{

#ifdef _DEBUG
//  ListAccelerators (NULL, 3);     // for documenting menus, accelerators
#endif 

App.m_bReconnectOnLinkFailure = !App.m_bReconnectOnLinkFailure;
App.db_write_int ("prefs", "ReconnectOnLinkFailure", App.m_bReconnectOnLinkFailure);   
}

void CMainFrame::OnUpdateConnectionReconnectondisconnect(CCmdUI* pCmdUI) 
{
DoFixMenus (pCmdUI);  // remove accelerators from menus
pCmdUI->SetCheck (App.m_bReconnectOnLinkFailure);
}

void CMainFrame::OnHelpContents() 
{
	App.HelpHelper(0L, HELP_FINDER);
}

void CMainFrame::OnUpdateBtnWorldsWorld0(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (10, pCmdUI);
}

void CMainFrame::OnUpdateBtnWorldsWorld1(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (1, pCmdUI);
}

void CMainFrame::OnUpdateBtnWorldsWorld2(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (2, pCmdUI);	
}

void CMainFrame::OnUpdateBtnWorldsWorld3(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (3, pCmdUI);
}

void CMainFrame::OnUpdateBtnWorldsWorld4(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (4, pCmdUI);
}

void CMainFrame::OnUpdateBtnWorldsWorld5(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (5, pCmdUI);
}

void CMainFrame::OnUpdateBtnWorldsWorld6(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (6, pCmdUI);
}

void CMainFrame::OnUpdateBtnWorldsWorld7(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (7, pCmdUI);
}

void CMainFrame::OnUpdateBtnWorldsWorld8(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (8, pCmdUI);
}

void CMainFrame::OnUpdateBtnWorldsWorld9(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  OnUpdateBtnWorlds (9, pCmdUI);
}

void CMainFrame::OnWebPage() 
{
if ((long) ShellExecute (Frame, _T("open"), MY_WEB_PAGE, NULL, NULL, SW_SHOWNORMAL) <= 32)
  ::TMessageBox(
            "Unable to open the Gammon Software Solutions web page: "
            MY_WEB_PAGE, 
            MB_ICONEXCLAMATION);
}  // end of CMainFrame::OnWebPage


void CMainFrame::OnHelpForum() 
{
if ((long) ShellExecute (Frame, _T("open"), MUSHCLIENT_FORUM_URL, NULL, NULL, SW_SHOWNORMAL) <= 32)
  ::TMessageBox(
            "Unable to open the MUSHclient forum web page: "
            MUSHCLIENT_FORUM_URL, 
            MB_ICONEXCLAMATION);
}   // end of CMainFrame::OnHelpForum

void CMainFrame::OnHelpFunctionswebpage() 
{
if ((long) ShellExecute (Frame, _T("open"), MUSHCLIENT_FUNCTIONS_URL, NULL, NULL, SW_SHOWNORMAL) <= 32)
  ::TMessageBox(
            "Unable to open the MUSHclient forum web page: "
            MUSHCLIENT_FUNCTIONS_URL, 
            MB_ICONEXCLAMATION);
	
}  // end of CMainFrame::OnHelpFunctionswebpage




void CMainFrame::OnViewResetToolbars() 
{

	DockControlBar(&m_wndToolBar, AFX_IDW_DOCKBAR_TOP);
	DockControlBar(&m_wndGameToolBar, AFX_IDW_DOCKBAR_TOP);
	DockControlBar(&m_wndActivityToolBar, AFX_IDW_DOCKBAR_TOP);

  if (m_wndInfoBar.m_hWnd)
    DockControlBar(&m_wndInfoBar, AFX_IDW_DOCKBAR_BOTTOM);

}   // end of CMainFrame::OnViewResetToolbars

void CMainFrame::OnHelpMudlists() 
{
if ((long) ShellExecute (Frame, _T("open"), MUD_LIST, NULL, NULL, SW_SHOWNORMAL) <= 32)
  ::TMessageBox(
              "Unable to open the MUD lists web page: "
              MUD_LIST, 
              MB_ICONEXCLAMATION);
}   // end of CMainFrame::OnHelpMudlists

void CMainFrame::OnSize(UINT nType, int cx, int cy) 
{
	CMDIFrameWndEx::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here
	
}   // end of CMainFrame::OnSize

void CMainFrame::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized) 
{
	CMDIFrameWndEx::OnActivate(nState, pWndOther, bMinimized);
	
  m_bActive = nState != WA_INACTIVE;

  // if we are becoming active, stop flashing the title car
  if (m_bActive)
    {
    /*
    if (bWin95)
      FlashWindow (FALSE);
    else
      {
      FLASHWINFO fw;
      fw.cbSize = sizeof fw;
      fw.hwnd = m_hWnd;
      fw.dwFlags = FLASHW_STOP;
      fw.uCount = 0;
      fw.dwTimeout = 0;
      FlashWindowEx (&fw);
      }
      */
    if (m_bFlashingWindow)
      {
      FlashWindow (FALSE);
      m_bFlashingWindow = false;
      }

    }

  FixUpTitleBar ();

}   // end of CMainFrame::OnActivate


LRESULT CMainFrame::OnMCINotify(WPARAM wParam, LPARAM lParam)
  {
  // don't close it if we already did (ie. after a STOP)
  if (wParam == MCI_NOTIFY_SUCCESSFUL)
    {
    mciSendCommand(lParam, MCI_CLOSE, 0, NULL);
    m_wDeviceID = 0;    // now can play another one
    }
  return 0;
  } // end of CMainFrame::OnMCINotify

void CMainFrame::OnDisplayStopsoundplaying() 
{
  CancelSound ();
}

void CMainFrame::OnUpdateDisplayStopsoundplaying(CCmdUI* pCmdUI) 
{
DoFixMenus (pCmdUI);  // remove accelerators from menus
pCmdUI->Enable ();
}   // end of CMainFrame::OnUpdateDisplayStopsoundplaying

// cancels any sound currently playing
void CMainFrame::CancelSound (void)
  {
  if (m_wDeviceID)
    {
    mciSendCommand(m_wDeviceID, MCI_STOP, MCI_WAIT, NULL);
    mciSendCommand(m_wDeviceID, MCI_CLOSE, MCI_WAIT, NULL);
    m_wDeviceID = 0;
    }   // end of stopping previous one

  PlaySound (NULL, NULL, SND_PURGE);

  } // end of CMainFrame::CancelSound 

bool CMainFrame::PlaySoundFile (const CString & strSound)
  {
MCI_OPEN_PARMS mciOpenParms;
MCI_PLAY_PARMS mciPlayParms;
DWORD dwReturn;
CString strType = strSound.Right (4);

  strType.MakeLower ();

  // do .wav files differently so you can have more than one of them
  if (strType == ".wav")
    {  
    if (!PlaySound (strSound, NULL, SND_ASYNC | SND_FILENAME))
      return false;

    return true;
    } // end of having a .wav file


  CancelSound ();   // cancel one if we are already playing it

  mciOpenParms.lpstrElementName = strSound;


  if (dwReturn = mciSendCommand(NULL, 
                                MCI_OPEN,
                                MCI_OPEN_ELEMENT,        
                                (DWORD)(LPVOID) &mciOpenParms))
    {
     ShowMCIError (dwReturn, strSound);
     return false;   // couldn't play it
    }


    // The device opened successfully; get the device ID.
    Frame.m_wDeviceID = mciOpenParms.wDeviceID;

    // Use our frame's window for messages
    mciPlayParms.dwCallback = (DWORD) Frame.m_hWnd;

    // play the file - our frame window will close it when finished
    if (dwReturn = mciSendCommand(Frame.m_wDeviceID, MCI_PLAY, MCI_NOTIFY, 
                      (DWORD)(LPSTR)&mciPlayParms))
      {
      mciSendCommand(Frame.m_wDeviceID, MCI_CLOSE, 0, NULL);  // close on error
      Frame.m_wDeviceID = 0;
      ShowMCIError (dwReturn, strSound);
      return false;   // couldn't play it
      }   // end of error on play

  
  return true;    // played it OK
  } // end of CMainFrame::PlaySound 


void CMainFrame::ShowMCIError (const DWORD dwCode, const CString & strSound)
  {
char sMessage [128];

  mciGetErrorString (dwCode, sMessage, sizeof (sMessage));
  ::UMessageBox (TFormat ("Unable to play file %s, reason: %s",
                    (LPCTSTR) strSound, 
                    sMessage),
                    MB_ICONEXCLAMATION);
  } // end of  CMainFrame::ShowMCIError

void CMainFrame::OnHelpBugreportsuggestion() 
{
if ((long) ShellExecute (Frame, _T("open"), BUG_REPORT_PAGE, NULL, NULL, SW_SHOWNORMAL) <= 32)
  ::TMessageBox(
            "Unable to open the Gammon Software Solutions Bug Report web page: "
            BUG_REPORT_PAGE, 
            MB_ICONEXCLAMATION);
	
}    // end of CMainFrame::OnHelpBugreportsuggestion

void CMainFrame::OnViewAlwaysontop() 
{

App.m_bAlwaysOnTop = !App.m_bAlwaysOnTop;

  if (App.m_bAlwaysOnTop)
    SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
  else
    SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE);
	
  App.db_write_int ("prefs", "AlwaysOnTop", App.m_bAlwaysOnTop);	

}   // end of  CMainFrame::OnViewAlwaysontop

void CMainFrame::OnUpdateViewAlwaysontop(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus

  pCmdUI->Enable ();
  pCmdUI->SetCheck (App.m_bAlwaysOnTop);
	
}    // end of CMainFrame::OnUpdateViewAlwaysontop

void CMainFrame::OnGameSendtoallworlds() 
{

CSendToAllDlg dlg;

  dlg.m_bEcho = App.m_bEchoSendToAll;

  if (dlg.DoModal () != IDOK)
    return;

  App.m_bEchoSendToAll = dlg.m_bEcho;

  // append an endline

  if (dlg.m_strSendText.Right (2) != ENDLINE)
    dlg.m_strSendText += ENDLINE;

  // now send to all worlds that were selected in the dialog
  for (POSITION docPos = App.m_pWorldDocTemplate->GetFirstDocPosition();
      docPos != NULL; )
    {

    CMUSHclientDoc * pDoc = (CMUSHclientDoc *) App.m_pWorldDocTemplate->GetNextDoc(docPos);

    if (pDoc->m_bSelected)
      pDoc->SendMsg (dlg.m_strSendText, App.m_bEchoSendToAll, false, pDoc->LoggingInput ());
    } // end of doing each document
	
}   // end of CMainFrame::OnGameSendtoallworlds


void CMainFrame::OnEditReloadnamesfile() 
  {
	try
	  {
    ReadNames ("*");
    }
	catch (CException* e)
	  {
		e->ReportError();
		e->Delete();
	  }
  }  // end of CMainFrame::OnEditReloadnamesfile

void CMainFrame::OnViewFullscreenmode() 
{
#define NORMAL_STYLES  WS_CAPTION | WS_BORDER | WS_SYSMENU | WS_THICKFRAME | \
        WS_POPUP | WS_OVERLAPPEDWINDOW
//#define FULL_SCREEN_STYLES WS_DLGFRAME  // old way
#define FULL_SCREEN_STYLES WS_BORDER      // new in version 4.43

  if (m_bFullScreen)
    {
    LockWindowUpdate ();     
    m_bFullScreen = false;

    ModifyStyle (FULL_SCREEN_STYLES, NORMAL_STYLES, SWP_FRAMECHANGED);
		// Invalidate before SetMenu since we are going to replace
		//  the frame's client area anyway
		Invalidate();
  	m_nIdleFlags |= idleMenu;
		// put the menu back in place if it was removed before
           
    CMDIChildWnd* pActiveWnd = MDIGetActive();
    
	  if (pActiveWnd != NULL)
		  pActiveWnd->OnUpdateFrameMenu(TRUE, pActiveWnd, NULL);
    else
      OnUpdateFrameMenu (NULL);   // select correct menu

    SetWindowPlacement (&m_wpPrev);   // put window back, including maximised state

		RecalcLayout();

    UnlockWindowUpdate ();
    }
  else
    // here to make it full screen
    {
    m_bMaximized = FALSE;
    CMDIChildWnd* pActiveWnd = MDIGetActive(&m_bMaximized);
    
    // to solve a bizarre problem with multiple system menus appearing if the child
    // window is maximized, we un-maximize it
	  if (pActiveWnd != NULL)
      pActiveWnd->MDIRestore ();

    m_bFullScreen = true;
    // Get rid of the menu first (will resize the window)
		if (::GetMenu(m_hWnd))
		  {
			// Invalidate before SetMenu since we are going to replace
			//  the frame's client area anyway
			Invalidate();
			SetMenu(NULL);
			m_nIdleFlags &= ~idleMenu;  // avoid any idle menu processing
		  }  // end of having a menu


    ModifyStyle (NORMAL_STYLES, FULL_SCREEN_STYLES, SWP_FRAMECHANGED);
    GetWindowPlacement (&m_wpPrev);
    ShowWindow(SW_MAXIMIZE);    // make sure it *does* take the full screen
	  if (pActiveWnd != NULL)
      {
      // now remaximize it if necessary
      if (m_bMaximized)
        pActiveWnd->MDIMaximize (); // restore child's maximized state
      }

    }

}   // end of CMainFrame::OnViewFullscreenmode

void CMainFrame::OnUpdateViewFullscreenmode(CCmdUI* pCmdUI) 
{
DoFixMenus (pCmdUI);  // remove accelerators from menus
pCmdUI->SetCheck (m_bFullScreen);
pCmdUI->Enable ();

}  // end of CMainFrame::OnUpdateViewFullscreenmode

void CMainFrame::OnEditNotesworkarea() 
{

  CreateTextWindow ("",     // contents
                    "",     // title
                    NULL,   // document
                    0,      // document number
                    App.m_strDefaultInputFont,
                    App.m_iDefaultInputFontHeight,
                    App.m_iDefaultInputFontWeight,
                    App.m_iDefaultInputFontCharset,
                    RGB (0, 0, 0),
                    RGB (255, 255, 255),
                    "",     // search string
                    "",       // line preamble
                    false,
                    false,
                    false,
                    false,  
                    false,
                    false,
                    eNotepadNormal
                    );

}  // end of CMainFrame::OnEditNotesworkarea


BOOL CMainFrame::OnDynamicTipText(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	// call one global single handler
	return ::OnNeedText(id,pNMHDR,pResult);
}   // end of CMainFrame::OnDynamicTipText



// OnNeedText
//
// This function has been designed so that this source code can be used in
// both UNICODE and non-UNICODE projects.

BOOL OnNeedText(UINT id, NMHDR* pNMHDR, LRESULT* pResult)
{
	// OnNeedText should only be called for TTN_NEEDTEXT notifications!
	ASSERT(pNMHDR->code == TTN_NEEDTEXTA || pNMHDR->code == TTN_NEEDTEXTW);

	// need to handle both ANSI and UNICODE versions of the message
	TOOLTIPTEXTA* pTTTA = (TOOLTIPTEXTA*)pNMHDR;
	TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
	CString strTipText; // used to hold final tool tip text before UNICODE conversion
	TCHAR szFullText[256];

	UINT nID = pNMHDR->idFrom;
	if (pNMHDR->code == TTN_NEEDTEXTA && (pTTTA->uFlags & TTF_IDISHWND) ||
		pNMHDR->code == TTN_NEEDTEXTW && (pTTTW->uFlags & TTF_IDISHWND))
	{
		// idFrom is actually the HWND of the tool
		nID = ((UINT)(WORD)::GetDlgCtrlID((HWND)nID));
	}

  int iWorld;

  // find the document relating to this toolbar ID, if any
  CMUSHclientDoc * pDoc = FindWorld (nID, iWorld);

  if (iWorld == 0)    // not the world toolbar
    {
		if (nID != 0) // will be zero on a separator
		{
			AfxLoadString(nID, szFullText);
				// this is the command id, not the button index
			AfxExtractSubString(strTipText, szFullText, 1, '\n');
		}
    }
  else
    {   // world toolbar
    if (pDoc)
      strTipText = CFormat ("%s (Ctrl+%i)",
                  (LPCTSTR) pDoc->m_mush_name,
                  iWorld == 10 ? 0 : iWorld);
    else
      strTipText = "Inactive world";
    }

// handle conditionally for both UNICODE and non-UNICODE apps
#ifndef _UNICODE
	if (pNMHDR->code == TTN_NEEDTEXTA)
		lstrcpyn(pTTTA->szText, strTipText, NUMITEMS(pTTTA->szText));
	else
		_mbstowcsz(pTTTW->szText, strTipText, NUMITEMS(pTTTW->szText));
#else
	if (pNMHDR->code == TTN_NEEDTEXTA)
		_wcstombsz(pTTTA->szText, strTipText, NUMITEMS(pTTTA->szText));
	else
		lstrcpyn(pTTTW->szText, strTipText, NUMITEMS(pTTTW->szText));
#endif
	*pResult = 0;

	// bring the tooltip window above other popup windows
	::SetWindowPos(pNMHDR->hwndFrom, HWND_TOP, 0, 0, 0, 0,
		SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOMOVE);

	return TRUE;    // message was handled
}     // end of OnNeedText


void CMainFrame::OnUpdateFrameMenu(HMENU hMenuAlt)
  {

  if (IsFullScreen ())
    return;

  CMDIFrameWndEx::OnUpdateFrameMenu (hMenuAlt);
  EnsureWorkspaceMenu();

  }  // end of CMainFrame::OnUpdateFrameMenu

void CMainFrame::OnWindowCloseallnotepadwindows() 
{
  for (POSITION docPos = App.m_pNormalDocTemplate->GetFirstDocPosition();
      docPos != NULL; )
    {

    CDocument * pDoc =  App.m_pNormalDocTemplate->GetNextDoc(docPos);

    // see if they want to save it
	  if (!pDoc->SaveModified())
		  return;

    // saved OK, let's close it
    pDoc->OnCloseDocument ();


    } // end of doing each document
	
}   // end of CMainFrame::OnWindowCloseallnotepadwindows

void CMainFrame::DoFileOpen (void)
  {

	CString title;
	VERIFY(title.LoadString(AFX_IDS_OPENFILE));

  CString fileName;

  CString strSuggestedName;
  CString strFilter = "World or text files (*.mcl;*.txt)|*.mcl;*.txt|All files (*.*)|*.*||";

  if (bWine)
    {
    strSuggestedName = "*.mcl";
    strFilter = "World files (*.mcl)|*.mcl|All files (*.*)|*.*||";
    }

  CFileDialog dlgFile (TRUE,   // loading the file
                 "mcl",        // default extension
                 strSuggestedName,           // suggested name
                 OFN_HIDEREADONLY | OFN_FILEMUSTEXIST,
                 strFilter,    // filter 
                 NULL);        // parent window

	dlgFile.m_ofn.lpstrTitle = title;
	dlgFile.m_ofn.lpstrFile = fileName.GetBuffer(_MAX_PATH);

  // use default world file directory
  dlgFile.m_ofn.lpstrInitialDir = Make_Absolute_Path (App.m_strDefaultWorldFileDirectory);

  ChangeToFileBrowsingDirectory ();
  int nResult = dlgFile.DoModal();
  ChangeToStartupDirectory ();

  fileName.ReleaseBuffer();

  if (nResult != IDOK)
    return;

  // wine seems to open the world file as text (XML) which isn't much use
  if (bWine)
    App.m_pWorldDocTemplate->OpenDocumentFile (dlgFile.m_ofn.lpstrFile);	
  else
    App.OpenDocumentFile (dlgFile.m_ofn.lpstrFile);	

  }   // end of  CMainFrame::DoFileOpen 

void CMainFrame::OnFileOpen() 
{
  DoFileOpen ();
	
}    // end of CMainFrame::OnFileOpen

void CMainFrame::OpenAndConnectToWorldsInStartupList (const bool bConnect)
  {

// open all worlds specified in global preferences

  CString strWorldList = App.m_strWorldList;
  CDocument * pDoc;

  while (!strWorldList.IsEmpty ())
    {
    int i = strWorldList.Find ('*');
    if (i == -1)
      {
      pDoc = App.OpenDocumentFile (strWorldList);
      }
    else
      {
      pDoc = App.OpenDocumentFile (strWorldList.Left (i));
      strWorldList = strWorldList.Mid (i + 1);
      }

    if (bConnect && pDoc && pDoc->IsKindOf(RUNTIME_CLASS(CMUSHclientDoc)))
      {
      // if connection wanted too, do it  [#433]
      if (((CMUSHclientDoc *) pDoc)->m_iConnectPhase == eConnectNotConnected)
        ((CMUSHclientDoc *) pDoc)->ConnectSocket();
      }

    if (i == -1)
      break;    // end of list reached
    } // end of processing list


  }  // end of CMainFrame::OpenAndConnectToWorldsInStartupList 

void CMainFrame::OnFileOpenworldsinstartuplist() 
{

	OpenAndConnectToWorldsInStartupList (false);

}     // end of  CMainFrame::OnFileOpenworldsinstartuplist

void CMainFrame::OnUpdateFileOpenworldsinstartuplist(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  pCmdUI->Enable (!App.m_strWorldList.IsEmpty ());
}    // end of CMainFrame::OnUpdateFileOpenworldsinstartuplist

void CMainFrame::OnFileWinsock() 
{
CWinsockInfoDlg dlg;

CString strHostName;
CString strAddresses;

  GetHostNameAndAddresses (strHostName, strAddresses);

  dlg.m_strVersion = CFormat ("%04X", App.m_wsadata.wVersion);
  dlg.m_strHighVersion = CFormat ("%04X", App.m_wsadata.wHighVersion);
  dlg.m_szDescription = App.m_wsadata.szDescription;
  dlg.m_szSystemStatus = App.m_wsadata.szSystemStatus;
  dlg.m_iMaxSockets = App.m_wsadata.iMaxSockets;
  dlg.m_strHostName = strHostName;
  dlg.m_strAddresses = strAddresses;

  dlg.DoModal ();

}    // end of CMainFrame::OnFileWinsock

void CMainFrame::OnConnectionConnecttoallopenworlds() 
{
  // connect to all worlds  [#433]
  for (POSITION docPos = App.m_pWorldDocTemplate->GetFirstDocPosition();
      docPos != NULL; )
    {

    CMUSHclientDoc * pDoc = (CMUSHclientDoc *) App.m_pWorldDocTemplate->GetNextDoc(docPos);

    if (pDoc->m_iConnectPhase == eConnectNotConnected)
      pDoc->ConnectSocket();
    } // end of doing each document
	
}   // end of CMainFrame::OnConnectionConnecttoallopenworlds

void CMainFrame::OnConnectionConnecttoworldsinstartuplist() 
{
	OpenAndConnectToWorldsInStartupList (true);
	
}    // end of CMainFrame::OnConnectionConnecttoworldsinstartuplist

void CMainFrame::OnUpdateConnectionConnecttoworldsinstartuplist(CCmdUI* pCmdUI) 
{
  DoFixMenus (pCmdUI);  // remove accelerators from menus
  pCmdUI->Enable (!App.m_strWorldList.IsEmpty ());
	
}   // end of CMainFrame::OnUpdateConnectionConnecttoworldsinstartuplist


void CMainFrame::OnF1Test(void)
  {
  if (!App.m_bF1macro)
    App.HelpHelper (0);

  }  // end of CMainFrame::OnF1Test

void CMainFrame::OnHelpDocumentationwebpage() 
{
if ((long) ShellExecute (Frame, _T("open"), DOCUMENTATION_PAGE, NULL, NULL, SW_SHOWNORMAL) <= 32)
  ::TMessageBox(
            "Unable to open the MUSHclient documentation web page: "
            DOCUMENTATION_PAGE, 
            MB_ICONEXCLAMATION);
	
}  // end of CMainFrame::OnHelpDocumentationwebpage

void CMainFrame::OnHelpRegularexpressionswebpage() 
{
if ((long) ShellExecute (Frame, _T("open"), REGEXP_PAGE, NULL, NULL, SW_SHOWNORMAL) <= 32)
  ::TMessageBox(
            "Unable to open the regular expressions web page: "
            REGEXP_PAGE, 
            MB_ICONEXCLAMATION);
	
}   // end of CMainFrame::OnHelpRegularexpressionswebpage

void CMainFrame::OnUpdateInfoBar(CCmdUI* pCmdUI)
  {
  DoFixMenus (pCmdUI);  // remove accelerators from menus

  if (!m_wndInfoBar.m_hWnd)
    {
    pCmdUI->Enable (FALSE);
    pCmdUI->SetCheck (0);
    return;
    }

  CMDIFrameWnd::OnUpdateControlBarMenu (pCmdUI);

  } // end of  CMainFrame::OnUpdateInfoBa


void CMainFrame::AddTrayIcon (void)
  {

//  MUSHclient icon in tray, if wanted
//  see: http://www.codeproject.com/shell/StealthDialog.asp#xx588118xx

// the ID number can be any UINT you choose and will
// be used to identify your icon in later calls to
// Shell_NotifyIcon


    m_niData.uID = WM_USER_TRAY_ICON_ID;


// state which structure members are valid
// here you can also choose the style of tooltip
// window if any - specifying a balloon window:
// NIF_INFO is a little more complicated 


    m_niData.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;

// set up tool tip

    strcpy (m_niData.szTip, "MUSHclient");

// load the icon note: you should destroy the icon
// after the call to Shell_NotifyIcon

    int iIconIDs [10] = 
      {
      IDR_MUSHCLTYPE,
      IDI_ICON1,
      IDI_ICON2,
      IDI_ICON3,
      IDI_ICON4,
      IDI_ICON5,
      IDI_ICON6,
      IDI_ICON7,
      IDI_ICON8,
      IDI_ICON9,
      };

    int iIconNumber = App.m_iTrayIcon;
    m_niData.hIcon = NULL;

    // load icon from disk - if at maximum number (ie. 10)
    if (iIconNumber == NUMITEMS (iIconIDs) && 
        !App.m_strTrayIconFileName.IsEmpty ())
      m_niData.hIcon = (HICON)::LoadImage(
                      NULL,
                      App.m_strTrayIconFileName,
                      IMAGE_ICON,
                      GetSystemMetrics(SM_CXSMICON),
                      GetSystemMetrics(SM_CYSMICON),
                      LR_LOADFROMFILE|LR_DEFAULTCOLOR
                      );

    if (iIconNumber < 0 || iIconNumber >= NUMITEMS (iIconIDs))
      iIconNumber = 0;

    // if not custom icon, use one from our resource
    if (m_niData.hIcon == NULL)
      {
   	  HINSTANCE hInst = AfxGetResourceHandle();

      m_niData.hIcon =
          (HICON)LoadImage( hInst,
              MAKEINTRESOURCE(iIconIDs [iIconNumber]),
              IMAGE_ICON,
              GetSystemMetrics(SM_CXSMICON),
              GetSystemMetrics(SM_CYSMICON),
              LR_DEFAULTCOLOR);
      }

   if (m_niData.hIcon == NULL)
     return;

// set the window you want to recieve event messages


    m_niData.hWnd = m_hWnd;


// set the message to send
// note: the message value should be in the
// range of WM_APP through 0xBFFF


    m_niData.uCallbackMessage = WM_USER_TRAY_ICON_MESSAGE;

// NIM_ADD adds a new tray icon
    
    Shell_NotifyIcon (NIM_ADD, &m_niData);

// now get rid of icon from memory

  if (m_niData.hIcon) 
        DestroyIcon(m_niData.hIcon); 
 
  } // end of CMainFrame::AddTrayIcon

LRESULT CMainFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
  if(message==WM_INITMENU)EnsureWorkspaceMenu();

 if (message == WM_USER_TRAY_ICON_MESSAGE)
   {

    switch(lParam)
        {
        case WM_LBUTTONDBLCLK:

            if (IsIconic ())
              ShowWindow(SW_RESTORE);

            SetForegroundWindow();
            SetFocus();
            return true;

        case WM_LBUTTONDOWN:

            if (IsIconic ())
              ShowWindow(SW_RESTORE);

            SetForegroundWindow();
            SetFocus();
            return true;

        case WM_RBUTTONDOWN:

            if (IsIconic ())
              ShowWindow(SW_RESTORE);

            SetForegroundWindow ();
            SetFocus();
            LeftTrayClick ();
            return true;
        }
    
   }  // end of tray icon message
	
	return CWorkspaceFrame::WindowProc(message, wParam, lParam);
}   // end of CMainFrame::WindowProc


void CMainFrame::LeftTrayClick (void)
  {
CPoint point;
  
  GetCursorPos(&point);

  multimap<string, int> sWorlds;

// add all documents to the list

  POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();

	for (int nItem = 0; pos != NULL; nItem++)
	  {
    CMUSHclientDoc* pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);

    CString strName = pDoc->m_mush_name;

    if (pDoc->m_new_lines)
      strName += CFormat (" (%i)", pDoc->m_new_lines);
    
    if ((pDoc->m_view_number - 1) < TRAY_MENU_COUNT)
     sWorlds.insert (make_pair (std::basic_string<TCHAR>(strName), pDoc->m_view_number - 1));

    }   // end of doing each world

  CMenu menu;
	VERIFY(menu.LoadMenu(IDR_MXP_MENU));

	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup != NULL);
	CWnd* pWndPopupOwner = this;

  pPopup->DeleteMenu (0, MF_BYPOSITION);  // get rid of dummy item

  if (sWorlds.empty ())
    pPopup->AppendMenu (MF_STRING | MF_GRAYED, TRAY_FIRST_MENU, "(no worlds open)");
  else
    for (multimap<string, int>::const_iterator i = sWorlds.begin ();
         i != sWorlds.end ();
         i++)
      // add menu item
      pPopup->AppendMenu (MF_STRING | MF_ENABLED, 
                          TRAY_FIRST_MENU + i->second,    // world number
                          i->first.c_str ());             // name and count of new lines         


	while (pWndPopupOwner->GetStyle() & WS_CHILD)
		pWndPopupOwner = pWndPopupOwner->GetParent();

	pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, 
                        point.x, 
                        point.y,
			                  pWndPopupOwner);

  }  // end of CMainFrame::LeftTrayClick

void CMainFrame::RightTrayClick (void)
  {
CPoint point;
  

// I am having a problem with crashes when you use this, when a dialog
// is open, because the menu doesn't grey out, so you can have 2 about
// boxes, for instance, followed by a crash.

  GetCursorPos(&point);

  int iEnabled = MF_ENABLED;

  if (!IsWindowEnabled ())
     iEnabled = 0;

  SetForegroundWindow ();
  SetFocus();

	CMenu menu;
	VERIFY(menu.LoadMenu(IDR_RIGHT_TRAY_MENU));

	CMenu* pPopup = menu.GetSubMenu(0);
	ASSERT(pPopup != NULL);
	CWnd* pWndPopupOwner = this;

	CMenu mainmenu;
	VERIFY(mainmenu.LoadMenu(IDR_MUSHCLTYPE));


  if (iEnabled)
    pPopup->AppendMenu (MF_STRING | MF_GRAYED, TRAY_FIRST_MENU, "(enabled)");
  else
    pPopup->AppendMenu (MF_STRING | MF_GRAYED, TRAY_FIRST_MENU, "(disabled)");

//  pPopup->AppendMenu (MF_POPUP | iEnabled, (UINT ) mainmenu.m_hMenu, 
//                      "Main Menus");     


		while (pWndPopupOwner->GetStyle() & WS_CHILD &&
          pWndPopupOwner != pWndPopupOwner->GetParent())
			pWndPopupOwner = pWndPopupOwner->GetParent();

   pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y,
		pWndPopupOwner);

  } // end of CMainFrame::RightTrayClick 

void CMainFrame::OnTrayMenu (UINT nID)
  {
  int iWorld = nID - TRAY_FIRST_MENU;

CMUSHclientDoc* pDoc = NULL;

  POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();

	for (int nItem = 0; pos != NULL; nItem++)
	  {
    pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);

    if (iWorld == (pDoc->m_view_number - 1))
      {

      for(POSITION pos=pDoc->GetFirstViewPosition();pos!=NULL;)
        {
        CView* pView = pDoc->GetNextView(pos);

        if (pView->IsKindOf(RUNTIME_CLASS(CSendView)))
          {
          CSendView* pmyView = (CSendView*)pView;

          if (pmyView->GetParentFrame ()->IsIconic ())
            pmyView->GetParentFrame ()->ShowWindow (SW_RESTORE);

          pmyView->GetParentFrame ()->ActivateFrame ();
          pmyView->m_owner_frame->SetActiveView(pmyView);

          break;

          }	
        }

      }   // end of found the world

    }   // end of doing each world


  } // end of CMainFrame::OnTrayMenu

void CMainFrame::OnFixMenus(CCmdUI* pCmdUI)
  {
  pCmdUI->ContinueRouting();
  DoFixMenus (pCmdUI);
  }   // end of CMainFrame::OnFixMenus

void CMainFrame::DoFixMenus(CCmdUI* pCmdUI)
  {

  CMDIChildWnd* pChild = MDIGetActive ();

  // find child, if any
  if (!pChild)
    return;

  // is it a mushview/sendview combination?
  if (!pChild->IsKindOf(RUNTIME_CLASS(CChildFrame)))
    return; // no

  // cast it
  CChildFrame * pMUDchild = (CChildFrame *) pChild;

  CMUSHclientDoc * pDoc = pMUDchild->m_pDoc;

  if (!pDoc)
    return;

  pDoc->DoFixMenus (pCmdUI);

  } // end of CMainFrame::OnFixMenus

void CMainFrame::OnHelpPluginslist() 
{
if ((long) ShellExecute (Frame, _T("open"), PLUGINS_PAGE, NULL, NULL, SW_SHOWNORMAL) <= 32)
  ::TMessageBox(
            "Unable to open the plugins web page: "
            PLUGINS_PAGE, 
            MB_ICONEXCLAMATION);
	
}   // end of CMainFrame::OnHelpPluginslist

void CMainFrame::OnUpdateFrameTitle(BOOL bAddToTitle)
  {
  CMDIFrameWndEx::OnUpdateFrameTitle (bAddToTitle);

  if (m_wndMDITabs.InUse ())
    m_wndMDITabs.Update ();

  } // end of CMainFrame::OnUpdateFrameTitle

void CMainFrame::OnEditGenerateuniqueid() 
{
CUniqueIDDlg dlg;

dlg.m_strUniqueID = ::GetUniqueID ();	

  dlg.DoModal ();

}   // end of CMainFrame::OnEditGenerateuniqueid


void CMainFrame::OnEditConvertclipboardforumcodes() 
{
CString strContents; 

  if (!GetClipboardContents (strContents, false, false))
    return;

 putontoclipboard  (QuoteForumCodes (strContents));

}    // end of CMainFrame::OnEditConvertclipboardforumcodes

void CMainFrame::OnSysCommand(UINT nID, LPARAM lParam) 
{
	/* This message is sent to the top level window by DefWindowProc in
	 * response to a WM_SYSKEYUP for Alt or F10. If we pass it to DefWindowProc
	 * the menu bar will be activated. When DisableKeyboardMenuActivation is 
	 * enabled, we suppress this behaviour by throwing away the message, provided
	 * a CSendView or CMUSHView has the input focus. */
	CWnd *focusedWnd = GetFocus();
	if (focusedWnd != NULL &&
      (
      nID == SC_KEYMENU && 
      App.m_bDisableKeyboardMenuActivation &&
			 (focusedWnd->IsKindOf(RUNTIME_CLASS(CSendView)) ||
			  (focusedWnd->IsKindOf(RUNTIME_CLASS(CMUSHView)) && 
			  App.m_bAllTypingToCommandWindow))))
    {
		/* Discard. */
    } 
  else
		CMDIFrameWndEx::OnSysCommand(nID, lParam);
}   // end of CMainFrame::OnSysCommand

/*

  See: http://www.gammon.com.au/forum/?id=10793

  This stuff is designed to compensate for the fact that timers are a low-priority event.

  From time to time (eg. when a packet arrives) we will check whether the timer interval has 
  elapsed that we expect (eg. normally 1/10 of a second these days) plus also the tick timers.

  If so, we call the timers anyway, so they fire closer to the advertised time.

  */

void CMainFrame::CheckTimerFallback ()
  {
  double fElapsedTime;      // normal timers
  double fElapsedTick;      // OnPluginTick timer

  if (App.m_iCounterFrequency)
    {
    LARGE_INTEGER timeNow;  

    QueryPerformanceCounter (&timeNow);

    LONGLONG iTimeTaken;
    
    // normal timers
    iTimeTaken = timeNow.QuadPart - hp_timeLastTimerFired.QuadPart;
    fElapsedTime = ((double) iTimeTaken) / ((double) App.m_iCounterFrequency);

    // OnPluginTick timer
    iTimeTaken = timeNow.QuadPart - hp_timeLastTickFired.QuadPart;
    fElapsedTick = ((double) iTimeTaken) / ((double) App.m_iCounterFrequency);
    }
  else  // 1-second resolution if no high-resolution timer available
    {
    fElapsedTime = (double) CTime::GetCurrentTime().GetTime () - (double) timeLastTimerFired.GetTime ();
    fElapsedTick = (double) CTime::GetCurrentTime().GetTime () - (double) timeLastTickFired.GetTime ();
    }

  // first do tick timers

  if (fElapsedTick > 0.040)    // more than 40 milliseconds has elapsed
    {
	  for (POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition(); pos;)
      {
      CMUSHclientDoc * pDoc = (CMUSHclientDoc*) App.m_pWorldDocTemplate->GetNextDoc(pos);
      pDoc->CheckTickTimers ();
      }  // end for

    // so we know when we prccessed this tick
    if (App.m_iCounterFrequency)
      QueryPerformanceCounter (&hp_timeLastTickFired);
    else
      timeLastTickFired = CTime::GetCurrentTime();

    }  // if time for OnPluginTick

  double fInterval = 0.10;   // 10th of a second (if m_nTimerInterval is zero)

  // if non-zero interval is seconds
  if (App.m_nTimerInterval)
    fInterval = App.m_nTimerInterval;

  // interval has elapsed, call timer anyway
  if (fElapsedTime > fInterval)
     ProcessTimers ();

  } // end of CMainFrame::CheckTimerFallback



void CMainFrame::EnsureWorkspaceMenu()
{
  CMenu* menu=GetMenu();
  if (!menu || menu->GetMenuState(ID_WORKSPACE_TRANSCRIPT, MF_BYCOMMAND)!=(UINT)-1) return;
  CMenu* workspace=new CMenu;
  workspace->CreatePopupMenu();
  workspace->AppendMenu(MF_STRING, ID_WORKSPACE_TRANSCRIPT, "Open &transcript for active world");
  workspace->AppendMenu(MF_STRING, 59006, "Manage named output &routes...");
  workspace->AppendMenu(MF_STRING, 59007, "Agent review and debug &captures...");
  workspace->AppendMenu(MF_STRING, 59008, "&Logging Setup for active world...");
  workspace->AppendMenu(MF_STRING, 59005, "&Float selected transcript separately");
  workspace->AppendMenu(MF_STRING, 59001, "PennMUSH code &editor...");
  workspace->AppendMenu(MF_STRING, 59002, "Agent &access...");
  workspace->AppendMenu(MF_STRING, 59003, "Agent &conversations...");
  workspace->AppendMenu(MF_STRING, 59004, "Scene &director...");
  workspace->AppendMenu(MF_SEPARATOR);
  workspace->AppendMenu(MF_STRING,59030,"Show &Worlds and channels");
  workspace->AppendMenu(MF_STRING,59035,"Game and &character labels...");
  workspace->AppendMenu(MF_STRING,59036,"Show character document tabs");
  workspace->AppendMenu(MF_STRING,59031,"Show &classic toolbars");
  CMenu appearance;appearance.CreatePopupMenu();appearance.AppendMenu(MF_STRING,59032,"Use Windows appearance");appearance.AppendMenu(MF_STRING,59033,"Light appearance");appearance.AppendMenu(MF_STRING,59034,"Dark appearance");
  workspace->AppendMenu(MF_POPUP,(UINT_PTR)appearance.Detach(),"Appearance");
  CMenu shortcuts;shortcuts.CreatePopupMenu();
  for(UINT index=0;index<9;++index) {CString label;label.Format("Pane %u\tCtrl+Shift+F%u",index+1,index+1);shortcuts.AppendMenu(MF_STRING,59010+index,label);}
  workspace->AppendMenu(MF_POPUP,(UINT_PTR)shortcuts.Detach(),"Pane shortcuts");
  menu->AppendMenu(MF_POPUP, (UINT_PTR)workspace->Detach(), "&Workspace");
  delete workspace;
  DrawMenuBar();
}

namespace {
std::wstring WorkspaceShellWide(const CString& text,bool utf8=false)
{const UINT page=utf8?CP_UTF8:CP_ACP;const DWORD flags=utf8?MB_ERR_INVALID_CHARS:0;const int size=MultiByteToWideChar(page,flags,text,text.GetLength(),NULL,0);if(!size && !text.IsEmpty())return L"Unnamed channel";std::wstring result(size,L'\0');if(size)MultiByteToWideChar(page,flags,text,text.GetLength(),&result[0],size);return result;}
CMUSHclientDoc* WorkspaceShellDocument(const std::string& identity)
{if(!App.m_pWorldDocTemplate)return NULL;for(POSITION pos=App.m_pWorldDocTemplate->GetFirstDocPosition();pos;){auto* doc=(CMUSHclientDoc*)App.m_pWorldDocTemplate->GetNextDoc(pos);if(identity==(LPCSTR)WorkspaceConnectionId(doc))return doc;}return NULL;}
std::string WorkspaceShellUtf8(const std::wstring& text)
{const int n=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),NULL,0,NULL,NULL);std::string value(n,'\0');if(n)WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),&value[0],n,NULL,NULL);return value;}
bool WorkspaceCharacterSchema()
{return App.db && sqlite3_exec(App.db,"CREATE TABLE IF NOT EXISTS workspace_character_labels_v1(world_id TEXT PRIMARY KEY,game_label TEXT NOT NULL,character_label TEXT NOT NULL)",NULL,NULL,NULL)==SQLITE_OK;}
CString WorkspaceCharacterKey(const CString& identity)
{const int runtime=identity.Find(":runtime:");return runtime<0?identity:identity.Left(runtime);}
void WorkspaceCharacterIdentity(CMainFrame& frame,CMUSHclientDoc& doc,workspace_shell::Row& row)
{const CString key=WorkspaceCharacterKey(row.world.c_str());auto found=frame.m_workspaceCharacterLabels.find(key);
 if(found==frame.m_workspaceCharacterLabels.end()){std::pair<std::wstring,std::wstring> value;
   if(WorkspaceCharacterSchema()){sqlite3_stmt* query=NULL;if(sqlite3_prepare_v2(App.db,"SELECT game_label,character_label FROM workspace_character_labels_v1 WHERE world_id=?",-1,&query,NULL)==SQLITE_OK){sqlite3_bind_text(query,1,key,-1,SQLITE_TRANSIENT);if(sqlite3_step(query)==SQLITE_ROW){const char* game=(const char*)sqlite3_column_text(query,0),*character=(const char*)sqlite3_column_text(query,1);value.first=WorkspaceShellWide(game?game:"",true);value.second=WorkspaceShellWide(character?character:"",true);if(value.first.size()>128 || value.second.size()>128)value={};}sqlite3_finalize(query);}}
   if(frame.m_workspaceCharacterLabels.size()>=4096)frame.m_workspaceCharacterLabels.clear();found=frame.m_workspaceCharacterLabels.insert(std::make_pair(key,value)).first;}
 CString player=doc.m_name;player.TrimLeft();player.TrimRight();row.characterLabel=!found->second.second.empty()?found->second.second:WorkspaceShellWide(player.IsEmpty()?doc.m_mush_name:player);if(row.characterLabel.empty())row.characterLabel=L"Unnamed character";
 row.gameLabel=!found->second.first.empty()?found->second.first:WorkspaceShellWide(doc.m_server);if(row.gameLabel.empty())row.gameLabel=L"Local game";
 if(!found->second.first.empty())row.game="named:"+WorkspaceShellUtf8(found->second.first);
 else{CString server=doc.m_server;server.MakeLower();row.game="endpoint:"+std::string((LPCSTR)server)+":"+std::to_string(doc.m_port);}}
bool WorkspaceShellActivate(CMainFrame& frame,const std::string& identity)
{return workspace_shell::ActivateOriginal(identity,[](const std::string& key){workspace_shell::Target target;auto* doc=WorkspaceShellDocument(key);if(!doc)return target;
    for(POSITION pos=doc->GetFirstViewPosition();pos;){auto* view=doc->GetNextView(pos);auto* child=DYNAMIC_DOWNCAST(CMDIChildWnd,view->GetParentFrame());if(child && child->GetSafeHwnd()){target.window=child->GetSafeHwnd();target.document=(UINT_PTR)doc;break;}}return target;},
  [&frame](HWND window){auto* child=DYNAMIC_DOWNCAST(CMDIChildWnd,CWnd::FromHandlePermanent(window));if(child)frame.MDIActivate(child);},
  [&frame](){workspace_shell::Target target;auto* child=frame.MDIGetActive();if(child){target.window=child->GetSafeHwnd();target.document=(UINT_PTR)child->GetActiveDocument();}return target;});}
bool WorkspaceShellSchema()
{if(!App.db || sqlite3_exec(App.db,"CREATE TABLE IF NOT EXISTS workspace_shell_v1(slot INTEGER PRIMARY KEY CHECK(slot=1),navigator INTEGER NOT NULL,legacy INTEGER NOT NULL,theme INTEGER NOT NULL,mask INTEGER NOT NULL DEFAULT 7)",NULL,NULL,NULL)!=SQLITE_OK)return false;
 sqlite3_stmt* columns=NULL;bool mask=false;if(sqlite3_prepare_v2(App.db,"PRAGMA table_info(workspace_shell_v1)",-1,&columns,NULL)!=SQLITE_OK)return false;
 while(sqlite3_step(columns)==SQLITE_ROW){const char* name=(const char*)sqlite3_column_text(columns,1);if(name && !strcmp(name,"mask"))mask=true;}sqlite3_finalize(columns);
 return mask || sqlite3_exec(App.db,"ALTER TABLE workspace_shell_v1 ADD COLUMN mask INTEGER NOT NULL DEFAULT 7",NULL,NULL,NULL)==SQLITE_OK;}
}
LRESULT CMainFrame::OnInitializeWorkspaceShell(WPARAM,LPARAM)
{
  if(m_workspaceShellReady)return 0;bool navigator=true,legacy=false;int theme=0,mask=0;
  if(WorkspaceShellSchema()){sqlite3_stmt* query=NULL;if(sqlite3_prepare_v2(App.db,"SELECT navigator,legacy,theme,mask FROM workspace_shell_v1 WHERE slot=1",-1,&query,NULL)==SQLITE_OK){if(sqlite3_step(query)==SQLITE_ROW){navigator=sqlite3_column_int(query,0)!=0;legacy=sqlite3_column_int(query,1)!=0;theme=sqlite3_column_int(query,2);mask=sqlite3_column_int(query,3);}sqlite3_finalize(query);}}
  const unsigned visible=(m_wndToolBar.IsWindowVisible()?1:0)|(m_wndGameToolBar.IsWindowVisible()?2:0)|(m_wndActivityToolBar.IsWindowVisible()?4:0);m_workspaceLegacyMask=visible?visible:mask>=1 && mask<=7?(unsigned)mask:7;
  if(!legacy){CMDIFrameWnd::ShowControlBar(&m_wndToolBar,FALSE,FALSE);CMDIFrameWnd::ShowControlBar(&m_wndGameToolBar,FALSE,FALSE);CMDIFrameWnd::ShowControlBar(&m_wndActivityToolBar,FALSE,FALSE);}
  else if(!visible){CMDIFrameWnd::ShowControlBar(&m_wndToolBar,m_workspaceLegacyMask&1,FALSE);CMDIFrameWnd::ShowControlBar(&m_wndGameToolBar,m_workspaceLegacyMask&2,FALSE);CMDIFrameWnd::ShowControlBar(&m_wndActivityToolBar,m_workspaceLegacyMask&4,FALSE);}
  workspace_style::SetTheme(theme==1?workspace_style::Theme::Light:theme==2?workspace_style::Theme::Dark:workspace_style::Theme::System);
  if(App.db){sqlite3_exec(App.db,"CREATE TABLE IF NOT EXISTS workspace_accent_v1(slot INTEGER PRIMARY KEY CHECK(slot=1),accent INTEGER NOT NULL)",NULL,NULL,NULL);sqlite3_stmt* query=NULL;
    if(sqlite3_prepare_v2(App.db,"SELECT accent FROM workspace_accent_v1 WHERE slot=1",-1,&query,NULL)==SQLITE_OK){if(sqlite3_step(query)==SQLITE_ROW)workspace_style::SetAccent(sqlite3_column_int(query,0)==1?workspace_style::Accent::Blue:workspace_style::Accent::Red);sqlite3_finalize(query);}}
  m_workspaceShellReady=true;m_workspaceShell->ShowNavigator(navigator);UpdateWorkspaceShell();return 0;
}
void CMainFrame::SetWorkspaceTabbedWorlds(bool enabled)
{
  // Let MFC measure and hit-test the wider labels itself.
  CMFCBaseTabCtrl::AFX_TAB_TEXT_MARGIN=workspace_style::Scale(GetSafeHwnd(),10);
  CMDITabInfo tabs;tabs.m_tabLocation=CMFCTabCtrl::LOCATION_TOP;tabs.m_style=CMFCTabCtrl::STYLE_3D;
  tabs.m_bTabIcons=FALSE;tabs.m_bTabCloseButton=FALSE;tabs.m_bActiveTabCloseButton=TRUE;
  tabs.m_bEnableTabSwap=TRUE;tabs.m_bAutoColor=FALSE;tabs.m_bFlatFrame=TRUE;tabs.m_nTabBorderSize=0;tabs.m_bDocumentMenu=TRUE;
  {WorkspaceModernResources resources;EnableMDITabbedGroups(enabled,tabs);}m_workspaceTabbedWorlds=enabled;if(!enabled){if(auto* child=MDIGetActive())MDIMaximize(child);}RecalcLayout();
  if(App.db){sqlite3_stmt* write=NULL;if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_window_mode_v1(slot,tabbed) VALUES(1,?)",-1,&write,NULL)==SQLITE_OK){sqlite3_bind_int(write,1,enabled?1:0);sqlite3_step(write);sqlite3_finalize(write);}}
}
void CMainFrame::SaveWorkspaceShell()
{
  if(!m_workspaceShellReady || !WorkspaceShellSchema())return;sqlite3_stmt* write=NULL;
  if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_shell_v1(slot,navigator,legacy,theme,mask) VALUES(1,?,?,?,?)",-1,&write,NULL)!=SQLITE_OK)return;
  const unsigned visible=(m_wndToolBar.IsWindowVisible()?1:0)|(m_wndGameToolBar.IsWindowVisible()?2:0)|(m_wndActivityToolBar.IsWindowVisible()?4:0);if(visible)m_workspaceLegacyMask=visible;
  sqlite3_bind_int(write,1,m_workspaceShell->NavigatorVisible());sqlite3_bind_int(write,2,visible!=0);sqlite3_bind_int(write,3,(int)workspace_style::GetTheme());sqlite3_bind_int(write,4,m_workspaceLegacyMask);sqlite3_step(write);sqlite3_finalize(write);
  if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_accent_v1(slot,accent) VALUES(1,?)",-1,&write,NULL)==SQLITE_OK){sqlite3_bind_int(write,1,(int)workspace_style::GetAccent());sqlite3_step(write);sqlite3_finalize(write);}
}
void CMainFrame::UpdateWorkspaceShell()
{
  if(!m_workspaceShellReady || !App.m_pWorldDocTemplate)return;workspace_shell::Snapshot snapshot;
  snapshot.world=(LPCSTR)m_workspaceShellWorld;snapshot.name=(LPCSTR)m_workspaceShellName;
  HWND focus=::GetFocus();const bool retain=m_workspaceShell->OwnsFocus();std::map<CString,std::vector<workspace_shell::Row>> channels;
  bool paneFocused=false;auto inspect=[&](CWorkspacePane* pane){std::vector<WorkspacePaneLayout> layouts;pane->GetLayouts(layouts,false);for(const auto& layout:layouts){workspace_shell::Row row;row.world=(LPCSTR)layout.connectionId;row.name=(LPCSTR)layout.name;row.label=layout.name=="Output"?L"Main transcript":WorkspaceShellWide(layout.name,true);row.channel=true;row.priority=WorkspaceTranscriptPreferences(layout.connectionId,layout.name).priority;row.unread=pane->UnreadCount(layout.connectionId,layout.name);row.mentions=pane->MentionCount(layout.connectionId,layout.name);channels[layout.connectionId].push_back(row);
    if(!retain && (focus==pane->GetSafeHwnd() || ::IsChild(pane->GetSafeHwnd(),focus) || (workspace_composer::BorrowedEditors().count(pane->SharedInputWindow()) && focus==pane->SharedInputWindow()))){snapshot.world=row.world;snapshot.name=row.name;paneFocused=true;}}};
  inspect(&m_workspacePane);for(auto* pane:m_workspaceSplits)inspect(pane);
  for(auto& group:channels)std::stable_sort(group.second.begin(),group.second.end(),[](const workspace_shell::Row& a,const workspace_shell::Row& b){if(a.priority!=b.priority)return a.priority>b.priority;if(a.label!=b.label)return a.label<b.label;return a.name<b.name;});
  if(!retain && !paneFocused){auto* child=MDIGetActive();auto* doc=child?DYNAMIC_DOWNCAST(CMUSHclientDoc,child->GetActiveDocument()):NULL;if(doc){snapshot.world=(LPCSTR)WorkspaceConnectionId(doc);snapshot.name.clear();}}
  bool found=false;
  for(POSITION pos=App.m_pWorldDocTemplate->GetFirstDocPosition();pos;){auto* doc=(CMUSHclientDoc*)App.m_pWorldDocTemplate->GetNextDoc(pos);workspace_shell::Row row;const CString identity=WorkspaceConnectionId(doc);row.world=(LPCSTR)identity;row.label=WorkspaceShellWide(doc->m_mush_name);if(row.label.empty())row.label=L"Untitled world";row.connected=doc->m_iConnectPhase==eConnectConnectedToMud;row.disconnecting=doc->m_iConnectPhase==eConnectDisconnecting;row.connecting=doc->m_iConnectPhase!=eConnectNotConnected && !row.connected && !row.disconnecting;row.logging=doc->m_logfile!=NULL;row.unread=(unsigned)max(0L,(long)doc->m_new_lines);
    WorkspaceCharacterIdentity(*this,*doc,row);
    workspace_composer::ForEachOriginalFrame(*doc,[&](CFrameWnd& frame){auto* child=DYNAMIC_DOWNCAST(CChildFrame,&frame);if(child)child->m_wndSplitter.SetComposerDestination(row.gameLabel,row.characterLabel,row.connected,doc->m_bEnableAutoSay?L"AutoSay (when applicable): \""+WorkspaceShellWide(doc->m_strAutoSayString,doc->m_bUTF_8)+L"\"":L"None (full command)");});
    auto present=[&](CWorkspacePane* pane){std::vector<WorkspacePaneLayout> layout;pane->GetLayouts(layout,false);if(!layout.empty() && layout[0].connectionId==identity)pane->SetPresentationIdentity(row.gameLabel,row.characterLabel);};
    present(&m_workspacePane);for(auto* pane:m_workspaceSplits)present(pane);
    if(row.world==snapshot.world)found=true;for(const auto& channel:channels[identity])row.mentions+=channel.mentions;snapshot.rows.push_back(row);for(const auto& channel:channels[identity])snapshot.rows.push_back(channel);}
  if(!found){snapshot.world.clear();snapshot.name.clear();}m_workspaceShellWorld=snapshot.world.c_str();m_workspaceShellName=snapshot.name.c_str();snapshot.legacy=m_wndToolBar.IsWindowVisible() || m_wndGameToolBar.IsWindowVisible() || m_wndActivityToolBar.IsWindowVisible();m_workspaceShell->Update(snapshot);
}
void CMainFrame::OnWorkspaceShellCommand(UINT command)
{WorkspaceShellAction(command==59037?workspace_shell::AccentRed:command==59038?workspace_shell::AccentBlue:command==59030?workspace_shell::ToggleWorlds:command==59031?workspace_shell::Legacy:command==59033?workspace_shell::ThemeLight:command==59034?workspace_shell::ThemeDark:command==59035?workspace_shell::OrganizeCharacter:command==59036?workspace_shell::ToggleWorldTabs:workspace_shell::ThemeSystem,(LPCSTR)m_workspaceShellWorld,(LPCSTR)m_workspaceShellName);}
void CMainFrame::OnUpdateWorkspaceShellCommand(CCmdUI* command)
{command->Enable(TRUE);if(command->m_nID==59037 || command->m_nID==59038)command->SetRadio(workspace_style::GetAccent()==(command->m_nID==59038?workspace_style::Accent::Blue:workspace_style::Accent::Red));else if(command->m_nID==59036)command->SetCheck(m_workspaceTabbedWorlds);else if(command->m_nID==59035)command->Enable(WorkspaceShellDocument((LPCSTR)m_workspaceShellWorld)!=NULL);else if(command->m_nID==59030)command->SetCheck(m_workspaceShellReady && m_workspaceShell->NavigatorVisible());else if(command->m_nID==59031)command->SetCheck(m_wndToolBar.IsWindowVisible() || m_wndGameToolBar.IsWindowVisible() || m_wndActivityToolBar.IsWindowVisible());else command->SetRadio((int)workspace_style::GetTheme()==(command->m_nID==59033?1:command->m_nID==59034?2:0));}
void CMainFrame::WorkspaceShellAction(int action,const std::string& world,const std::string& name)
{
  using namespace workspace_shell;
  if(action==AccentRed || action==AccentBlue){workspace_style::SetAccent(action==AccentBlue?workspace_style::Accent::Blue:workspace_style::Accent::Red);SaveWorkspaceShell();return;}
  if(action==Editor){if(WorkspaceShellActivate(*this,world))SendMessage(WM_COMMAND,59001);return;}
  if(action==Review){SendMessage(WM_COMMAND,59007);return;}
  if(action==ToggleWorldTabs){SetWorkspaceTabbedWorlds(!m_workspaceTabbedWorlds);return;}
  if(action==OrganizeCharacter){auto* doc=WorkspaceShellDocument(world);if(!doc)return;Row row;row.world=world;WorkspaceCharacterIdentity(*this,*doc,row);std::wstring game=row.gameLabel,character=row.characterLabel;
    if(!ShowCharacterLabelsDialog(GetSafeHwnd(),game,character))return;
    // The modal may run callbacks; labels are saved only for its original world.
    if(!WorkspaceShellDocument(world) || !WorkspaceCharacterSchema())return;sqlite3_stmt* write=NULL;
    if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_character_labels_v1(world_id,game_label,character_label) VALUES(?,?,?)",-1,&write,NULL)!=SQLITE_OK){AfxMessageBox("Character labels could not be saved.");return;}
    const CString key=WorkspaceCharacterKey(world.c_str());const std::string gameUtf8=WorkspaceShellUtf8(game),characterUtf8=WorkspaceShellUtf8(character);sqlite3_bind_text(write,1,key,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,gameUtf8.c_str(),(int)gameUtf8.size(),SQLITE_TRANSIENT);sqlite3_bind_text(write,3,characterUtf8.c_str(),(int)characterUtf8.size(),SQLITE_TRANSIENT);const int status=sqlite3_step(write);sqlite3_finalize(write);if(status!=SQLITE_DONE){AfxMessageBox("Character labels could not be saved.");return;}m_workspaceCharacterLabels[key]=std::make_pair(game,character);UpdateWorkspaceShell();return;}
  if(action==ToggleWorlds){m_workspaceShell->ToggleNavigator();SaveWorkspaceShell();return;}
  if(action==OpenWorld){SendMessage(WM_COMMAND,ID_FILE_OPEN);return;}
  if(action==Legacy){const bool show=!(m_wndToolBar.IsWindowVisible() || m_wndGameToolBar.IsWindowVisible() || m_wndActivityToolBar.IsWindowVisible());
    if(!show)m_workspaceLegacyMask=(m_wndToolBar.IsWindowVisible()?1:0)|(m_wndGameToolBar.IsWindowVisible()?2:0)|(m_wndActivityToolBar.IsWindowVisible()?4:0);
    CMDIFrameWnd::ShowControlBar(&m_wndToolBar,show && (m_workspaceLegacyMask&1),FALSE);CMDIFrameWnd::ShowControlBar(&m_wndGameToolBar,show && (m_workspaceLegacyMask&2),FALSE);CMDIFrameWnd::ShowControlBar(&m_wndActivityToolBar,show && (m_workspaceLegacyMask&4),FALSE);RecalcLayout();SaveWorkspaceShell();return;}
  if(action==ThemeSystem || action==ThemeLight || action==ThemeDark){workspace_style::SetTheme(action==ThemeLight?workspace_style::Theme::Light:action==ThemeDark?workspace_style::Theme::Dark:workspace_style::Theme::System);RecalcLayout();SaveWorkspaceShell();return;}
  if(action==Tools || action==Logging){CMenu menu;menu.CreatePopupMenu();auto* doc=WorkspaceShellDocument(world);const UINT unavailable=doc?0:MF_GRAYED;
    if(action==Logging){menu.AppendMenu(MF_STRING|unavailable,ID_FILE_LOGSESSION,doc && doc->m_logfile?"Stop logging":"Start logging...");menu.AppendMenu(MF_STRING|unavailable,59008,"Automatic filenames...");}
    else{menu.AppendMenu(MF_STRING|unavailable,59001,"PennMUSH editor...");menu.AppendMenu(MF_STRING,59002,"Agent access...");menu.AppendMenu(MF_STRING,59003,"Agent conversations...");menu.AppendMenu(MF_STRING,59004,"Scene director...");menu.AppendMenu(MF_STRING,59007,"Agent review and captures...");menu.AppendMenu(MF_STRING|unavailable,59035,"Game and character labels...");menu.AppendMenu(MF_STRING|(m_workspaceTabbedWorlds?MF_CHECKED:0),59036,"Show character document tabs");menu.AppendMenu(MF_SEPARATOR);menu.AppendMenu(MF_STRING,59031,"Show classic toolbars");CMenu appearance;appearance.CreatePopupMenu();appearance.AppendMenu(MF_STRING,59032,"Use Windows appearance");appearance.AppendMenu(MF_STRING,59033,"Light appearance");appearance.AppendMenu(MF_STRING,59034,"Dark appearance");appearance.AppendMenu(MF_SEPARATOR);appearance.AppendMenu(MF_STRING|(workspace_style::GetAccent()==workspace_style::Accent::Red?MF_CHECKED:0),59037,"Valyria red accent (default)");appearance.AppendMenu(MF_STRING|(workspace_style::GetAccent()==workspace_style::Accent::Blue?MF_CHECKED:0),59038,"Blue accent");menu.AppendMenu(MF_POPUP,(UINT_PTR)appearance.Detach(),"Appearance");}
    CPoint point;GetCursorPos(&point);if(auto* focused=GetFocus()){CRect rect;focused->GetWindowRect(rect);if(m_workspaceShell->OwnsFocus())point=CPoint(rect.left,rect.bottom);}
    const UINT command=menu.TrackPopupMenu(TPM_RETURNCMD|TPM_NONOTIFY|TPM_LEFTALIGN,point.x,point.y,this);if(!command)return;if(command==59036){WorkspaceShellAction(ToggleWorldTabs,world,name);return;}if(command==59035){WorkspaceShellAction(OrganizeCharacter,world,name);return;}if(command>=59030 && command<=59038){OnWorkspaceShellCommand(command);return;}
    if(command==59002 || command==59003 || command==59004 || command==59007){SendMessage(WM_COMMAND,command);return;}
    if(!WorkspaceShellActivate(*this,world))return;SendMessage(WM_COMMAND,command);UpdateWorkspaceShell();return;}
  if(!WorkspaceShellActivate(*this,world)){SetStatusMessage("The selected world changed or closed; no action was taken.");UpdateWorkspaceShell();return;}
  auto* doc=WorkspaceShellDocument(world);if(!doc)return;
  m_workspaceShellWorld=world.c_str();m_workspaceShellName=name.c_str();
  CWorkspacePane* selected=NULL;size_t selectedIndex=0;auto locate=[&](CWorkspacePane* pane){std::vector<WorkspacePaneLayout> layouts;pane->GetLayouts(layouts,false);for(size_t i=0;i<layouts.size();++i)if(world==(LPCSTR)layouts[i].connectionId && name==(LPCSTR)layouts[i].name){selected=pane;selectedIndex=i;}};locate(&m_workspacePane);for(auto* pane:m_workspaceSplits)locate(pane);
  if(action==Select && !selected){auto* child=DYNAMIC_DOWNCAST(CChildFrame,MDIGetActive());if(child){child->m_wndSplitter.ReleaseChannelEditor();::SetFocus(child->m_wndSplitter.CurrentInput());}}
  if(action==Select && selected){ShowControlBar(selected,TRUE,FALSE);selected->FocusTranscript(selectedIndex);}
  else if(action==Arrange && selected){ShowControlBar(selected,TRUE,FALSE);selected->ShowPlacementMenu();}
  else if(action==Arrange){SetStatusMessage("Choose a channel in the navigator to arrange its native pane.");}
  else if(action==Find && selected){ShowControlBar(selected,TRUE,FALSE);selected->FocusTranscript(selectedIndex);selected->FocusSearch();}
  else if(action==Find)SendMessage(WM_COMMAND,ID_DISPLAY_FIND);
  else if(action==Connect && doc->m_iConnectPhase==eConnectNotConnected)SendMessage(WM_COMMAND,ID_CONNECTION_CONNECT);
  else if(action==Disconnect && doc->m_iConnectPhase!=eConnectNotConnected && doc->m_iConnectPhase!=eConnectDisconnecting)SendMessage(WM_COMMAND,ID_CONNECTION_DISCONNECT);
  else if(action==Channels)SendMessage(WM_COMMAND,59006);
  UpdateWorkspaceShell();
}

namespace {
CString WorkspacePersistentWorld(const CString& identity)
{
  int runtime=identity.Find(":runtime:");return runtime>=0?identity.Left(runtime):identity;
}
bool WorkspaceLayoutSchema()
{
  if(!App.db) return false;
  return sqlite3_exec(App.db,
    "CREATE TABLE IF NOT EXISTS workspace_dock_forest_v3 (slot INTEGER PRIMARY KEY CHECK(slot=1),data BLOB NOT NULL);"
    "CREATE TABLE IF NOT EXISTS workspace_split_v2 (world_id TEXT NOT NULL,name TEXT NOT NULL,target_world TEXT NOT NULL,target_name TEXT NOT NULL,side INTEGER NOT NULL,depth INTEGER NOT NULL,percent INTEGER NOT NULL,PRIMARY KEY(world_id,name));"
    "CREATE TABLE IF NOT EXISTS workspace_tab_group_v2 (world_id TEXT NOT NULL,name TEXT NOT NULL,group_world TEXT NOT NULL,group_name TEXT NOT NULL,tab_order INTEGER NOT NULL,active INTEGER NOT NULL,PRIMARY KEY(world_id,name));"
    "CREATE TABLE IF NOT EXISTS workspace_pane_layout_v1 (world_id TEXT NOT NULL,name TEXT NOT NULL,detached INTEGER,floating INTEGER,alignment INTEGER,x INTEGER,y INTEGER,width INTEGER,height INTEGER,PRIMARY KEY(world_id,name));"
    "CREATE TABLE IF NOT EXISTS workspace_input_prefix_v1 (world_id TEXT NOT NULL,name TEXT NOT NULL,prefix TEXT NOT NULL,PRIMARY KEY(world_id,name));"
    "CREATE TABLE IF NOT EXISTS workspace_route_v1 (world_id TEXT NOT NULL,name TEXT NOT NULL,pattern TEXT NOT NULL,regex INTEGER,omit_main INTEGER,PRIMARY KEY(world_id,name,pattern,regex));",
    NULL,NULL,NULL)==SQLITE_OK;
}
std::string WorkspaceLayoutUtf8(const std::wstring& value)
{
  if(value.empty()) return std::string();
  int bytes=WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),NULL,0,NULL,NULL);
  std::string result(bytes,'\0');if(bytes)WideCharToMultiByte(CP_UTF8,0,value.data(),(int)value.size(),&result[0],bytes,NULL,NULL);return result;
}
std::wstring WorkspaceLayoutWide(const char* value)
{
  if(!value) return std::wstring();
  int bytes=(int)strlen(value);int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,bytes,NULL,0);
  std::wstring result(count,L'\0');if(count)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value,bytes,&result[0],count);return result;
}
bool WorkspaceReadLayout(const CString& identity,const CString& name,WorkspacePaneLayout& layout)
{
  if(!WorkspaceLayoutSchema())return false;
  sqlite3_stmt* query=NULL;
  if(sqlite3_prepare_v2(App.db,"SELECT l.detached,l.floating,l.alignment,l.x,l.y,l.width,l.height,g.group_world,g.group_name,g.tab_order,g.active FROM workspace_pane_layout_v1 l LEFT JOIN workspace_tab_group_v2 g ON l.world_id=g.world_id AND l.name=g.name WHERE l.world_id=? AND l.name=?",-1,&query,NULL)!=SQLITE_OK)return false;
  const CString world=WorkspacePersistentWorld(identity);
  sqlite3_bind_text(query,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(query,2,name,-1,SQLITE_TRANSIENT);
  bool found=sqlite3_step(query)==SQLITE_ROW;
  if(found)
    { layout.connectionId=identity;layout.name=name;layout.detached=sqlite3_column_int(query,0)!=0;
      layout.floating=sqlite3_column_int(query,1)!=0;layout.alignment=sqlite3_column_int(query,2);
      layout.x=sqlite3_column_int(query,3);layout.y=sqlite3_column_int(query,4);
      layout.width=max(220,min(1500,sqlite3_column_int(query,5)));layout.height=max(140,min(1000,sqlite3_column_int(query,6)));
      if(sqlite3_column_type(query,7)!=SQLITE_NULL){layout.groupConnectionId=(const char*)sqlite3_column_text(query,7);layout.groupName=(const char*)sqlite3_column_text(query,8);layout.tabOrder=max(0,min(127,sqlite3_column_int(query,9)));layout.active=sqlite3_column_int(query,10)!=0;}
      else{layout.groupConnectionId=world;layout.groupName=name;} }
  sqlite3_finalize(query);
  if(found && sqlite3_prepare_v2(App.db,"SELECT target_world,target_name,side,depth,percent FROM workspace_split_v2 WHERE world_id=? AND name=?",-1,&query,NULL)==SQLITE_OK)
    {sqlite3_bind_text(query,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(query,2,name,-1,SQLITE_TRANSIENT);
     if(sqlite3_step(query)==SQLITE_ROW){layout.splitConnectionId=(const char*)sqlite3_column_text(query,0);layout.splitName=(const char*)sqlite3_column_text(query,1);layout.splitAlignment=sqlite3_column_int(query,2);layout.splitDepth=max(0,min(128,sqlite3_column_int(query,3)));layout.splitPercent=max(10,min(90,sqlite3_column_int(query,4)));}sqlite3_finalize(query);}
  return found;
}
void WorkspacePositionPane(CMainFrame& frame,CWorkspacePane& pane,const WorkspacePaneLayout& layout)
{
  WorkspacePaneLayout safe=layout;
  pane.ApplyLayout(safe);
  if(layout.floating)
    {
    RECT proposed={layout.x,layout.y,layout.x+layout.width,layout.y+layout.height};
    HMONITOR monitor=MonitorFromRect(&proposed,MONITOR_DEFAULTTONEAREST);
    MONITORINFO info={sizeof(info)};
    CPoint point(layout.x,layout.y);
    if(GetMonitorInfo(monitor,&info))
      {safe.width=max(220,min(safe.width,info.rcWork.right-info.rcWork.left-32));
       safe.height=max(140,min(safe.height,info.rcWork.bottom-info.rcWork.top-48));pane.ApplyLayout(safe);
       point.x=max(info.rcWork.left,min(point.x,info.rcWork.right-safe.width-32));
       point.y=max(info.rcWork.top,min(point.y,info.rcWork.bottom-safe.height-48));}
    pane.FloatPane(CRect(point,CSize(safe.width,safe.height)),DM_SHOW);
    }
  else
    {
    UINT dock=AFX_IDW_DOCKBAR_RIGHT;
    if(layout.alignment==CBRS_ALIGN_LEFT)dock=AFX_IDW_DOCKBAR_LEFT;
    else if(layout.alignment==CBRS_ALIGN_TOP)dock=AFX_IDW_DOCKBAR_TOP;
    else if(layout.alignment==CBRS_ALIGN_BOTTOM)dock=AFX_IDW_DOCKBAR_BOTTOM;
    frame.DockControlBar(&pane,dock);
    }
}
}

bool CMainFrame::OpenWorkspaceTranscript(const CString& connectionId, const CString& name)
{
  if(m_workspaceClosedConnections.find(connectionId)!=m_workspaceClosedConnections.end())return false;
  RestoreWorkspaceRoutes(connectionId);
  CWorkspacePane* target=&m_workspacePane;
  if(target->HasTranscript(connectionId,name))
    { target->OpenTranscript(connectionId,name);ShowControlBar(target,TRUE,FALSE);return true; }
  for(size_t i=0;i<m_workspaceSplits.size();++i)
    if(m_workspaceSplits[i]->HasTranscript(connectionId,name))
      { m_workspaceSplits[i]->OpenTranscript(connectionId,name);ShowControlBar(m_workspaceSplits[i],TRUE,FALSE);return true; }
  WorkspacePaneLayout layout;
  const bool restore=WorkspaceReadLayout(connectionId,name,layout);
  if(!target->IsEmpty())
    {
    for(size_t i=0;i<m_workspaceSplits.size();++i)
      if(m_workspaceSplits[i]->IsEmpty()) { target=m_workspaceSplits[i];break; }
    if(target==&m_workspacePane && m_workspaceSplits.size()<128)
      { CWorkspacePane* extra=new CWorkspacePane;
        if(extra->Create(this,(UINT)(0xE900+m_workspaceSplits.size())))
          {extra->EnableDocking(CBRS_ALIGN_ANY);extra->SetActionHandler(ActivateWorkspaceAction);extra->SetInputHandler(WorkspaceSendHumanInput);extra->SetSharedInputHandler(WorkspaceSelectSharedInput);extra->SetPruneHandler(WorkspacePanePruned);extra->SetConnectionHandler(WorkspaceIsConnectionLive);extra->SetInputErrorHandler(WorkspaceHumanInputError);
            m_workspaceSplits.push_back(extra);target=extra; }
        else delete extra; }
    }
  if (!target->OpenTranscript(connectionId,name)) return false;
  target->SetPreferences(connectionId,name,WorkspaceTranscriptPreferences(connectionId,name));
  if(WorkspaceLayoutSchema())
    {sqlite3_stmt* query=NULL;
     if(sqlite3_prepare_v2(App.db,"SELECT prefix FROM workspace_input_prefix_v1 WHERE world_id=? AND name=?",-1,&query,NULL)==SQLITE_OK)
       {const CString world=WorkspacePersistentWorld(connectionId);sqlite3_bind_text(query,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(query,2,name,-1,SQLITE_TRANSIENT);
        if(sqlite3_step(query)==SQLITE_ROW)target->SetInputPrefix(connectionId,name,WorkspaceLayoutWide((const char*)sqlite3_column_text(query,0)));
        sqlite3_finalize(query);}}
  const bool exactLayout=RestoreWorkspaceDockState(*target,connectionId,name);
  if(exactLayout) { }
  else if(restore) { WorkspacePositionPane(*this,*target,layout);RestoreWorkspacePaneGroup(*target,layout); }
  else if(target!=&m_workspacePane && !m_workspacePane.IsEmpty())target->AttachToTabWnd(&m_workspacePane,DM_SHOW);
  else DockPane(target,AFX_IDW_DOCKBAR_RIGHT);
  if(!exactLayout)ShowControlBar(target, TRUE, FALSE);
  RecalcLayout();
  return true;
}
bool CMainFrame::AppendWorkspaceRecord(const CString& name, const WorkspaceRecord& record)
{
  while(m_workspacePane.RenderBatchDepth()<m_workspaceRenderBatchDepth)m_workspacePane.BeginRenderBatch();
  bool appended=m_workspacePane.AppendRecord(name,record);
  for(size_t i=0;i<m_workspaceSplits.size();++i){
    while(m_workspaceSplits[i]->RenderBatchDepth()<m_workspaceRenderBatchDepth)m_workspaceSplits[i]->BeginRenderBatch();
    appended=m_workspaceSplits[i]->AppendRecord(name,record)||appended;
  }
  return appended;
}
void CMainFrame::BeginWorkspaceRenderBatch()
{
  ++m_workspaceRenderBatchDepth;m_workspacePane.BeginRenderBatch();
  for(auto* pane:m_workspaceSplits)pane->BeginRenderBatch();
}
void CMainFrame::EndWorkspaceRenderBatch()
{
  if(!m_workspaceRenderBatchDepth)return;
  --m_workspaceRenderBatchDepth;m_workspacePane.EndRenderBatch();
  for(auto* pane:m_workspaceSplits)pane->EndRenderBatch();
}
void CMainFrame::CloseWorkspaceConnection(const CString& connectionId)
{
  SaveWorkspaceLayouts();
  m_workspaceClosedConnections.insert(connectionId);
  m_workspaceRestored.erase(connectionId);
  for(size_t i=m_workspaceRoutes.size();i>0;--i)
    if(m_workspaceRoutes[i-1].connectionId==connectionId) m_workspaceRoutes.erase(m_workspaceRoutes.begin()+i-1);
  m_workspacePane.CloseConnection(connectionId);
  if(m_workspacePane.IsEmpty()){ShowControlBar(&m_workspacePane,FALSE,FALSE);m_workspaceRestoreGroups.erase(&m_workspacePane);m_workspaceRestoredSplits.erase(&m_workspacePane);}
  for(size_t i=0;i<m_workspaceSplits.size();++i)
    { m_workspaceSplits[i]->CloseConnection(connectionId);
      if(m_workspaceSplits[i]->IsEmpty()){ShowControlBar(m_workspaceSplits[i],FALSE,FALSE);m_workspaceRestoreGroups.erase(m_workspaceSplits[i]);m_workspaceRestoredSplits.erase(m_workspaceSplits[i]);} }
  m_workspaceDockSession->Observe(WorkspaceDockEntries());
}
void CMainFrame::OnWorkspaceTranscript()
{
  CMDIChildWnd* child=MDIGetActive();
  CMUSHclientDoc* doc=child?DYNAMIC_DOWNCAST(CMUSHclientDoc,child->GetActiveDocument()):NULL;
  if (!doc) { AfxMessageBox("Open a world before creating its transcript."); return; }
  const CString identity=WorkspaceConnectionId(doc);
  bool existed=m_workspacePane.HasTranscript(identity,"Output");
  for(size_t i=0;i<m_workspaceSplits.size();++i)existed=m_workspaceSplits[i]->HasTranscript(identity,"Output")||existed;
  if(OpenWorkspaceTranscript(identity,"Output") && !existed)WorkspaceRebuildOutput(doc);
}

void CMainFrame::OnWorkspaceTool(UINT id)
{
  OpenWorkspaceTool(*this,id);
}

namespace {
std::wstring LogSetupWide(const CString& value)
{
  int count=MultiByteToWideChar(CP_ACP,0,value,value.GetLength(),NULL,0);
  std::wstring result(count,L'\0');
  if(count)MultiByteToWideChar(CP_ACP,0,value,value.GetLength(),&result[0],count);
  return result;
}
bool LogSetupNarrow(const std::wstring& value,CString& result)
{
  if(value.empty()){result.Empty();return true;}
  BOOL substituted=FALSE;
  int count=WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,value.data(),(int)value.size(),NULL,0,NULL,&substituted);
  if(!count || substituted)return false;
  char* buffer=result.GetBuffer(count);
  int written=WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,value.data(),(int)value.size(),buffer,count,NULL,&substituted);
  result.ReleaseBuffer(written);
  return written==count && !substituted;
}
CString LogSetupVariable(CMUSHclientDoc* document,const CString& plugin,const char* name)
{
  VARIANT value=document->GetPluginVariable(plugin,name);
  CString result;
  if(value.vt==VT_BSTR && value.bstrVal)result=CString(value.bstrVal);
  VariantClear(&value);
  return result;
}
}
void CMainFrame::OnUpdateWorkspaceLogStartSetup(CCmdUI* command)
{
  CMDIChildWnd* child=MDIGetActive();
  command->Enable(child && DYNAMIC_DOWNCAST(CMUSHclientDoc,child->GetActiveDocument()));
}
void CMainFrame::OnWorkspaceLogStartSetup()
{
  CMDIChildWnd* child=MDIGetActive();
  CMUSHclientDoc* doc=child?DYNAMIC_DOWNCAST(CMUSHclientDoc,child->GetActiveDocument()):NULL;
  if(!doc){AfxMessageBox("Open a world before setting up its logging.");return;}
  const __int64 documentID=doc->m_iUniqueDocumentNumber;
  LogStartSetupOptions options;
  options.worldName=LogSetupWide(doc->m_mush_name);
  options.script=LogSetupWide(doc->m_strLogStartScript);
  options.plugin=LogSetupWide(doc->m_strLogStartPlugin);
  options.defaultFolder=LogSetupWide(App.m_strDefaultLogFileDirectory);
  if(options.defaultFolder.empty())options.defaultFolder=L"logs";
  if(!(options.defaultFolder.size()>2 && options.defaultFolder[1]==L':') &&
     options.defaultFolder.substr(0,2)!=L"\\\\")
    {
    wchar_t executable[32768]={};GetModuleFileNameW(NULL,executable,32768);
    std::wstring folder=executable;folder=folder.substr(0,folder.find_last_of(L"\\/")+1);
    options.defaultFolder=folder+options.defaultFolder;
    }
  for(PluginListIterator plugin=doc->m_PluginList.begin();plugin!=doc->m_PluginList.end();++plugin)
    if((*plugin)->m_bEnabled)
      {
      LogStartSetupPlugin choice;
      choice.name=LogSetupWide((*plugin)->m_strName);choice.id=LogSetupWide((*plugin)->m_strID);
      CString saved=LogSetupVariable(doc,(*plugin)->m_strID,"scripted_log_configuration");
      choice.namingConfiguration=std::string(saved,saved.GetLength());
      options.plugins.push_back(choice);
      }
  for(;;)
    {
  if(!ShowLogStartSetup(m_hWnd,options))return;
  // The modal message loop can run scripts that close a world. Locate the
  // original document afresh; a newly active or reused document is not a target.
  CMUSHclientDoc* original=NULL;
  for(POSITION position=App.m_pWorldDocTemplate->GetFirstDocPosition();position;)
    {
    CMUSHclientDoc* candidate=(CMUSHclientDoc*)App.m_pWorldDocTemplate->GetNextDoc(position);
    if(candidate->m_iUniqueDocumentNumber==documentID){original=candidate;break;}
    }
  if(!original){AfxMessageBox("The original world was closed. Logging settings were not saved.");return;}
  CString script,pluginID;
  if(!LogSetupNarrow(options.script,script) || !LogSetupNarrow(options.plugin,pluginID))
    {AfxMessageBox("The function name contains characters this world's script interface cannot represent. Logging settings were not saved.");return;}
  if(script.IsEmpty())pluginID.Empty();
  if(!pluginID.IsEmpty())
    {
    bool enabled=false;
    for(PluginListIterator plugin=original->m_PluginList.begin();plugin!=original->m_PluginList.end();++plugin)
      if((*plugin)->m_bEnabled && (*plugin)->m_strID.CompareNoCase(pluginID)==0){enabled=true;break;}
    if(!enabled){AfxMessageBox("The selected plugin is no longer enabled in the original world. Logging settings were not saved.");return;}
    }
  if(options.saveNamingConfiguration && script=="StartLog" &&
     pluginID.CompareNoCase("7c1e480cf1d24cb28f9a1062")==0)
    {
    const CString data(options.namingConfiguration.data(),(int)options.namingConfiguration.size());
    const long status=original->CallPlugin(pluginID,"ConfigureLogNames",data);
    // Plugin state saving can pump callbacks. Resolve the original world and
    // enabled plugin again before checking its result or applying the hook.
    original=NULL;
    for(POSITION position=App.m_pWorldDocTemplate->GetFirstDocPosition();position;)
      {
      CMUSHclientDoc* candidate=(CMUSHclientDoc*)App.m_pWorldDocTemplate->GetNextDoc(position);
      if(candidate->m_iUniqueDocumentNumber==documentID){original=candidate;break;}
      }
    if(!original){AfxMessageBox("The original world closed. Its logging hook was not changed.");return;}
    CPlugin* selected=original->GetPlugin(pluginID);
    if(status!=eOK || !selected || !selected->m_bEnabled ||
       LogSetupVariable(original,pluginID,"scripted_log_configuration_result")!="ok" ||
       LogSetupVariable(original,pluginID,"scripted_log_configuration")!=data)
      {
      AfxMessageBox("Automatic filename settings could not be saved. Check the world's message for a rule or folder error. Your entered settings have been kept; reopen automatic file names to correct them, or Cancel. The logging hook was not changed.");
      continue;
      }
    }
  if(original->m_strLogStartScript!=script || original->m_strLogStartPlugin!=pluginID)
    {
    original->m_strLogStartScript=script;
    original->m_strLogStartPlugin=pluginID;
    original->SetModifiedFlag(TRUE);
    }
  return;
    }
}

void CMainFrame::OnWorkspaceFloatTranscript()
{
  CWorkspacePane* selected=NULL;
  CWnd* focus=GetFocus();
  for(CWnd* parent=focus;parent;parent=parent->GetParent())
    if((selected=DYNAMIC_DOWNCAST(CWorkspacePane,parent))!=NULL)break;
  if(!selected && focus){
    auto owns=[&](CWorkspacePane* pane){if(workspace_composer::BorrowedEditors().count(pane->SharedInputWindow()) && pane->SharedInputWindow()==focus->GetSafeHwnd())selected=pane;};
    owns(&m_workspacePane);for(auto* pane:m_workspaceSplits)owns(pane);
  }
  if(!selected && !m_workspacePane.IsEmpty())selected=&m_workspacePane;
  if(!selected)return;
  CRect bounds;GetWindowRect(bounds);FloatWorkspacePane(*selected,CRect(bounds.TopLeft()+CPoint(80,120),CSize(500,350)));
  selected->ShowPane(TRUE,FALSE,TRUE);RecalcLayout();
}

bool CMainFrame::HostWorkspaceTool(HWND toolWindow,const CString& title)
{
  if (!::IsWindow(toolWindow)) return false;
  CWorkspacePane* host=NULL;
  for(size_t i=0;i<m_workspaceTools.size();++i)
    {
    if(::GetParent(toolWindow)==m_workspaceTools[i]->m_hWnd)
      { ShowControlBar(m_workspaceTools[i],TRUE,FALSE); return true; }
    if(!host && !m_workspaceTools[i]->IsToolHost())host=m_workspaceTools[i];
    }
  bool created=false;
  if(!host)
    {if(m_workspaceTools.size()>=32)return false;
     host=new CWorkspacePane;if(!host->Create(this,(UINT)(0xE830+m_workspaceTools.size()))){delete host;return false;}
     host->EnableDocking(CBRS_ALIGN_ANY);created=true;}
  if(!host->HostTool(toolWindow,title)){if(created){host->DestroyWindow();delete host;}return false;}
  if(created)m_workspaceTools.push_back(host);
  m_workspaceRestoreGroups.erase(host);m_workspaceRestoredSplits.erase(host);
  WorkspacePaneLayout layout;const bool exactLayout=RestoreWorkspaceDockState(*host,"@tools",title);
  if(!exactLayout){if(WorkspaceReadLayout("@tools",title,layout)){WorkspacePositionPane(*this,*host,layout);RestoreWorkspacePaneGroup(*host,layout);}else DockControlBar(host,AFX_IDW_DOCKBAR_RIGHT);ShowControlBar(host,TRUE,FALSE);}RecalcLayout();
  return true;
}

namespace {
bool WorkspaceSpawnSchema()
{
  return App.db && sqlite3_exec(App.db,
    "CREATE TABLE IF NOT EXISTS workspace_spawn_preferences_v1(world_id TEXT NOT NULL,name TEXT NOT NULL,mention TEXT NOT NULL,priority INTEGER NOT NULL,shortcut INTEGER NOT NULL,PRIMARY KEY(world_id,name));"
    "CREATE TABLE IF NOT EXISTS workspace_route_images_v1(world_id TEXT NOT NULL,name TEXT NOT NULL,pattern TEXT NOT NULL,regex INTEGER NOT NULL,following_images INTEGER NOT NULL,PRIMARY KEY(world_id,name,pattern,regex));",NULL,NULL,NULL)==SQLITE_OK;
}
}
bool CMainFrame::AddWorkspaceRoute(const WorkspaceRoute& source,CString& error)
{
  WorkspaceRoute route=source;if(!route.Compile(error))return false;
  bool existing=false;for(const auto& item:m_workspaceRoutes)
    if(item.connectionId==route.connectionId && item.transcriptName==route.transcriptName && item.pattern==route.pattern && item.regex==route.regex)existing=true;
  if(m_workspaceRoutes.size()>=64 && !existing){error="Maximum of 64 workspace routes reached.";return false;}
  if(!WorkspaceLayoutSchema() || !WorkspaceSpawnSchema()){error="Transcript settings storage is unavailable.";return false;}
  ClearWorkspaceImageAssociation(route.connectionId);
  if(!OpenWorkspaceTranscript(route.connectionId,route.transcriptName)){error="Unable to create the transcript pane.";return false;}
  if(sqlite3_exec(App.db,"SAVEPOINT workspace_route_save",NULL,NULL,NULL)!=SQLITE_OK){error="Route settings could not be saved.";return false;}
  const CString world=WorkspacePersistentWorld(route.connectionId);const std::string pattern=WorkspaceLayoutUtf8(route.pattern);
  auto save=[&](const char* sql,bool images){sqlite3_stmt* write=NULL;
    if(sqlite3_prepare_v2(App.db,sql,-1,&write,NULL)!=SQLITE_OK)return false;
    sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,route.transcriptName,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,3,pattern.c_str(),(int)pattern.size(),SQLITE_TRANSIENT);sqlite3_bind_int(write,4,route.regex);sqlite3_bind_int(write,5,images?route.followingImages:route.omitFromMain);
    const int result=sqlite3_step(write);sqlite3_finalize(write);return result==SQLITE_DONE;};
  bool saved=save("INSERT OR REPLACE INTO workspace_route_v1(world_id,name,pattern,regex,omit_main) VALUES(?,?,?,?,?)",false) &&
    save("INSERT OR REPLACE INTO workspace_route_images_v1 VALUES(?,?,?,?,?)",true);
  if(saved)saved=sqlite3_exec(App.db,"RELEASE workspace_route_save",NULL,NULL,NULL)==SQLITE_OK;
  if(!saved){sqlite3_exec(App.db,"ROLLBACK TO workspace_route_save; RELEASE workspace_route_save",NULL,NULL,NULL);error="Route settings could not be saved. The previous rule remains active.";return false;}
  for(auto& item:m_workspaceRoutes)if(item.connectionId==route.connectionId && item.transcriptName==route.transcriptName && item.pattern==route.pattern && item.regex==route.regex){item=route;return true;}
  m_workspaceRoutes.push_back(route);return true;
}
bool CMainFrame::RouteWorkspaceRecord(const WorkspaceRecord& record)
{
  if(record.connectionId.IsEmpty() || m_workspaceClosedConnections.find(record.connectionId)!=m_workspaceClosedConnections.end())return false;
  RestoreWorkspaceRoutes(record.connectionId); // cold-start routes exist before the first completed record is matched
  bool omit=false;
  std::set<CString> delivered;
  std::map<std::string,bool> following;
  const std::string connection=(LPCTSTR)record.connectionId;
  auto images=record.serverOutput && record.imageOnly?m_workspaceImages.Take(connection,record.sequence,GetTickCount64()):std::vector<workspace_spawn::Recipient>();
  if(!record.serverOutput || !record.imageOnly)m_workspaceImages.Clear(connection);
  for(size_t i=0;i<m_workspaceRoutes.size();++i)
    {
    const WorkspaceRoute& route=m_workspaceRoutes[i];
    if(!route.Matches(record)) continue;
    bool accepted=delivered.find(route.transcriptName)!=delivered.end();
    if(!accepted)
      { accepted=AppendWorkspaceRecord(route.transcriptName,record);
        if(accepted)WorkspacePublishToPane(record.connectionId,route.transcriptName,record); }
    if(accepted) { delivered.insert(route.transcriptName);omit=omit||route.omitFromMain;
      if(record.serverOutput && !record.imageOnly && route.followingImages)following[(LPCTSTR)route.transcriptName]=following[(LPCTSTR)route.transcriptName]||route.omitFromMain; }
    }
  for(const auto& recipient:images){
    const CString name=recipient.name.c_str();bool accepted=delivered.count(name)!=0;
    if(!accepted){accepted=AppendWorkspaceRecord(name,record);if(accepted)WorkspacePublishToPane(record.connectionId,name,record);}
    if(accepted){delivered.insert(name);omit=omit||recipient.hide;}
  }
  if(!following.empty()){
    std::vector<workspace_spawn::Recipient> recipients;
    for(const auto& entry:following){workspace_spawn::Recipient item;item.name=entry.first;item.hide=entry.second;recipients.push_back(item);}
    m_workspaceImages.Start(connection,record.sequence,GetTickCount64(),recipients);
  }
  return omit;
}

void CMainFrame::RestoreWorkspacePaneGroup(CWorkspacePane& pane,const WorkspacePaneLayout& layout)
{
  if(layout.groupConnectionId.IsEmpty() || layout.groupName.IsEmpty())return;
  m_workspaceRestoreGroups[&pane]=layout;
  for(std::map<CWorkspacePane*,WorkspacePaneLayout>::iterator it=m_workspaceRestoreGroups.begin();it!=m_workspaceRestoreGroups.end();++it)
    if(it->first!=&pane && it->first->GetSafeHwnd() && (!it->first->IsEmpty() || it->first->IsToolHost()) && it->second.groupConnectionId==layout.groupConnectionId && it->second.groupName==layout.groupName)
      {pane.AttachToTabWnd(it->first,DM_SHOW,FALSE);break;}
  CBaseTabbedPane* group=pane.GetParentTabbedPane();if(group)
  {
  CMFCBaseTabCtrl* tabs=group->GetUnderlyingWindow();
  std::vector<std::pair<int,CWorkspacePane*> > order;
  for(int i=0;i<tabs->GetTabsNum();++i)
    {CWorkspacePane* leaf=DYNAMIC_DOWNCAST(CWorkspacePane,tabs->GetTabWnd(i));std::map<CWorkspacePane*,WorkspacePaneLayout>::iterator saved=m_workspaceRestoreGroups.find(leaf);if(saved!=m_workspaceRestoreGroups.end())order.push_back(std::make_pair(saved->second.tabOrder,leaf));}
  std::stable_sort(order.begin(),order.end());
  for(size_t i=0;i<order.size();++i)tabs->MoveTab(tabs->GetTabFromHwnd(order[i].second->m_hWnd),(int)i);
  for(size_t i=0;i<order.size();++i)if(m_workspaceRestoreGroups[order[i].second].active)tabs->SetActiveTab((int)i);
  }
  std::vector<std::pair<int,CWorkspacePane*> > pending;
  for(std::map<CWorkspacePane*,WorkspacePaneLayout>::iterator it=m_workspaceRestoreGroups.begin();it!=m_workspaceRestoreGroups.end();++it)
    if(!it->second.splitName.IsEmpty() && m_workspaceRestoredSplits.find(it->first)==m_workspaceRestoredSplits.end())pending.push_back(std::make_pair(it->second.splitDepth,it->first));
  std::stable_sort(pending.begin(),pending.end());
  for(size_t i=0;i<pending.size();++i)
    {
    CWorkspacePane* leaf=pending[i].second;const WorkspacePaneLayout& saved=m_workspaceRestoreGroups[leaf];
    if(saved.splitAlignment!=CBRS_ALIGN_RIGHT && saved.splitAlignment!=CBRS_ALIGN_BOTTOM)continue;
    if(!leaf->GetSafeHwnd() || (leaf->IsEmpty() && !leaf->IsToolHost()))continue;
    // Corrupt or manually edited portable layouts must never form dock cycles.
    CString nextWorld=saved.splitConnectionId,nextName=saved.splitName;bool cycle=false;
    const CString originalWorld=WorkspacePersistentWorld(saved.connectionId);
    for(size_t hop=0;hop<128 && !nextName.IsEmpty();++hop)
      {if(nextWorld==originalWorld && nextName==saved.name){cycle=true;break;}
       bool found=false;for(std::map<CWorkspacePane*,WorkspacePaneLayout>::iterator path=m_workspaceRestoreGroups.begin();path!=m_workspaceRestoreGroups.end();++path)
         if(WorkspacePersistentWorld(path->second.connectionId)==nextWorld && path->second.name==nextName){nextWorld=path->second.splitConnectionId;nextName=path->second.splitName;found=true;break;}
       if(!found)break;if(hop==127)cycle=true;}
    if(cycle)continue;
    for(std::map<CWorkspacePane*,WorkspacePaneLayout>::iterator it=m_workspaceRestoreGroups.begin();it!=m_workspaceRestoreGroups.end();++it)
      {const WorkspacePaneLayout& target=it->second;if(it->first==leaf || WorkspacePersistentWorld(target.connectionId)!=saved.splitConnectionId || target.name!=saved.splitName)continue;
       CDockablePane* owner=leaf->GetParentTabbedPane()?static_cast<CDockablePane*>(leaf->GetParentTabbedPane()):leaf;
       CDockablePane* anchor=it->first->GetParentTabbedPane()?static_cast<CDockablePane*>(it->first->GetParentTabbedPane()):it->first;
       if(owner==anchor)break;CRect bounds;anchor->GetWindowRect(bounds);CRect desired=bounds;
       if(saved.splitAlignment==CBRS_ALIGN_BOTTOM)desired.top=bounds.bottom-bounds.Height()*saved.splitPercent/100;else desired.left=bounds.right-bounds.Width()*saved.splitPercent/100;
       if(owner->DockToWindow(anchor,saved.splitAlignment,desired))m_workspaceRestoredSplits.insert(leaf);break;}
    }
}

std::vector<workspace_dock::Entry> CMainFrame::WorkspaceDockEntries() const
{
  std::vector<workspace_dock::Entry> entries;
  const auto add=[&](const CWorkspacePane& pane,bool detached){
    std::vector<WorkspacePaneLayout> layouts;pane.GetLayouts(layouts,detached);
    for(const auto& layout:layouts){workspace_dock::Entry entry;
      entry.key.world=(LPCSTR)WorkspacePersistentWorld(layout.connectionId);entry.key.name=(LPCSTR)layout.name;
      entry.pane=const_cast<CWorkspacePane*>(&pane);entries.push_back(entry);}
  };
  add(m_workspacePane,false);for(const auto* pane:m_workspaceSplits)add(*pane,true);for(const auto* pane:m_workspaceTools)add(*pane,true);return entries;
}
void CMainFrame::LoadWorkspaceDockState()
{
  if(m_workspaceDockSession->loaded)return;
  m_workspaceDockSession->loaded=true;
  if(!WorkspaceLayoutSchema())return;
  sqlite3_stmt* query=NULL;
  if(sqlite3_prepare_v2(App.db,"SELECT CASE WHEN length(data)<=262144 THEN data ELSE NULL END FROM workspace_dock_forest_v3 WHERE slot=1",-1,&query,NULL)!=SQLITE_OK){m_workspaceDockSession->Load("");return;}
  const int read=sqlite3_step(query);
  if(read==SQLITE_ROW){const char* data=(const char*)sqlite3_column_blob(query,0);const int size=sqlite3_column_bytes(query,0);
    m_workspaceDockSession->Load(data && size>0?std::string(data,size):std::string());}
  else if(read!=SQLITE_DONE)m_workspaceDockSession->Load("");
  sqlite3_finalize(query);
}
bool CMainFrame::RestoreWorkspaceDockState(CWorkspacePane& pane,const CString& world,const CString& name)
{
  LoadWorkspaceDockState();workspace_dock::Key key={(LPCSTR)WorkspacePersistentWorld(world),(LPCSTR)name};
  if(!m_workspaceDockSession->Contains(key))return false;
  // An unavailable sibling is projected away. A manually rearranged component
  // opts out of later joins, so opening another world never moves that reader.
  DockPane(&pane,AFX_IDW_DOCKBAR_RIGHT);pane.ShowPane(TRUE,FALSE,FALSE);
  const auto result=m_workspaceDockSession->ArriveResult(*this,key,WorkspaceDockEntries());
  // Manual opt-out is handled as a visible standalone pane. Native restoration
  // failure instead permits the legacy placement fallback below the caller.
  return result==workspace_dock::Session::Restored || result==workspace_dock::Session::Standalone;
}
void CMainFrame::SaveWorkspaceLayouts()
{
  if(!WorkspaceLayoutSchema()) return;
  LoadWorkspaceDockState();
  const std::string forest=m_workspaceDockSession->Save(WorkspaceDockEntries());
  if(!forest.empty()){
    sqlite3_stmt* snapshot=NULL;
    if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_dock_forest_v3(slot,data) VALUES(1,?)",-1,&snapshot,NULL)==SQLITE_OK){
      sqlite3_bind_blob(snapshot,1,forest.data(),(int)forest.size(),SQLITE_TRANSIENT);sqlite3_step(snapshot);sqlite3_finalize(snapshot);}
  }
  std::vector<WorkspacePaneLayout> layouts;m_workspacePane.GetLayouts(layouts,false);
  for(size_t i=0;i<m_workspaceSplits.size();++i)m_workspaceSplits[i]->GetLayouts(layouts,true);
  for(size_t i=0;i<m_workspaceTools.size();++i)m_workspaceTools[i]->GetLayouts(layouts,true);
  sqlite3_stmt* write=NULL;
  if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_pane_layout_v1(world_id,name,detached,floating,alignment,x,y,width,height) VALUES(?,?,?,?,?,?,?,?,?)",-1,&write,NULL)!=SQLITE_OK)return;
  for(size_t i=0;i<layouts.size();++i)
    {
    const WorkspacePaneLayout& layout=layouts[i];const CString world=WorkspacePersistentWorld(layout.connectionId);
    sqlite3_reset(write);sqlite3_clear_bindings(write);sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,layout.name,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(write,3,layout.detached);sqlite3_bind_int(write,4,layout.floating);sqlite3_bind_int(write,5,layout.alignment);
    sqlite3_bind_int(write,6,layout.x);sqlite3_bind_int(write,7,layout.y);sqlite3_bind_int(write,8,layout.width);sqlite3_bind_int(write,9,layout.height);sqlite3_step(write);
    }
  sqlite3_finalize(write);
  if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_tab_group_v2(world_id,name,group_world,group_name,tab_order,active) VALUES(?,?,?,?,?,?)",-1,&write,NULL)!=SQLITE_OK)return;
  for(size_t i=0;i<layouts.size();++i)
    {const WorkspacePaneLayout& layout=layouts[i];const CString world=WorkspacePersistentWorld(layout.connectionId),groupWorld=WorkspacePersistentWorld(layout.groupConnectionId);
     sqlite3_reset(write);sqlite3_clear_bindings(write);sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,layout.name,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,3,groupWorld,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,4,layout.groupName,-1,SQLITE_TRANSIENT);sqlite3_bind_int(write,5,layout.tabOrder);sqlite3_bind_int(write,6,layout.active);sqlite3_step(write);}
  sqlite3_finalize(write);
  if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_split_v2(world_id,name,target_world,target_name,side,depth,percent) VALUES(?,?,?,?,?,?,?)",-1,&write,NULL)!=SQLITE_OK)return;
  for(size_t i=0;i<layouts.size();++i)
    {const WorkspacePaneLayout& layout=layouts[i];const CString world=WorkspacePersistentWorld(layout.connectionId),targetWorld=WorkspacePersistentWorld(layout.splitConnectionId);
     sqlite3_reset(write);sqlite3_clear_bindings(write);sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,layout.name,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,3,targetWorld,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,4,layout.splitName,-1,SQLITE_TRANSIENT);sqlite3_bind_int(write,5,layout.splitAlignment);sqlite3_bind_int(write,6,layout.splitDepth);sqlite3_bind_int(write,7,layout.splitPercent);sqlite3_step(write);}
  sqlite3_finalize(write);
}
void CMainFrame::RestoreWorkspaceRoutes(const CString& identity)
{
  if(!m_workspaceRestored.insert(identity).second || !WorkspaceLayoutSchema()) return;
  sqlite3_stmt* query=NULL;
  if(!WorkspaceSpawnSchema())return;
  if(sqlite3_prepare_v2(App.db,"SELECT r.name,r.pattern,r.regex,r.omit_main,COALESCE(i.following_images,0) FROM workspace_route_v1 r LEFT JOIN workspace_route_images_v1 i ON r.world_id=i.world_id AND r.name=i.name AND r.pattern=i.pattern AND r.regex=i.regex WHERE r.world_id=? LIMIT 64",-1,&query,NULL)!=SQLITE_OK)return;
  const CString world=WorkspacePersistentWorld(identity);sqlite3_bind_text(query,1,world,-1,SQLITE_TRANSIENT);
  std::vector<WorkspaceRoute> pending;
  while(sqlite3_step(query)==SQLITE_ROW)
    { WorkspaceRoute route;route.connectionId=identity;route.transcriptName=(const char*)sqlite3_column_text(query,0);
      route.pattern=WorkspaceLayoutWide((const char*)sqlite3_column_text(query,1));route.regex=sqlite3_column_int(query,2)!=0;
      route.omitFromMain=sqlite3_column_int(query,3)!=0;route.followingImages=sqlite3_column_int(query,4)!=0;pending.push_back(route); }
  sqlite3_finalize(query);
  for(size_t i=0;i<pending.size();++i) { CString error;AddWorkspaceRoute(pending[i],error); }
}

void CMainFrame::ClearWorkspaceTranscript(const CString& id,const CString& name)
{
  ClearWorkspaceImageAssociation(id);
  m_workspacePane.ClearTranscript(id,name);
  for(size_t i=0;i<m_workspaceSplits.size();++i)m_workspaceSplits[i]->ClearTranscript(id,name);
}
bool CMainFrame::WorkspaceOutputRetainedAfter(const CString& id,unsigned __int64 boundary) const
{
  if(!m_workspacePane.RetainedSourceAfter(id,"Output",boundary))return false;
  for(const auto* pane:m_workspaceSplits)if(!pane->RetainedSourceAfter(id,"Output",boundary))return false;
  return true;
}

void CMainFrame::OnWorkspaceAgentReview()
{
  OpenWorkspaceTool(*this,59007);
}

bool CMainFrame::RemoveWorkspaceRoute(const WorkspaceRoute& route)
{
  ClearWorkspaceImageAssociation(route.connectionId);
  bool removed=false;
  if(WorkspaceLayoutSchema())
    {sqlite3_stmt* write=NULL;
     if(sqlite3_prepare_v2(App.db,"DELETE FROM workspace_route_v1 WHERE world_id=? AND name=? AND pattern=? AND regex=?",-1,&write,NULL)!=SQLITE_OK)return false;
     const CString world=WorkspacePersistentWorld(route.connectionId);const std::string pattern=WorkspaceLayoutUtf8(route.pattern);
     sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,route.transcriptName,-1,SQLITE_TRANSIENT);
     sqlite3_bind_text(write,3,pattern.c_str(),(int)pattern.size(),SQLITE_TRANSIENT);sqlite3_bind_int(write,4,route.regex);
     const int status=sqlite3_step(write);removed=sqlite3_changes(App.db)>0;sqlite3_finalize(write);if(status!=SQLITE_DONE)return false;}
  for(size_t i=m_workspaceRoutes.size();i>0;--i)
    if(m_workspaceRoutes[i-1].connectionId==route.connectionId && m_workspaceRoutes[i-1].transcriptName==route.transcriptName &&
       m_workspaceRoutes[i-1].pattern==route.pattern && m_workspaceRoutes[i-1].regex==route.regex)
      {m_workspaceRoutes.erase(m_workspaceRoutes.begin()+i-1);removed=true;}
  if(removed && WorkspaceSpawnSchema()){
    sqlite3_stmt* write=NULL;
    if(sqlite3_prepare_v2(App.db,"DELETE FROM workspace_route_images_v1 WHERE world_id=? AND name=? AND pattern=? AND regex=?",-1,&write,NULL)==SQLITE_OK){
      const CString world=WorkspacePersistentWorld(route.connectionId);const std::string pattern=WorkspaceLayoutUtf8(route.pattern);
      sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,route.transcriptName,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,3,pattern.c_str(),(int)pattern.size(),SQLITE_TRANSIENT);sqlite3_bind_int(write,4,route.regex);sqlite3_step(write);sqlite3_finalize(write);
    }
  }
  return removed;
}
bool CMainFrame::ReplaceWorkspaceRoute(const WorkspaceRoute& original,const WorkspaceRoute& replacement,CString& error)
{
  if(original.connectionId!=replacement.connectionId) {error="The route belongs to another connection.";return false;}
  if(original.transcriptName==replacement.transcriptName && original.pattern==replacement.pattern && original.regex==replacement.regex)
    return AddWorkspaceRoute(replacement,error);
  WorkspaceRoute checked=replacement;if(!checked.Compile(error))return false;
  size_t existing=m_workspaceRoutes.size();
  for(size_t i=0;i<m_workspaceRoutes.size();++i)
    if(m_workspaceRoutes[i].connectionId==original.connectionId && m_workspaceRoutes[i].transcriptName==original.transcriptName &&
       m_workspaceRoutes[i].pattern==original.pattern && m_workspaceRoutes[i].regex==original.regex){existing=i;break;}
  if(existing==m_workspaceRoutes.size()){error="That route has already been removed.";return false;}
  m_workspaceRoutes.erase(m_workspaceRoutes.begin()+existing);
  if(!AddWorkspaceRoute(checked,error)){m_workspaceRoutes.insert(m_workspaceRoutes.begin()+existing,original);return false;}
  if(!RemoveWorkspaceRoute(original)){error="The replacement was created, but the old saved rule could not be removed.";return false;}
  return true;
}
bool CMainFrame::SetWorkspaceInputPrefix(const CString& id,const CString& name,const std::wstring& prefix)
{
  if(prefix.size()>1024)return false;
  bool found=m_workspacePane.SetInputPrefix(id,name,prefix);
  for(size_t i=0;i<m_workspaceSplits.size();++i)found=m_workspaceSplits[i]->SetInputPrefix(id,name,prefix)||found;
  if(!found || !WorkspaceLayoutSchema())return found;
  sqlite3_stmt* write=NULL;
  if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_input_prefix_v1(world_id,name,prefix) VALUES(?,?,?)",-1,&write,NULL)!=SQLITE_OK)return false;
  const CString world=WorkspacePersistentWorld(id);const std::string utf8=WorkspaceLayoutUtf8(prefix);
  sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,name,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,3,utf8.c_str(),(int)utf8.size(),SQLITE_TRANSIENT);
  const int status=sqlite3_step(write);sqlite3_finalize(write);return status==SQLITE_DONE;
}
std::wstring CMainFrame::WorkspaceInputPrefix(const CString& id,const CString& name) const
{
  if(m_workspacePane.HasTranscript(id,name))return m_workspacePane.InputPrefix(id,name);
  for(size_t i=0;i<m_workspaceSplits.size();++i)if(m_workspaceSplits[i]->HasTranscript(id,name))return m_workspaceSplits[i]->InputPrefix(id,name);
  return std::wstring();
}
workspace_spawn::Preferences CMainFrame::WorkspaceTranscriptPreferences(const CString& id,const CString& name)
{
  const auto key=std::make_pair(id,name);auto cached=m_workspacePreferences.find(key);
  if(cached!=m_workspacePreferences.end())return cached->second;
  workspace_spawn::Preferences preferences;
  if(WorkspaceSpawnSchema()){
    sqlite3_stmt* query=NULL;
    if(sqlite3_prepare_v2(App.db,"SELECT mention,priority,shortcut FROM workspace_spawn_preferences_v1 WHERE world_id=? AND name=?",-1,&query,NULL)==SQLITE_OK){
      const CString world=WorkspacePersistentWorld(id);sqlite3_bind_text(query,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(query,2,name,-1,SQLITE_TRANSIENT);
      if(sqlite3_step(query)==SQLITE_ROW){preferences.mention=WorkspaceLayoutWide((const char*)sqlite3_column_text(query,0));preferences.priority=sqlite3_column_int(query,1);const int chord=sqlite3_column_int(query,2);preferences.shortcut=chord>=0 && chord<=65535?(unsigned short)chord:0;}
      sqlite3_finalize(query);
    }
  }
  if(!workspace_spawn::Valid(preferences))preferences=workspace_spawn::Preferences();
  // Runtime identities isolate duplicate opens even though preferences persist
  // under the original world's stable identity.
  if(m_workspacePreferences.size()>=4096)m_workspacePreferences.clear();
  m_workspacePreferences[key]=preferences;return preferences;
}
namespace {
bool AcceleratorUses(HACCEL table,unsigned short chord)
{
  if(!table)return false;
  int count=CopyAcceleratorTable(table,NULL,0);if(count<0 || count>4096)return true;
  std::vector<ACCEL> entries(count);if(count && CopyAcceleratorTable(table,entries.data(),count)!=count)return true;
  const unsigned modifiers=chord>>8;
  for(const auto& entry:entries){
    unsigned actual=((entry.fVirt&FSHIFT)?HOTKEYF_SHIFT:0)|((entry.fVirt&FCONTROL)?HOTKEYF_CONTROL:0)|((entry.fVirt&FALT)?HOTKEYF_ALT:0);
    if((entry.fVirt&FVIRTKEY) && entry.key==(chord&255) && actual==modifiers)return true;
  }
  return false;
}
bool NativeShortcutConflict(CMainFrame& frame,unsigned short chord)
{
  if(AcceleratorUses(frame.GetDefaultAccelerator(),chord))return true;
  for(POSITION position=App.m_pWorldDocTemplate->GetFirstDocPosition();position;){
    auto* doc=(CMUSHclientDoc*)App.m_pWorldDocTemplate->GetNextDoc(position);
    if(AcceleratorUses(doc->m_accelerator,chord))return true;
    POSITION viewPosition=doc->GetFirstViewPosition();if(viewPosition){CView* view=doc->GetNextView(viewPosition);if(view && view->GetParentFrame() && AcceleratorUses(view->GetParentFrame()->GetDefaultAccelerator(),chord))return true;}
  }
  return false;
}
bool WorkspaceCompositionActive(HWND window)
{
  HMODULE module=GetModuleHandleW(L"imm32.dll");if(!module)return false;
  auto context=(HANDLE(WINAPI*)(HWND))GetProcAddress(module,"ImmGetContext");
  auto length=(LONG(WINAPI*)(HANDLE,DWORD,LPVOID,DWORD))GetProcAddress(module,"ImmGetCompositionStringW");
  auto release=(BOOL(WINAPI*)(HWND,HANDLE))GetProcAddress(module,"ImmReleaseContext");
  if(!context || !length || !release)return false;
  HANDLE current=context(window);if(!current)return false;
  const bool composing=length(current,8,NULL,0)>0;release(window,current);return composing;
}
struct SpawnNavigation {CWorkspacePane* pane;size_t index;WorkspacePaneLayout layout;int priority;};
std::vector<SpawnNavigation> WorkspaceNavigation(CMainFrame& frame)
{
  std::vector<SpawnNavigation> result;
  auto append=[&](CWorkspacePane* pane){std::vector<WorkspacePaneLayout> layouts;pane->GetLayouts(layouts,false);for(size_t i=0;i<layouts.size() && i<pane->TranscriptCount();++i){SpawnNavigation item={pane,i,layouts[i],frame.WorkspaceTranscriptPreferences(layouts[i].connectionId,layouts[i].name).priority};result.push_back(item);}};
  append(&frame.m_workspacePane);for(auto* pane:frame.m_workspaceSplits)append(pane);
  std::stable_sort(result.begin(),result.end(),[](const SpawnNavigation& a,const SpawnNavigation& b){if(a.priority!=b.priority)return a.priority>b.priority;const int world=a.layout.connectionId.Compare(b.layout.connectionId);return world?world<0:a.layout.name.Compare(b.layout.name)<0;});
  return result;
}
}
bool CMainFrame::WorkspaceShortcutAvailable(unsigned short chord,const CString& id,const CString& name,CString& error)
{
  error.Empty();if(!chord)return true;
  if(!workspace_spawn::ShortcutValid(chord)){error="Choose Ctrl+Alt with a letter/digit, or a modified function key. Editing/Windows chords and Ctrl+Shift+F1-F9 are reserved.";return false;}
  if(NativeShortcutConflict(*this,chord)){error="That shortcut is already used by an application or open world's command/macro.";return false;}
  if(!WorkspaceSpawnSchema()){error="Transcript settings storage is unavailable.";return false;}
  sqlite3_stmt* query=NULL;
  if(sqlite3_prepare_v2(App.db,"SELECT world_id,name FROM workspace_spawn_preferences_v1 WHERE shortcut=?",-1,&query,NULL)!=SQLITE_OK){error="Shortcut conflicts could not be checked.";return false;}
  sqlite3_bind_int(query,1,chord);bool conflict=false;const CString world=WorkspacePersistentWorld(id);
  int status=SQLITE_OK;
  while((status=sqlite3_step(query))==SQLITE_ROW){CString existingWorld=(const char*)sqlite3_column_text(query,0),existingName=(const char*)sqlite3_column_text(query,1);if(existingWorld!=world || existingName!=name)conflict=true;}
  sqlite3_finalize(query);
  if(status!=SQLITE_DONE){error="Shortcut conflicts could not be checked.";return false;}
  if(conflict){error="That shortcut belongs to another saved transcript, including closed worlds.";return false;}return true;
}
bool CMainFrame::SetWorkspaceTranscriptPreferences(const CString& id,const CString& name,const workspace_spawn::Preferences& preferences,CString& error)
{
  if(WorkspaceEndpointForWindow((LPCTSTR)id).empty()){error="The original world has closed. Reopen its transcript options before saving.";return false;}
  if(!workspace_spawn::Valid(preferences)){error="Use mention text up to 128 characters without controls, priority -100 to 100, and a valid shortcut.";return false;}
  if(!WorkspaceShortcutAvailable(preferences.shortcut,id,name,error))return false;
  if(!WorkspaceSpawnSchema()){error="Transcript settings storage is unavailable.";return false;}
  sqlite3_stmt* write=NULL;
  if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_spawn_preferences_v1 VALUES(?,?,?,?,?)",-1,&write,NULL)!=SQLITE_OK){error="Transcript settings could not be saved.";return false;}
  const CString world=WorkspacePersistentWorld(id);const std::string mention=WorkspaceLayoutUtf8(preferences.mention);
  sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,name,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,3,mention.c_str(),(int)mention.size(),SQLITE_TRANSIENT);sqlite3_bind_int(write,4,preferences.priority);sqlite3_bind_int(write,5,preferences.shortcut);
  const int status=sqlite3_step(write);sqlite3_finalize(write);if(status!=SQLITE_DONE){error="Transcript settings could not be saved.";return false;}
  for(auto it=m_workspacePreferences.begin();it!=m_workspacePreferences.end();)if(WorkspacePersistentWorld(it->first.first)==world && it->first.second==name)it=m_workspacePreferences.erase(it);else ++it;
  auto apply=[&](CWorkspacePane* pane){std::vector<WorkspacePaneLayout> layouts;pane->GetLayouts(layouts,false);for(const auto& item:layouts)if(WorkspacePersistentWorld(item.connectionId)==world && item.name==name){m_workspacePreferences[std::make_pair(item.connectionId,name)]=preferences;pane->SetPreferences(item.connectionId,name,preferences);}};
  apply(&m_workspacePane);for(auto* pane:m_workspaceSplits)apply(pane);return true;
}
bool CMainFrame::FocusWorkspaceTranscript(size_t index)
{
  auto entries=WorkspaceNavigation(*this);if(index>=entries.size())return false;
  auto& entry=entries[index];ShowControlBar(entry.pane,TRUE,FALSE);RecalcLayout();return entry.pane->FocusTranscript(entry.index);
}
bool CMainFrame::FocusRelativeWorkspaceTranscript(int direction)
{
  auto entries=WorkspaceNavigation(*this);if(entries.empty())return false;
  HWND focus=::GetFocus();size_t current=0;bool found=false;
  for(size_t i=0;i<entries.size();++i)if(focus==entries[i].pane->GetSafeHwnd() || ::IsChild(entries[i].pane->GetSafeHwnd(),focus) || (workspace_composer::BorrowedEditors().count(entries[i].pane->SharedInputWindow()) && focus==entries[i].pane->SharedInputWindow())){current=i;found=true;break;}
  const size_t next=!found?(direction<0?entries.size()-1:0):(current+entries.size()+(direction<0?-1:1))%entries.size();return FocusWorkspaceTranscript(next);
}
bool CMainFrame::HandleWorkspaceShortcut(MSG* message)
{
  if((message->message!=WM_KEYDOWN && message->message!=WM_SYSKEYDOWN) || (message->lParam&(1UL<<30)) || (::GetKeyState(VK_RMENU)&0x8000) || (::GetKeyState(VK_LWIN)&0x8000) || (::GetKeyState(VK_RWIN)&0x8000))return false;
  // Never steal keys from a modal review/settings dialog.
  if(!IsWindowEnabled())return false;
  for(HWND cursor=message->hwnd;cursor;cursor=::GetParent(cursor)){wchar_t name[32]={};::GetClassNameW(cursor,name,32);if(!wcscmp(name,L"#32770"))return false;}
  const unsigned modifiers=((GetKeyState(VK_SHIFT)&0x8000)?HOTKEYF_SHIFT:0)|((GetKeyState(VK_CONTROL)&0x8000)?HOTKEYF_CONTROL:0)|((GetKeyState(VK_MENU)&0x8000)?HOTKEYF_ALT:0);
  const unsigned short chord=(unsigned short)((modifiers<<8)|(message->wParam&255));
  if((workspace_spawn::ShortcutValid(chord) || (modifiers==(HOTKEYF_CONTROL|HOTKEYF_ALT) && (message->wParam==VK_PRIOR || message->wParam==VK_NEXT))) && WorkspaceCompositionActive(message->hwnd))return false;
  if(modifiers==(HOTKEYF_CONTROL|HOTKEYF_ALT) && (message->wParam==VK_PRIOR || message->wParam==VK_NEXT)){
    if(NativeShortcutConflict(*this,chord))return false;return FocusRelativeWorkspaceTranscript(message->wParam==VK_PRIOR?-1:1);
  }
  if(!workspace_spawn::ShortcutValid(chord) || !chord)return false;
  CWorkspacePane* match=NULL;size_t selected=0;
  auto inspect=[&](CWorkspacePane* pane){std::vector<WorkspacePaneLayout> layouts;pane->GetLayouts(layouts,false);for(size_t i=0;i<layouts.size() && i<pane->TranscriptCount();++i){const auto& layout=layouts[i];if(WorkspaceTranscriptPreferences(layout.connectionId,layout.name).shortcut==chord){if(match){match=(CWorkspacePane*)-1;return;}match=pane;selected=i;}}};
  inspect(&m_workspacePane);for(auto* pane:m_workspaceSplits){if(match==(CWorkspacePane*)-1)break;inspect(pane);}
  if(!match || match==(CWorkspacePane*)-1 || NativeShortcutConflict(*this,chord))return false;
  ShowControlBar(match,TRUE,FALSE);RecalcLayout();return match->FocusTranscript(selected);
}
void CMainFrame::OnWorkspaceFocus(UINT id){FocusWorkspaceTranscript(id-59010);}
void CMainFrame::OnUpdateWorkspaceFocus(CCmdUI* command)
{
  size_t count=m_workspacePane.TranscriptCount();for(size_t i=0;i<m_workspaceSplits.size();++i)count+=m_workspaceSplits[i]->TranscriptCount();
  command->Enable(command->m_nID>=59010 && command->m_nID-59010<count);
}

// Route UI owns a captured original connection; implementation lives separately.
#include "WorkspaceRouteManager.h"
void CMainFrame::OnWorkspaceRoute()
{
  CMDIChildWnd* child=MDIGetActive();
  CMUSHclientDoc* doc=child?DYNAMIC_DOWNCAST(CMUSHclientDoc,child->GetActiveDocument()):NULL;
  if(!doc){AfxMessageBox("Open a world before creating an output route.");return;}
  OpenWorkspaceRouteManager(this,WorkspaceConnectionId(doc));
}
