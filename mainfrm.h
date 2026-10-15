// mainfrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "MDIClientWnd.h"
#include "MyStatusBar.h"
#include "MDITabs.h"
#include "WorkspacePane.h"
#include "WorkspaceFrame.h"
namespace workspace_dock {class Session;struct Entry;}
class CWorkspaceShell;

class CMUSHclientDoc;

#define TRAY_FIRST_MENU 11000
#define TRAY_MENU_COUNT 50   // number of world menu items we support

#define ACCELERATOR_FIRST_COMMAND 12000
#define ACCELERATOR_COUNT 1000   // number of accelerator items we support

class CMyToolBar : public CToolBar
{
public:
	void EnableDocking(DWORD dwDockStyle);
	void DrawGripper(CDC* pDC, const CRect& rect);
	void EraseNonClient();
	virtual void DoPaint(CDC* pDC);

protected:

	//{{AFX_MSG(CMyToolBar)
	afx_msg void OnNcPaint();
	//}}AFX_MSG
	
	DECLARE_MESSAGE_MAP()
};


class CMainFrame : public CWorkspaceFrame
{
	DECLARE_DYNAMIC(CMainFrame)
public:
	CMainFrame();
  using CMDIFrameWnd::DockControlBar;
  using CMDIFrameWnd::FloatControlBar;
  using CMDIFrameWnd::ShowControlBar;
  void DockControlBar(CWorkspacePane* pane,UINT dock=AFX_IDW_DOCKBAR_RIGHT,LPCRECT rect=NULL) { DockPane(pane,dock,rect); }
  void ShowControlBar(CWorkspacePane* pane,BOOL show,BOOL delay) { pane->ShowPane(show,delay,show); }
  void FloatControlBar(CWorkspacePane* pane,CPoint point,DWORD) { pane->FloatPane(CRect(point,CSize(500,350)),DM_SHOW); }


	BOOL OnDynamicTipText(UINT id, NMHDR* pNMHDR, LRESULT* pResult);

// Attributes
public:

   int m_nPauseItem;

   UINT m_wDeviceID;

   bool m_bFullScreen;      // are we in full screen mode?
   WINDOWPLACEMENT m_wpPrev;  // where was the window before full screen mode?
   BOOL m_bMaximized;       // were MDI child windows maximized?

   bool m_bFlashingWindow;  // true if we flashed the window

   NOTIFYICONDATA m_niData; 

   CMDITabs m_wndMDITabs;   // for tabs

   CTime m_timeFlashed;     // when we flashed the icon last
   int m_iTabsCount;

// Operations
public:

   void SetStatusMessage(const CString& msg);
   void SetStatusMessageNow(const CString& msg);
   void SetStatusNormal (void);
   void ReturnStatusRect (CRect & rect);
   void ReturnToolbarRect(CRect & rect);
   void DelayDebugStatus(const CString& msg);

   void FixUpTitleBar (void);
   void OnUpdateBtnWorlds (int iWorld, CCmdUI* pCmdUI); 
   void CancelSound (void);
   bool PlaySoundFile (const CString & strSound);
   void ShowMCIError (const DWORD dwCode, const CString & strSound);
   bool IsFullScreen () { return m_bFullScreen; };
   void AddTrayIcon (void);
   void LeftTrayClick (void);
   void RightTrayClick (void);

   void OpenAndConnectToWorldsInStartupList (const bool bConnect);

   virtual void OnUpdateFrameMenu(HMENU hMenuAlt);
   virtual void OnUpdateFrameTitle(BOOL bAddToTitle);

   void DoFileOpen (void);
   void DoFixMenus(CCmdUI* pCmdUI);

   void DockControlBarNextTo(CControlBar* pBar,
                             CControlBar* pTargetBar);

