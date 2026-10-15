// textchildfrm.cpp : implementation of the CTextChildFrame class
//

#include "stdafx.h"
#include "MUSHclient.h"
#include "mainfrm.h"
#include "TextDocument.h"
#include "textchildfrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTextChildFrame

IMPLEMENT_DYNCREATE(CTextChildFrame, CMDIChildWndEx)

BEGIN_MESSAGE_MAP(CTextChildFrame, CMDIChildWndEx)
	//{{AFX_MSG_MAP(CTextChildFrame)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTextChildFrame construction/destruction

CTextChildFrame::CTextChildFrame()
{
  m_pDoc = NULL;

}

CTextChildFrame::~CTextChildFrame()
{
}

/////////////////////////////////////////////////////////////////////////////
// CTextChildFrame diagnostics

#ifdef _DEBUG
void CTextChildFrame::AssertValid() const
{
	CMDIChildWndEx::AssertValid();
}

void CTextChildFrame::Dump(CDumpContext& dc) const
{
	CMDIChildWndEx::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CTextChildFrame message handlers


BOOL CTextChildFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	
	cs.style &= ~FWS_ADDTOTITLE;  // do not add document name to window title
		
	return CMDIChildWndEx::PreCreateWindow(cs);
}



void CTextChildFrame::OnUpdateFrameMenu(BOOL bActivate, CWnd* pActivateWnd, HMENU hMenuAlt)
  {

  if (Frame.IsFullScreen ())
    return;

  CMDIChildWndEx::OnUpdateFrameMenu (bActivate, pActivateWnd, hMenuAlt);

  }

BOOL CTextChildFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
   m_pDoc = (CTextDocument*) pContext->m_pCurrentDoc;
   ASSERT_VALID(m_pDoc);
	
	return CMDIChildWndEx::OnCreateClient(lpcs, pContext);
}

void CTextChildFrame::OnUpdateFrameTitle(BOOL addToTitle)
{
  WorkspaceModernResources resources;
  CMDIChildWndEx::OnUpdateFrameTitle(addToTitle);
}

LRESULT CTextChildFrame::WindowProc(UINT message, WPARAM wp, LPARAM lp)
{
  if(message==WM_MDIACTIVATE || message==WM_SETTEXT || message==WM_SIZE)
  {
    WorkspaceModernResources resources;
    return CMDIChildWndEx::WindowProc(message,wp,lp);
  }
  return CMDIChildWndEx::WindowProc(message,wp,lp);
}
