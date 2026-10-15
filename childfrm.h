// childfrm.h : interface of the CChildFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "MySplitterWnd.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxmdichildwndex.h>
#pragma pop_macro("Translate")

class CChildFrame : public CMDIChildWndEx
{
	DECLARE_DYNCREATE(CChildFrame)
public:
	CChildFrame();

// Attributes
protected:

public:
  
	CMySplitterWnd m_wndSplitter;

  CMUSHView * m_topview;

  CMUSHclientDoc * m_pDoc;

// Operations
public:

  void FixUpSplitterBar (void);

  virtual void OnUpdateFrameMenu(BOOL bActivate, CWnd* pActivateWnd, HMENU hMenuAlt);
  virtual CString GetFrameText() const override;
  virtual void OnUpdateFrameTitle(BOOL addToTitle) override;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CChildFrame)
	public:
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  virtual LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override;
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CChildFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	//{{AFX_MSG(CChildFrame)
	afx_msg void OnClose();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	//}}AFX_MSG
	BOOL OnToolTipNeedText(UINT id, NMHDR * pNMHDR, LRESULT * pResult);
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