   void CheckTimerFallback (void);
   void ProcessTimers (void);
   bool OpenWorkspaceTranscript(const CString& connectionId, const CString& name);
   bool AppendWorkspaceRecord(const CString& name, const WorkspaceRecord& record);
   void CloseWorkspaceConnection(const CString& connectionId);
   void ClearWorkspaceTranscript(const CString& connectionId,const CString& name);
   bool WorkspaceOutputRetainedAfter(const CString& connectionId,unsigned __int64 boundary) const;
   bool HostWorkspaceTool(HWND toolWindow, const CString& title);
   bool AddWorkspaceRoute(const WorkspaceRoute& route, CString& error);
   bool ReplaceWorkspaceRoute(const WorkspaceRoute& original,const WorkspaceRoute& replacement,CString& error);
   bool RemoveWorkspaceRoute(const WorkspaceRoute& route);
   bool SetWorkspaceInputPrefix(const CString& connectionId,const CString& name,const std::wstring& prefix);
   std::wstring WorkspaceInputPrefix(const CString& connectionId,const CString& name) const;
   bool FocusWorkspaceTranscript(size_t index);
   bool RouteWorkspaceRecord(const WorkspaceRecord& record);
   void ClearWorkspaceImageAssociation(const CString& connectionId) { m_workspaceImages.Clear((LPCTSTR)connectionId); }
   workspace_spawn::Preferences WorkspaceTranscriptPreferences(const CString& connectionId,const CString& name);
   bool SetWorkspaceTranscriptPreferences(const CString& connectionId,const CString& name,const workspace_spawn::Preferences& preferences,CString& error);
   bool HandleWorkspaceShortcut(MSG* message);
   bool WorkspaceShortcutAvailable(unsigned short chord,const CString& connectionId,const CString& name,CString& error);
   bool FocusRelativeWorkspaceTranscript(int direction);
   void SaveWorkspaceLayouts();
  workspace_dock::Session* m_workspaceDockSession;
  std::vector<workspace_dock::Entry> WorkspaceDockEntries() const;
  void LoadWorkspaceDockState();
  bool RestoreWorkspaceDockState(CWorkspacePane& pane,const CString& world,const CString& name);
  CWorkspaceShell* m_workspaceShell;
  CString m_workspaceShellWorld,m_workspaceShellName;
  bool m_workspaceShellReady;
  unsigned m_workspaceLegacyMask;
  void UpdateWorkspaceShell();
  void WorkspaceShellAction(int action,const std::string& world,const std::string& name);
  void SaveWorkspaceShell();
  afx_msg LRESULT OnInitializeWorkspaceShell(WPARAM,LPARAM);
  afx_msg void OnWorkspaceShellCommand(UINT command);
  afx_msg void OnUpdateWorkspaceShellCommand(CCmdUI* command);
   void RestoreWorkspaceRoutes(const CString& connectionId);
   const std::vector<WorkspaceRoute>& WorkspaceRoutes() const { return m_workspaceRoutes; }

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
public:

	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	CMyStatusBar  m_wndStatusBar;

  UINT m_timer;
  UINT m_ticktimer;

	CMyToolBar    m_wndToolBar;
	CMyToolBar    m_wndGameToolBar;
	CMyToolBar    m_wndActivityToolBar;

  CDialogBar  m_wndInfoBar;

  CHARFORMAT  m_defaultInfoBarFormat;

  COLORREF    m_backgroundColour;  // MDI frame background colour, 0xFFFFFFFF for the default

  CWorkspacePane m_workspacePane;
  unsigned m_workspaceRenderBatchDepth;
  void BeginWorkspaceRenderBatch();
  void EndWorkspaceRenderBatch();
  std::vector<CWorkspacePane*> m_workspaceSplits;
  std::vector<CWorkspacePane*> m_workspaceTools;
  std::vector<WorkspaceRoute> m_workspaceRoutes;
  workspace_spawn::ImageAssociation m_workspaceImages;
  std::map<std::pair<CString,CString>,workspace_spawn::Preferences> m_workspacePreferences;
  std::map<CWorkspacePane*,WorkspacePaneLayout> m_workspaceRestoreGroups;
  std::set<CWorkspacePane*> m_workspaceRestoredSplits;
  void RestoreWorkspacePaneGroup(CWorkspacePane& pane,const WorkspacePaneLayout& layout);
  std::set<CString> m_workspaceRestored;
  std::set<CString> m_workspaceClosedConnections;
  afx_msg void OnWorkspaceTranscript();
  afx_msg void OnWorkspaceFloatTranscript();
  afx_msg void OnWorkspaceTool(UINT id);
  afx_msg void OnWorkspaceAgentReview();
  afx_msg void OnWorkspaceLogStartSetup();
  afx_msg void OnUpdateWorkspaceLogStartSetup(CCmdUI* command);
  afx_msg void OnWorkspaceRoute();
  afx_msg void OnWorkspaceFocus(UINT id);
  afx_msg void OnUpdateWorkspaceFocus(CCmdUI* command);
  void EnsureWorkspaceMenu();

protected:  // control bar embedded members

