// activitychildfrm.h : interface of the CActivityChildFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma push_macro("Translate")
#undef Translate
#include <afxmdichildwndex.h>
#pragma pop_macro("Translate")
#include "WorkspaceResources.h"

class CActivityChildFrame : public CMDIChildWndEx
{
	DECLARE_DYNCREATE(CActivityChildFrame)
public:
	CActivityChildFrame();

// Attributes
protected:
  
// Operations
public:

  virtual void OnUpdateFrameTitle(BOOL addToTitle) override;
  virtual LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override;
  virtual void OnUpdateFrameMenu(BOOL bActivate, CWnd* pActivateWnd, HMENU hMenuAlt);

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CActivityChildFrame)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CActivityChildFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	//{{AFX_MSG(CActivityChildFrame)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
