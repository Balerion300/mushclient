// Textchildfrm.h : interface of the CTextChildFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma push_macro("Translate")
#undef Translate
#include <afxmdichildwndex.h>
#pragma pop_macro("Translate")
#include "WorkspaceResources.h"

class CTextChildFrame : public CMDIChildWndEx
{
	DECLARE_DYNCREATE(CTextChildFrame)
public:
	CTextChildFrame();

  CTextDocument * m_pDoc;

// Attributes
protected:
  
// Operations
public:

  virtual void OnUpdateFrameTitle(BOOL addToTitle) override;
  virtual LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override;
  virtual void OnUpdateFrameMenu(BOOL bActivate, CWnd* pActivateWnd, HMENU hMenuAlt);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTextChildFrame)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTextChildFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	//{{AFX_MSG(CTextChildFrame)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