  CMDIClientWnd m_wndMDIClient;

  bool        m_bActive;  // is app active or not?

// Generated message map functions
protected:
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnDisplayActivitylist();
	afx_msg BOOL OnWorldSwitch(UINT nIDC);
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnUpdateStatuslineFreeze(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStatuslineMushname(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStatuslineLines(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStatuslineLog(CCmdUI* pCmdUI);
	afx_msg void OnUpdateStatuslineTime(CCmdUI* pCmdUI);
	afx_msg void OnHelpTipoftheday();
	afx_msg void OnUpdateDisplayActivitylist(CCmdUI* pCmdUI);
	afx_msg void OnFilePreferences();
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg void OnConnectionAutoconnect();
	afx_msg void OnUpdateConnectionAutoconnect(CCmdUI* pCmdUI);
	afx_msg void OnConnectionReconnectondisconnect();
	afx_msg void OnUpdateConnectionReconnectondisconnect(CCmdUI* pCmdUI);
	afx_msg void OnHelpContents();
	afx_msg void OnUpdateBtnWorldsWorld0(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld1(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld2(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld3(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld4(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld5(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld6(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld7(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld8(CCmdUI* pCmdUI);
	afx_msg void OnUpdateBtnWorldsWorld9(CCmdUI* pCmdUI);
	afx_msg void OnWebPage();
	afx_msg void OnViewResetToolbars();
	afx_msg void OnHelpMudlists();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	afx_msg void OnDisplayStopsoundplaying();
	afx_msg void OnUpdateDisplayStopsoundplaying(CCmdUI* pCmdUI);
	afx_msg void OnHelpBugreportsuggestion();
	afx_msg void OnViewAlwaysontop();
	afx_msg void OnUpdateViewAlwaysontop(CCmdUI* pCmdUI);
	afx_msg void OnGameSendtoallworlds();
	afx_msg void OnEditReloadnamesfile();
	afx_msg void OnViewFullscreenmode();
	afx_msg void OnUpdateViewFullscreenmode(CCmdUI* pCmdUI);
	afx_msg void OnEditNotesworkarea();
	afx_msg void OnWindowCloseallnotepadwindows();
	afx_msg void OnFileOpen();
	afx_msg void OnFileOpenworldsinstartuplist();
	afx_msg void OnUpdateFileOpenworldsinstartuplist(CCmdUI* pCmdUI);
	afx_msg void OnFileWinsock();
	afx_msg void OnConnectionConnecttoallopenworlds();
	afx_msg void OnConnectionConnecttoworldsinstartuplist();
	afx_msg void OnUpdateConnectionConnecttoworldsinstartuplist(CCmdUI* pCmdUI);
	afx_msg void OnHelpForum();
	afx_msg void OnHelpFunctionswebpage();
	afx_msg void OnHelpDocumentationwebpage();
	afx_msg void OnHelpRegularexpressionswebpage();
	afx_msg void OnHelpPluginslist();
	afx_msg void OnEditGenerateuniqueid();
	afx_msg void OnGameFunctionslist();
	afx_msg void OnEditConvertclipboardforumcodes();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	//}}AFX_MSG
  afx_msg LRESULT OnMCINotify(WPARAM, LPARAM);
	afx_msg void OnF1Test();
	afx_msg void OnUpdateInfoBar(CCmdUI* pCmdUI);
  afx_msg void OnTrayMenu(UINT nID);    
	afx_msg void OnFixMenus(CCmdUI* pCmdUI);
  DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

CMUSHclientDoc * FindWorld (const UINT nIDC);

