// childfrm.cpp : implementation of the CActivityChildFrame class
//

#include "stdafx.h"
#include "MUSHclient.h"
#include "mainfrm.h"

#include "activitychildfrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CActivityChildFrame

IMPLEMENT_DYNCREATE(CActivityChildFrame, CMDIChildWndEx)

BEGIN_MESSAGE_MAP(CActivityChildFrame, CMDIChildWndEx)
	//{{AFX_MSG_MAP(CActivityChildFrame)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CActivityChildFrame construction/destruction

CActivityChildFrame::CActivityChildFrame()
{
	// TODO: add member initialization code here

}

CActivityChildFrame::~CActivityChildFrame()
{
}

/////////////////////////////////////////////////////////////////////////////
// CActivityChildFrame diagnostics

#ifdef _DEBUG
void CActivityChildFrame::AssertValid() const
{
	CMDIChildWndEx::AssertValid();
}

void CActivityChildFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWndEx::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CActivityChildFrame message handlers


BOOL CActivityChildFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	
	cs.style &= ~FWS_ADDTOTITLE;  // do not add document name to window title
		
	return CMDIChildWndEx::PreCreateWindow(cs);
}




void CActivityChildFrame::OnUpdateFrameMenu(BOOL bActivate, CWnd* pActivateWnd, HMENU hMenuAlt)
  {

  if (Frame.IsFullScreen ())
    return;

  CMDIChildWndEx::OnUpdateFrameMenu (bActivate, pActivateWnd, hMenuAlt);

  }


void CActivityChildFrame::OnUpdateFrameTitle(BOOL addToTitle)
{
  WorkspaceModernResources resources;
  CMDIChildWndEx::OnUpdateFrameTitle(addToTitle);
}

LRESULT CActivityChildFrame::WindowProc(UINT message, WPARAM wp, LPARAM lp)
{
  if(message==WM_MDIACTIVATE || message==WM_SETTEXT || message==WM_SIZE)
  {
    WorkspaceModernResources resources;
    return CMDIChildWndEx::WindowProc(message,wp,lp);
  }
  return CMDIChildWndEx::WindowProc(message,wp,lp);
}
