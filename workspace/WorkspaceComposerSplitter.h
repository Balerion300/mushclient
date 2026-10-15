#pragma once
#include <afxext.h>
#include <functional>
#include <string>
#include <set>
#include "../WorkspaceStyle.h"
#include "WorkspaceFocus.h"

namespace workspace_composer {
inline void ForEachOriginalFrame(CDocument& document,const std::function<void(CFrameWnd&)>& update){
  std::set<CFrameWnd*> seen;
  for(POSITION pos=document.GetFirstViewPosition();pos;){
    CView* view=document.GetNextView(pos);CFrameWnd* frame=view->GetParentFrame();
    if(frame && seen.insert(frame).second)update(*frame);
  }
}
}

// Adds chrome as siblings of the original command CView. The splitter's pane
// ID, parent, HWND, text buffer and all CEditView behavior remain unchanged.
class CWorkspaceComposerSplitter : public CSplitterWnd {
  HWND editor_=NULL,label_=NULL,send_=NULL;
  int row_=1;
  int requestedInputHeight_=-1;
  bool layingOut_=false;
  std::function<void()> submit_;
  static const UINT SendId=0x7b20,LabelId=0x7b21;
  bool OriginalEditor() const {
    return editor_ && ::IsWindow(editor_) && ::GetParent(editor_)==m_hWnd &&
      ::GetDlgCtrlID(editor_)==AFX_IDW_PANE_FIRST+row_*16;
  }
protected:
  LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp) override {
    if(message==WM_COMMAND && LOWORD(wp)==SendId && HIWORD(wp)==BN_CLICKED && (HWND)lp==send_){
      // Copy before dispatch: a plugin can close this entire frame synchronously.
      const auto submit=submit_;
      if(OriginalEditor() && ::IsWindowEnabled(send_) && submit)submit();
      return 0;
    }
    const LRESULT result=CSplitterWnd::WindowProc(message,wp,lp);
    if(message==WM_THEMECHANGED || message==0x02e3)RecalcLayout();
    return result;
  }
  void OnDrawSplitter(CDC* dc,ESplitType type,const CRect& rect) override {
    if(!dc || workspace_style::Colors(m_hWnd).highContrast){CSplitterWnd::OnDrawSplitter(dc,type,rect);return;}
    const auto palette=workspace_style::Colors(m_hWnd);
    dc->FillSolidRect(rect,palette.background);
    if(type==splitBar){const int y=rect.CenterPoint().y;dc->FillSolidRect(rect.left,y,rect.Width(),1,palette.border);}
  }
public:
  bool AttachComposer(int row,HWND editor,const std::function<void()>& submit){
    row_=row;editor_=editor;submit_=submit;
    if(!OriginalEditor())return false;
    label_=::CreateWindowExW(0,L"STATIC",L"Main input",WS_CHILD|WS_VISIBLE|SS_LEFT|SS_CENTERIMAGE|SS_ENDELLIPSIS|SS_NOPREFIX,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)LabelId,AfxGetInstanceHandle(),NULL);
    send_=::CreateWindowExW(0,L"BUTTON",L"Send",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)SendId,AfxGetInstanceHandle(),NULL);
    if(!label_ || !send_)return false;
    ModifyStyleEx(0,WS_EX_CONTROLPARENT);
    workspace_style::ApplySurface(m_hWnd);
    workspace_style::StyleControl(label_);
    workspace_style::StyleButton(send_,workspace_style::Icon::Send,true,L"Send this character's original Main input. Enter sends from the editor.");
    RecalcLayout();return true;
  }
  int ComposerOverhead() const {return label_?workspace_style::Scale(m_hWnd,48):0;}
  int RowHeightForInput(int height) const {return height>0?height+ComposerOverhead():0;}
  int InputHeightFromRow(int height) const {return max(0,height-ComposerOverhead());}
  int RestoreInputHeight(int saved,bool ready=true){if(requestedInputHeight_<0){if(!ready)return max(0,saved);requestedInputHeight_=max(0,saved);}return requestedInputHeight_;}
  void RememberInputHeight(int height){requestedInputHeight_=max(0,height);}
  void SetComposerDestination(const std::wstring& game,const std::wstring& character,bool connected){
    if(!label_)return;
    const std::wstring label=L"To "+character+(game.empty()?L"":L" / "+game)+L" / Main"+(connected?L"":L"  - Offline");
    wchar_t old[1024]={};::GetWindowTextW(label_,old,1024);if(label!=old)::SetWindowTextW(label_,label.c_str());
    ::EnableWindow(send_,connected);
  }
  void RecalcLayout() override {
    if(layingOut_ || !GetSafeHwnd())return;
    layingOut_=true;CSplitterWnd::RecalcLayout();
    if(OriginalEditor() && label_ && send_){
      RECT native;::GetWindowRect(editor_,&native);::MapWindowPoints(NULL,m_hWnd,(POINT*)&native,2);CRect area(native);
      const int pad=workspace_style::Scale(m_hWnd,8),button=workspace_style::Scale(m_hWnd,84),height=workspace_style::Scale(m_hWnd,32),overhead=ComposerOverhead();
      const bool visible=area.Height()>overhead;
      ::ShowWindow(label_,visible?SW_SHOW:SW_HIDE);::ShowWindow(send_,visible?SW_SHOW:SW_HIDE);
      if(visible){
        ::MoveWindow(label_,area.left+pad,area.top+pad,max(1,area.Width()-button-3*pad),height,TRUE);
        ::MoveWindow(send_,max(area.left,area.right-button-pad),area.top+pad,min(button,area.Width()),height,TRUE);
        ::MoveWindow(editor_,area.left+pad,area.top+overhead,max(1,area.Width()-2*pad),max(1,area.Height()-overhead),TRUE);
        WorkspaceFocusState::Trace("main-composer-body",editor_,WM_SIZE,(WPARAM)max(1,area.Height()-overhead));
      }
    }
    layingOut_=false;
  }
};
