// mainfrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "MUSHclient.h"

#include "doc.h"
#include "mainfrm.h"
#include "WorkspaceIntegration.h"
#include "WorkspaceTools.h"
#include "WorkspaceResources.h"
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
  ON_COMMAND(ID_WORKSPACE_TRANSCRIPT, OnWorkspaceTranscript)
  ON_COMMAND(59005, OnWorkspaceFloatTranscript)
  ON_COMMAND(59006, OnWorkspaceRoute)
  ON_COMMAND(59007, OnWorkspaceAgentReview)
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

m_timer = 0;
m_ticktimer = 0;
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
  for (size_t i=0;i<m_workspaceSplits.size();++i) delete m_workspaceSplits[i];
  for (size_t i=0;i<m_workspaceTools.size();++i) delete m_workspaceTools[i];
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{

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

  // Tabbed Windows - if wanted

  if (App.m_iWindowTabsStyle == WINDOW_TABS_TOP)
    m_wndMDITabs.Create (this, MT_TOP);
  else if (App.m_iWindowTabsStyle == WINDOW_TABS_BOTTOM)
    m_wndMDITabs.Create (this, MT_BOTTOM);

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
  workspace->AppendMenu(MF_STRING, 59005, "&Float selected transcript separately");
  workspace->AppendMenu(MF_STRING, 59001, "PennMUSH code &editor...");
  workspace->AppendMenu(MF_STRING, 59002, "Agent &access...");
  workspace->AppendMenu(MF_STRING, 59003, "Agent &conversations...");
  workspace->AppendMenu(MF_STRING, 59004, "Scene &director...");
  CMenu shortcuts;shortcuts.CreatePopupMenu();
  for(UINT index=0;index<9;++index) {CString label;label.Format("Pane %u\tCtrl+Shift+F%u",index+1,index+1);shortcuts.AppendMenu(MF_STRING,59010+index,label);}
  workspace->AppendMenu(MF_POPUP,(UINT_PTR)shortcuts.Detach(),"Pane shortcuts");
  menu->AppendMenu(MF_POPUP, (UINT_PTR)workspace->Detach(), "&Workspace");
  delete workspace;
  DrawMenuBar();
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
          {extra->EnableDocking(CBRS_ALIGN_ANY);extra->SetActionHandler(ActivateWorkspaceAction);extra->SetInputHandler(WorkspaceSendHumanInput);extra->SetPruneHandler(WorkspacePanePruned);extra->SetConnectionHandler(WorkspaceIsConnectionLive);extra->SetInputErrorHandler(WorkspaceHumanInputError);
            m_workspaceSplits.push_back(extra);target=extra; }
        else delete extra; }
    }
  if (!target->OpenTranscript(connectionId,name)) return false;
  if(WorkspaceLayoutSchema())
    {sqlite3_stmt* query=NULL;
     if(sqlite3_prepare_v2(App.db,"SELECT prefix FROM workspace_input_prefix_v1 WHERE world_id=? AND name=?",-1,&query,NULL)==SQLITE_OK)
       {const CString world=WorkspacePersistentWorld(connectionId);sqlite3_bind_text(query,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(query,2,name,-1,SQLITE_TRANSIENT);
        if(sqlite3_step(query)==SQLITE_ROW)target->SetInputPrefix(connectionId,name,WorkspaceLayoutWide((const char*)sqlite3_column_text(query,0)));
        sqlite3_finalize(query);}}
  if(restore) { WorkspacePositionPane(*this,*target,layout);RestoreWorkspacePaneGroup(*target,layout); }
  else if(target!=&m_workspacePane && !m_workspacePane.IsEmpty())target->AttachToTabWnd(&m_workspacePane,DM_SHOW);
  else DockPane(target,AFX_IDW_DOCKBAR_RIGHT);
  ShowControlBar(target, TRUE, FALSE);
  RecalcLayout();
  return true;
}
bool CMainFrame::AppendWorkspaceRecord(const CString& name, const WorkspaceRecord& record)
{
  bool appended=m_workspacePane.AppendRecord(name,record);
  for(size_t i=0;i<m_workspaceSplits.size();++i)
    appended=m_workspaceSplits[i]->AppendRecord(name,record)||appended;
  return appended;
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

void CMainFrame::OnWorkspaceFloatTranscript()
{
  CWorkspacePane* selected=NULL;
  CWnd* focus=GetFocus();
  for(CWnd* parent=focus;parent;parent=parent->GetParent())
    if((selected=DYNAMIC_DOWNCAST(CWorkspacePane,parent))!=NULL)break;
  if(!selected && !m_workspacePane.IsEmpty())selected=&m_workspacePane;
  if(!selected)return;
  CRect bounds;GetWindowRect(bounds);selected->FloatPane(CRect(bounds.TopLeft()+CPoint(80,120),CSize(500,350)),DM_SHOW);
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
  WorkspacePaneLayout layout;if(WorkspaceReadLayout("@tools",title,layout)){WorkspacePositionPane(*this,*host,layout);RestoreWorkspacePaneGroup(*host,layout);}else DockControlBar(host,AFX_IDW_DOCKBAR_RIGHT); ShowControlBar(host,TRUE,FALSE); RecalcLayout();
  return true;
}

bool CMainFrame::AddWorkspaceRoute(const WorkspaceRoute& source,CString& error)
{
  bool alreadyExists=false;
  for(size_t i=0;i<m_workspaceRoutes.size();++i)
    if(m_workspaceRoutes[i].connectionId==source.connectionId && m_workspaceRoutes[i].transcriptName==source.transcriptName &&
       m_workspaceRoutes[i].pattern==source.pattern && m_workspaceRoutes[i].regex==source.regex)alreadyExists=true;
  if(m_workspaceRoutes.size()>=64 && !alreadyExists) { error="Maximum of 64 workspace routes reached.";return false; }
  WorkspaceRoute route=source;
  if(!route.Compile(error)) return false;
  if(!OpenWorkspaceTranscript(route.connectionId,route.transcriptName))
    { error="Unable to create the transcript pane.";return false; }
  bool replaced=false;
  for(size_t i=0;i<m_workspaceRoutes.size();++i)
    if(m_workspaceRoutes[i].connectionId==route.connectionId && m_workspaceRoutes[i].transcriptName==route.transcriptName &&
       m_workspaceRoutes[i].pattern==route.pattern && m_workspaceRoutes[i].regex==route.regex)
      { m_workspaceRoutes[i]=route;replaced=true;break; }
  if(!replaced)m_workspaceRoutes.push_back(route);
  if(WorkspaceLayoutSchema())
    {
    sqlite3_stmt* write=NULL;
    if(sqlite3_prepare_v2(App.db,"INSERT OR REPLACE INTO workspace_route_v1(world_id,name,pattern,regex,omit_main) VALUES(?,?,?,?,?)",-1,&write,NULL)==SQLITE_OK)
      { const CString world=WorkspacePersistentWorld(route.connectionId);const std::string pattern=WorkspaceLayoutUtf8(route.pattern);
        sqlite3_bind_text(write,1,world,-1,SQLITE_TRANSIENT);sqlite3_bind_text(write,2,route.transcriptName,-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(write,3,pattern.c_str(),(int)pattern.size(),SQLITE_TRANSIENT);
        sqlite3_bind_int(write,4,route.regex);sqlite3_bind_int(write,5,route.omitFromMain);sqlite3_step(write);sqlite3_finalize(write); }
    }
  return true;
}
bool CMainFrame::RouteWorkspaceRecord(const WorkspaceRecord& record)
{
  if(record.connectionId.IsEmpty() || m_workspaceClosedConnections.find(record.connectionId)!=m_workspaceClosedConnections.end())return false;
  RestoreWorkspaceRoutes(record.connectionId); // cold-start routes exist before the first completed record is matched
  bool omit=false;
  std::set<CString> delivered;
  for(size_t i=0;i<m_workspaceRoutes.size();++i)
    {
    const WorkspaceRoute& route=m_workspaceRoutes[i];
    if(!route.Matches(record)) continue;
    bool accepted=delivered.find(route.transcriptName)!=delivered.end();
    if(!accepted)
      { accepted=AppendWorkspaceRecord(route.transcriptName,record);
        if(accepted)WorkspacePublishToPane(record.connectionId,route.transcriptName,record); }
    if(accepted) { delivered.insert(route.transcriptName);omit=omit||route.omitFromMain; }
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

void CMainFrame::SaveWorkspaceLayouts()
{
  if(!WorkspaceLayoutSchema()) return;
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
  if(sqlite3_prepare_v2(App.db,"SELECT name,pattern,regex,omit_main FROM workspace_route_v1 WHERE world_id=? LIMIT 64",-1,&query,NULL)!=SQLITE_OK)return;
  const CString world=WorkspacePersistentWorld(identity);sqlite3_bind_text(query,1,world,-1,SQLITE_TRANSIENT);
  std::vector<WorkspaceRoute> pending;
  while(sqlite3_step(query)==SQLITE_ROW)
    { WorkspaceRoute route;route.connectionId=identity;route.transcriptName=(const char*)sqlite3_column_text(query,0);
      route.pattern=WorkspaceLayoutWide((const char*)sqlite3_column_text(query,1));route.regex=sqlite3_column_int(query,2)!=0;
      route.omitFromMain=sqlite3_column_int(query,3)!=0;pending.push_back(route); }
  sqlite3_finalize(query);
  for(size_t i=0;i<pending.size();++i) { CString error;AddWorkspaceRoute(pending[i],error); }
}

void CMainFrame::ClearWorkspaceTranscript(const CString& id,const CString& name)
{
  m_workspacePane.ClearTranscript(id,name);
  for(size_t i=0;i<m_workspaceSplits.size();++i)m_workspaceSplits[i]->ClearTranscript(id,name);
}

void CMainFrame::OnWorkspaceAgentReview()
{
  OpenWorkspaceTool(*this,59007);
}

bool CMainFrame::RemoveWorkspaceRoute(const WorkspaceRoute& route)
{
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
bool CMainFrame::FocusWorkspaceTranscript(size_t index)
{
  CWorkspacePane* host=&m_workspacePane;
  if(index>=host->TranscriptCount())
    {index-=host->TranscriptCount();host=NULL;
     for(size_t i=0;i<m_workspaceSplits.size();++i)
       {if(index<m_workspaceSplits[i]->TranscriptCount()){host=m_workspaceSplits[i];break;}index-=m_workspaceSplits[i]->TranscriptCount();}}
  if(!host)return false;ShowControlBar(host,TRUE,FALSE);RecalcLayout();return host->FocusTranscript(index);
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
