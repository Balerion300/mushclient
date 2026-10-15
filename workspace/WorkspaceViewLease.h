#pragma once
#include <afxwin.h>
#include <vector>
#include <set>

// A short-lived guard across modal/plugin callbacks. It does not own the view.
// Native destruction removes the token even if Windows later reuses the HWND.
class WorkspaceViewLease {
  HWND window_;CView* view_;CDocument* document_;HANDLE token_;
  static LPCWSTR Property(){return L"Valyria.OriginalViewLifetime";}
public:
  explicit WorkspaceViewLease(CView* view):window_(view->GetSafeHwnd()),view_(view),document_(view->GetDocument()),token_(NULL){
    token_=::GetPropW(window_,Property());
    if(!token_){static LONG next=0;token_=(HANDLE)(UINT_PTR)(DWORD)InterlockedIncrement(&next);if(!::SetPropW(window_,Property(),token_))token_=NULL;}
  }
  bool IsLive() const {
    return token_ && ::IsWindow(window_) && ::GetPropW(window_,Property())==token_ &&
      CWnd::FromHandlePermanent(window_)==view_ && view_->GetDocument()==document_;
  }
};

// A frame may close one view while its document and another original view live.
// Keep snapshots of original view lifetimes, never a naked document dereference.
class WorkspaceDocumentLease {
  std::vector<WorkspaceViewLease> views_;
public:
  explicit WorkspaceDocumentLease(CDocument* document){
    for(POSITION pos=document->GetFirstViewPosition();pos;)views_.emplace_back(document->GetNextView(pos));
  }
  bool IsLive()const{for(const auto& view:views_)if(view.IsLive())return true;return false;}
};

// Submission is document-wide, including its optional New Window views. Pointer
// reuse during a nested callback is conservatively rejected until this scope ends.
class WorkspaceMainSubmission {
  const CDocument* document_;bool acquired_;
  static std::set<const CDocument*>& Active(){static std::set<const CDocument*> active;return active;}
public:
  static bool IsActive(const CDocument* document){return Active().count(document)!=0;}
  explicit WorkspaceMainSubmission(const CDocument* document):document_(document),acquired_(Active().insert(document).second){}
  ~WorkspaceMainSubmission(){if(acquired_)Active().erase(document_);}
  bool Acquired()const{return acquired_;}
  WorkspaceMainSubmission(const WorkspaceMainSubmission&)=delete;
  WorkspaceMainSubmission& operator=(const WorkspaceMainSubmission&)=delete;
};
