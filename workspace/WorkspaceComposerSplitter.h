#pragma once
#include <afxext.h>
#include "../resource.h"
#include <functional>
#include <string>
#include <set>
#include <map>
#include "../WorkspaceStyle.h"
#include "WorkspaceFocus.h"

class CWorkspaceComposerSplitter;
namespace workspace_composer {
inline std::map<HWND,CWorkspaceComposerSplitter*>& BorrowedEditors(){static std::map<HWND,CWorkspaceComposerSplitter*> editors;return editors;}
inline void ReleaseEditor(HWND editor);
inline BOOL PreTranslateInput(MSG* message);
inline bool CompositionActive(HWND window){
  HMODULE module=GetModuleHandleW(L"imm32.dll");if(!module)return false;
  auto context=(HANDLE(WINAPI*)(HWND))GetProcAddress(module,"ImmGetContext");
  auto length=(LONG(WINAPI*)(HANDLE,DWORD,LPVOID,DWORD))GetProcAddress(module,"ImmGetCompositionStringW");
  auto release=(BOOL(WINAPI*)(HWND,HANDLE))GetProcAddress(module,"ImmReleaseContext");
  if(!context || !length || !release)return false;
  HANDLE current=context(window);if(!current)return false;
  const bool composing=length(current,8,NULL,0)>0;release(window,current);return composing;
}
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
  HWND editor_=NULL,label_=NULL,send_=NULL,prefix_=NULL,size_=NULL;
  std::function<void(int)> heightPolicy_;
  int heightMode_=-1;
  bool heightInitialized_=false,mainHistory_=false;
  HWND borrowed_=NULL,home_=NULL;
  std::wstring mainLabel_,mainPrefix_=L"None (full command)",channelLabel_,channelPrefix_;
  bool connected_=false;
  std::function<void()> channelSubmit_;
  std::function<BOOL(MSG*)> channelKeys_;
  int row_=1;
  int requestedInputHeight_=-1;
  bool layingOut_=false;
  std::function<void()> submit_;
  static const UINT SendId=0x7b20,LabelId=0x7b21;
  bool OriginalEditor() const {
    return editor_ && ::IsWindow(editor_) && ::GetParent(editor_)==m_hWnd &&
      ::GetDlgCtrlID(editor_)==AFX_IDW_PANE_FIRST+row_*16;
  }
  void UpdateDestination(){
    if(label_)::SetWindowTextW(label_,(borrowed_?channelLabel_:mainLabel_).c_str());
    if(prefix_)::SetWindowTextW(prefix_,(L"Prefix: "+(borrowed_?channelPrefix_:(mainHistory_?L"History: full command (aliases bypassed)":mainPrefix_))).c_str());
    if(send_)::EnableWindow(send_,connected_);
  }
protected:
  LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp) override {
    if(message==WM_DESTROY)ReleaseChannelEditor();
    if(message==WM_COMMAND && LOWORD(wp)==LabelId+2 && HIWORD(wp)==BN_CLICKED){
      HMENU menu=::CreatePopupMenu();const int lines[]={0,1,2,3,6,10,-1};
      const wchar_t* names[]={L"Auto-grow (1 to 10 lines)",L"1 line",L"2 lines",L"3 lines",L"6 lines",L"10 lines",L"Custom (drag divider)"};
      for(int i=0;i<7;++i)::AppendMenuW(menu,MF_STRING|(heightMode_==lines[i]?MF_CHECKED:0),0x7c00+i,names[i]);
      RECT rect;::GetWindowRect(size_,&rect);const int choice=::TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,rect.left,rect.bottom,0,m_hWnd,NULL);::DestroyMenu(menu);
      if(choice>=0x7c00 && choice<0x7c07){SetInputHeightMode(lines[choice-0x7c00]);}
      return 0;
    }
    if(message==WM_COMMAND && HIWORD(wp)==EN_CHANGE && (HWND)lp==CurrentInput())UpdateInputHeight();
    if(message==WM_COMMAND && LOWORD(wp)==SendId && HIWORD(wp)==BN_CLICKED && (HWND)lp==send_){
      // Copy before dispatch: a plugin can close this entire frame synchronously.
      const auto submit=borrowed_?channelSubmit_:submit_;
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
  // CFrameWnd otherwise routes menu editing to its retained, hidden Main view.
  bool RouteEditCommand(UINT id,int code,void* extra,AFX_CMDHANDLERINFO* info,HWND target=NULL){
    if(!borrowed_ || !::IsWindow(borrowed_))return false;
    const HWND edit=target?target:borrowed_;
    UINT native=0;bool enabled=true;DWORD first=0,last=0;
    ::SendMessageW(edit,EM_GETSEL,(WPARAM)&first,(LPARAM)&last);
    const bool writable=(::GetWindowLongPtrW(edit,GWL_STYLE)&ES_READONLY)==0;
    switch(id){
    case ID_EDIT_COPY:native=WM_COPY;enabled=first!=last;break;
    case ID_EDIT_CUT:native=WM_CUT;enabled=writable && first!=last;break;
    case ID_EDIT_CLEAR:native=WM_CLEAR;enabled=writable && first!=last;break;
    case ID_EDIT_PASTE:native=WM_PASTE;enabled=writable && (::IsClipboardFormatAvailable(CF_UNICODETEXT)||::IsClipboardFormatAvailable(CF_TEXT));break;
    case (UINT)(WORD)ID_EDIT_CTRL_Z:
    case ID_EDIT_UNDO:native=WM_UNDO;enabled=writable && ::SendMessageW(edit,EM_CANUNDO,0,0);break;
    case ID_EDIT_SELECT_ALL:native=EM_SETSEL;enabled=::GetWindowTextLengthW(edit)>0;break;
    case ID_KEYS_ESCAPE:
      if(code==CN_UPDATE_COMMAND_UI){if(extra)static_cast<CCmdUI*>(extra)->Enable(writable);return true;}
      if(code==CN_COMMAND && !info){if(writable)::SetWindowTextW(edit,L"");return true;}return false;
    case ID_KEYS_PREVCOMMAND:case ID_ALT_UPARROW:
    case ID_KEYS_NEXTCOMMAND:case ID_ALT_DOWNARROW:
      if(code==CN_UPDATE_COMMAND_UI){if(extra)static_cast<CCmdUI*>(extra)->Enable(TRUE);return true;}
      if(code==CN_COMMAND && !info){MSG message={};message.hwnd=borrowed_;message.message=WM_KEYDOWN;message.wParam=(id==ID_KEYS_PREVCOMMAND || id==ID_ALT_UPARROW)?VK_UP:VK_DOWN;DispatchChannelKeys(&message);return true;}return false;
    // These legacy Main-only transformations have no channel equivalent yet.
    // Consume their accelerators so they cannot silently edit the retained Main draft.
    case ID_KEYS_TAB:case ID_INPUT_GLOBALCHANGE:case ID_EDIT_SPELLCHECK:
    case ID_EDIT_GENERATECHARACTERNAME:case ID_REPEAT_LAST_WORD:
    case ID_COMMAND_HOME:case ID_COMMAND_END:case ID_COMPLETE_FUNCTION:
    case ID_EDIT_GOTOMATCHINGBRACE:case ID_EDIT_SELECTTOMATCHINGBRACE:
    case ID_DISPLAY_CLEAR_COMMAND_HISTORY:case ID_REPEAT_LAST_COMMAND:
      if(code==CN_UPDATE_COMMAND_UI){if(extra)static_cast<CCmdUI*>(extra)->Enable(FALSE);return true;}
      return code==CN_COMMAND && !info;
    default:return false;
    }
    if(info)return false;
    if(code==CN_UPDATE_COMMAND_UI){if(extra)static_cast<CCmdUI*>(extra)->Enable(enabled);return true;}
    if(code!=CN_COMMAND)return false;
    if(enabled)::SendMessageW(edit,native,0,id==ID_EDIT_SELECT_ALL?-1:0);
    return true;
  }
  bool AttachComposer(int row,HWND editor,const std::function<void()>& submit){
    row_=row;editor_=editor;submit_=submit;
    if(!OriginalEditor())return false;
    label_=::CreateWindowExW(0,L"STATIC",L"Main input",WS_CHILD|WS_VISIBLE|SS_LEFT|SS_CENTERIMAGE|SS_ENDELLIPSIS|SS_NOPREFIX,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)LabelId,AfxGetInstanceHandle(),NULL);
    prefix_=::CreateWindowExW(0,L"STATIC",L"Prefix: None (full command)",WS_CHILD|WS_VISIBLE|SS_LEFT|SS_CENTERIMAGE|SS_ENDELLIPSIS|SS_NOPREFIX,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)(LabelId+1),AfxGetInstanceHandle(),NULL);
    send_=::CreateWindowExW(0,L"BUTTON",L"Send",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)SendId,AfxGetInstanceHandle(),NULL);
    size_=::CreateWindowExW(0,L"BUTTON",L"Size",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)(LabelId+2),AfxGetInstanceHandle(),NULL);
    if(!label_ || !send_ || !prefix_ || !size_)return false;
    ModifyStyleEx(0,WS_EX_CONTROLPARENT);
    workspace_style::ApplySurface(m_hWnd);
    workspace_style::StyleButton(size_,workspace_style::Icon::Settings,false,L"Input height: auto-grow, fixed lines, or drag the divider.");
    workspace_style::StyleControl(label_);
    workspace_style::StyleControl(prefix_);
    workspace_style::StyleButton(send_,workspace_style::Icon::Send,true,L"Send to the displayed character and channel. Enter sends from the editor.");
    RecalcLayout();return true;
  }
  void SetHeightPolicy(const std::function<void(int)>& apply){heightPolicy_=apply;}
  int HeightMode()const{return heightMode_;}
  void SetMainHistoryMode(bool full){mainHistory_=full;UpdateDestination();}
  void InitializeHeightMode(int mode){if(!heightInitialized_)SetInputHeightMode(mode,false);}
  void SetInputHeightMode(int mode,bool notify=true){
    if(mode<-1 || mode>10)return;
    heightInitialized_=true;heightMode_=mode;const auto apply=heightPolicy_;if(notify && apply)apply(mode);
    if(size_)::SetWindowTextW(size_,mode==0?L"Auto":mode<0?L"Size":(std::to_wstring(mode)+L" lines").c_str());
    UpdateInputHeight();
  }
  void UpdateInputHeight(){
    if(heightMode_<0 || layingOut_ || !OriginalEditor())return;
    const HWND input=CurrentInput();if(!input || !::IsWindow(input))return;
    int lines=heightMode_?heightMode_:max(1,min(10,(int)::SendMessageW(input,EM_GETLINECOUNT,0,0)));
    HDC dc=::GetDC(input);if(!dc)return;const auto font=(HFONT)::SendMessageW(input,WM_GETFONT,0,0);const auto old=font?::SelectObject(dc,font):NULL;
    TEXTMETRICW metric={};::GetTextMetricsW(dc,&metric);if(old)::SelectObject(dc,old);::ReleaseDC(input,dc);
    const int body=lines*max(1,(int)metric.tmHeight)+workspace_style::Scale(m_hWnd,8);
    requestedInputHeight_=body;CRect area;GetClientRect(&area);const int total=RowHeightForInput(body);
    int previous,minimum;GetRowInfo(row_,previous,minimum);if(previous==total)return;
    SetRowInfo(0,max(20,area.Height()-total-7),20);SetRowInfo(row_,total,ComposerOverhead()+9);RecalcLayout();
  }
  int ComposerOverhead() const {return label_?workspace_style::Scale(m_hWnd,48):0;}
  int RowHeightForInput(int height) const {return height>0?height+ComposerOverhead():0;}
  int InputHeightFromRow(int height) const {return max(0,height-ComposerOverhead());}
  int RestoreInputHeight(int saved,bool ready=true){if(requestedInputHeight_<0){if(!ready)return max(0,saved);requestedInputHeight_=max(0,saved);}return requestedInputHeight_;}
  void RememberInputHeight(int height){requestedInputHeight_=max(0,height);}
  void SetComposerDestination(const std::wstring& game,const std::wstring& character,bool connected,const std::wstring& mainPrefix=L"None (full command)"){
    if(!label_)return;
    const std::wstring label=L"To "+character+(game.empty()?L"":L" / "+game)+L" / Main"+(connected?L"":L"  - Offline");
    mainLabel_=label;mainPrefix_=mainPrefix;connected_=connected;UpdateDestination();
  }
  bool UseChannelEditor(HWND editor,HWND home,const std::wstring& label,const std::wstring& prefix,
                        const std::function<void()>& submit,const std::function<BOOL(MSG*)>& keys){
    if(!OriginalEditor() || !::IsWindow(editor) || !::IsWindow(home))return false;
    if(borrowed_!=editor){
      workspace_composer::ReleaseEditor(editor);ReleaseChannelEditor();
      if(::GetParent(editor)!=home)return false;
      ::SetParent(editor,m_hWnd);if(::GetParent(editor)!=m_hWnd)return false;
      borrowed_=editor;home_=home;workspace_composer::BorrowedEditors()[editor]=this;
      ::SetPropW(editor,L"Valyria.SharedComposerHost",m_hWnd);
    }
    channelLabel_=label;channelPrefix_=prefix.empty()?L"None (full command)":prefix;
    channelSubmit_=submit;channelKeys_=keys;UpdateDestination();RecalcLayout();UpdateInputHeight();return true;
  }
  void ReleaseChannelEditor(){
    if(!borrowed_)return;const HWND editor=borrowed_,home=home_;
    workspace_composer::BorrowedEditors().erase(editor);
    borrowed_=home_=NULL;channelSubmit_=nullptr;channelKeys_=nullptr;
    if(::IsWindow(editor) && ::GetPropW(editor,L"Valyria.SharedComposerHost")==m_hWnd){
      ::RemovePropW(editor,L"Valyria.SharedComposerHost");::ShowWindow(editor,SW_HIDE);
      if(::IsWindow(home)){::SetParent(editor,home);::InvalidateRect(home,NULL,FALSE);}
    }
    if(OriginalEditor())::ShowWindow(editor_,SW_SHOWNA);
    UpdateDestination();if(::IsWindow(m_hWnd))RecalcLayout();
  }
  BOOL DispatchChannelKeys(MSG* message){
    const auto handler=channelKeys_;
    if(!borrowed_ || message->hwnd!=borrowed_ || ::GetPropW(borrowed_,L"Valyria.SharedComposerHost")!=m_hWnd)return FALSE;
    return handler?handler(message):FALSE;
  }
  HWND CurrentInput()const{return borrowed_?borrowed_:editor_;}
  bool HasChannelEditor()const{return borrowed_!=NULL;}
  void RecalcLayout() override {
    if(layingOut_ || !GetSafeHwnd())return;
    layingOut_=true;CSplitterWnd::RecalcLayout();
    if(OriginalEditor() && label_ && send_){
      RECT native;::GetWindowRect(editor_,&native);::MapWindowPoints(NULL,m_hWnd,(POINT*)&native,2);CRect area(native);
      const int pad=workspace_style::Scale(m_hWnd,8),button=workspace_style::Scale(m_hWnd,84),sizeWidth=workspace_style::Scale(m_hWnd,54),height=workspace_style::Scale(m_hWnd,32),overhead=ComposerOverhead();
      const bool visible=area.Height()>overhead;
      ::ShowWindow(label_,visible?SW_SHOW:SW_HIDE);::ShowWindow(prefix_,visible?SW_SHOW:SW_HIDE);::ShowWindow(send_,visible?SW_SHOW:SW_HIDE);::ShowWindow(size_,visible?SW_SHOW:SW_HIDE);
      if(borrowed_)::ShowWindow(editor_,SW_HIDE);
      if(!visible && borrowed_ && ::IsWindow(borrowed_))::ShowWindow(borrowed_,SW_HIDE);
      if(visible){
        ::MoveWindow(label_,area.left+pad,area.top+pad,max(1,area.Width()-button-sizeWidth-4*pad),height/2,TRUE);
        ::MoveWindow(prefix_,area.left+pad,area.top+pad+height/2,max(1,area.Width()-button-sizeWidth-4*pad),height/2,TRUE);
        ::MoveWindow(size_,max(area.left,area.right-button-sizeWidth-2*pad),area.top+pad,min(sizeWidth,area.Width()),height,TRUE);
        ::MoveWindow(send_,max(area.left,area.right-button-pad),area.top+pad,min(button,area.Width()),height,TRUE);
        ::MoveWindow(editor_,area.left+pad,area.top+overhead,max(1,area.Width()-2*pad),max(1,area.Height()-overhead),TRUE);
        if(borrowed_ && ::IsWindow(borrowed_)){
          ::ShowWindow(editor_,SW_HIDE);
          ::MoveWindow(borrowed_,area.left+pad,area.top+overhead,max(1,area.Width()-2*pad),max(1,area.Height()-overhead),TRUE);
          ::ShowWindow(borrowed_,SW_SHOWNA);
        }
        WorkspaceFocusState::Trace("main-composer-body",editor_,WM_SIZE,(WPARAM)max(1,area.Height()-overhead));
      }
    }
    layingOut_=false;
  }
};

namespace workspace_composer {
inline void ReleaseEditor(HWND editor){auto found=BorrowedEditors().find(editor);if(found!=BorrowedEditors().end())found->second->ReleaseChannelEditor();}
inline BOOL PreTranslateInput(MSG* message){auto found=BorrowedEditors().find(message->hwnd);return found!=BorrowedEditors().end()?found->second->DispatchChannelKeys(message):FALSE;}
}
