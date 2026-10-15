#include "stdafx.h"
#include "WorkspaceShell.h"
#include "../WorkspaceStyle.h"
#include "../WorkspaceFrame.h"
#include <afxcmn.h>
#include <map>
#include <set>
#include <algorithm>

namespace ws=workspace_shell;
namespace style=workspace_style;
bool ws::ActivateOriginal(const std::string& identity,const std::function<Target(const std::string&)>& resolve,const std::function<void(HWND)>& activate,const std::function<Target()>& active)
{const Target before=resolve(identity);if(!before.window || !before.document)return false;activate(before.window);
  // Activation can synchronously run focus callbacks. Never use a pre-callback
  // document pointer, or trust whichever world those callbacks made active.
  const Target live=resolve(identity);if(!live.window || !live.document)return false;const Target current=active();return live.window==current.window && live.document==current.document;}
namespace {
const UINT BarId=0xe7a0,NavigatorId=0xe7a1,FirstButton=0x7800,TreeId=0x7820;
std::wstring Count(unsigned value){return std::to_wstring(value);}
std::wstring Caption(const ws::Row& row){std::wstring text=row.label;
  if(!row.channel){text+=row.connected?L"  - Connected":row.connecting?L"  - Connecting":row.disconnecting?L"  - Disconnecting":L"  - Offline";if(row.logging)text+=L"  - Logging";}
  if(row.unread)text+=L"  ("+Count(row.unread)+L" unread)";
  if(row.mentions)text+=L"  @"+Count(row.mentions);return text;}
class ActionBar:public CControlBar {
  std::vector<HWND> buttons_;HWND status_=NULL;ws::Snapshot snapshot_;ws::Handler handler_;
  int S(int value)const{return style::Scale(m_hWnd,value);}
  void Layout(){if(buttons_.empty())return;CRect bounds;GetClientRect(bounds);const int gap=S(6),height=S(34);int x=S(10),y=S(7);
    const int widths[]={94,122,82,110,116,90};
    for(size_t i=0;i<buttons_.size();++i){int width=S(widths[i]);if(x+width>bounds.Width()-S(10) && x>S(10)){x=S(10);y+=height+gap;}
      ::MoveWindow(buttons_[i],x,y,width,height,TRUE);x+=width+gap;}
    const int room=bounds.Width()-x-S(12);if(y==S(7) && room>S(160))::MoveWindow(status_,x+S(8),S(9),room-S(8),S(30),TRUE);
    else ::MoveWindow(status_,S(14),y+height+S(5),max(1,bounds.Width()-S(28)),S(22),TRUE);
  }
protected:
  afx_msg void OnSize(UINT type,int cx,int cy){CControlBar::OnSize(type,cx,cy);Layout();}
  afx_msg BOOL OnEraseBkgnd(CDC*){return TRUE;}
  afx_msg void OnPaint(){CPaintDC dc(this);CRect rect;GetClientRect(rect);const auto colors=style::Colors(m_hWnd);dc.FillSolidRect(rect,colors.surface);dc.FillSolidRect(0,rect.bottom-1,rect.Width(),1,colors.border);}
  virtual BOOL OnCommand(WPARAM wp,LPARAM lp){if(HIWORD(wp)==BN_CLICKED && LOWORD(wp)>=FirstButton && LOWORD(wp)<FirstButton+buttons_.size()){
      const ws::Action actions[]={ws::ToggleWorlds,ws::Connect,ws::Find,ws::Logging,ws::Channels,ws::Tools};const int index=LOWORD(wp)-FirstButton;
      ws::Action action=actions[index];if(index==1){const auto found=std::find_if(snapshot_.rows.begin(),snapshot_.rows.end(),[&](const ws::Row& row){return !row.channel && row.world==snapshot_.world;});if(found!=snapshot_.rows.end() && (found->connected || found->connecting))action=ws::Disconnect;}
      handler_(action,snapshot_.world,snapshot_.name);return TRUE;}return CControlBar::OnCommand(wp,lp);}
  DECLARE_MESSAGE_MAP()
public:
  virtual void OnUpdateCmdUI(CFrameWnd*,BOOL){} // Snapshot owns explicit enabled states.
  bool Create(CMDIFrameWndEx& frame,const ws::Handler& handler){handler_=handler;
    if(!CWnd::Create(AfxRegisterWndClass(0,LoadCursor(NULL,IDC_ARROW)),"Daily actions",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|CBRS_TOP,CRect(0,0,900,50),&frame,BarId))return false;
    SetBarStyle(CBRS_TOP|CBRS_SIZE_FIXED);SetOwner(&frame);ModifyStyleEx(0,WS_EX_CONTROLPARENT);style::ApplySurface(m_hWnd);
    const wchar_t* labels[]={L"&Worlds",L"&Connect",L"&Find",L"&Logging",L"C&hannels",L"&Tools"};
    const style::Icon icons[]={style::Icon::Worlds,style::Icon::Connect,style::Icon::Find,style::Icon::Logging,style::Icon::Channels,style::Icon::Tools};
    const wchar_t* tips[]={L"Show or hide Worlds and channels",L"Connect the selected original world",L"Find in the selected transcript",L"Logging for the selected original world",L"Channels and tells for the selected original world",L"Editor, agents, scenes and appearance"};
    for(int i=0;i<6;++i){HWND button=::CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)(FirstButton+i),AfxGetInstanceHandle(),NULL);if(!button)return false;buttons_.push_back(button);style::StyleButton(button,icons[i],false,tips[i]);}
    status_=::CreateWindowExW(0,L"STATIC",L"Open a world to get started",WS_CHILD|WS_VISIBLE|SS_LEFT|SS_CENTERIMAGE,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)(FirstButton+6),AfxGetInstanceHandle(),NULL);style::StyleControl(status_);Layout();return true;}
  virtual CSize CalcFixedLayout(BOOL stretch,BOOL){CRect rect;if(GetParent())GetParent()->GetClientRect(rect);const int width=max(S(320),rect.Width());
    const int actionWidth=S(664);const int rows=width>=actionWidth?1:width>=S(360)?2:3;const bool inlineStatus=width>=actionWidth+S(190);
    return CSize(stretch?32767:width,S(14)+rows*S(34)+(rows-1)*S(6)+(inlineStatus?0:S(27)));}
  void Update(const ws::Snapshot& snapshot){snapshot_=snapshot;const auto world=std::find_if(snapshot.rows.begin(),snapshot.rows.end(),[&](const ws::Row& row){return !row.channel && row.world==snapshot.world;});
    const bool exists=world!=snapshot.rows.end(),disconnect=exists && (world->connected || world->connecting);const wchar_t* label=disconnect?L"&Disconnect":L"&Connect";
    wchar_t old[64]={};::GetWindowTextW(buttons_[1],old,64);if(wcscmp(old,label)){::SetWindowTextW(buttons_[1],label);style::StyleButton(buttons_[1],disconnect?style::Icon::Disconnect:style::Icon::Connect,false,disconnect?L"Disconnect the selected original world":L"Connect the selected original world");}
    for(int i:{1,2,3,4})::EnableWindow(buttons_[i],exists && (i!=1 || !world->disconnecting));
    const std::wstring status=exists?Caption(*world):L"Open a world to get started";wchar_t current[2048]={};::GetWindowTextW(status_,current,2048);if(status!=current)::SetWindowTextW(status_,status.c_str());
  }
  HWND Button(ws::Action action)const{const int index=action==ws::Logging?3:action==ws::Tools?5:0;return buttons_.empty()?NULL:buttons_[index];}
};
BEGIN_MESSAGE_MAP(ActionBar,CControlBar)
 ON_WM_SIZE()
 ON_WM_ERASEBKGND()
 ON_WM_PAINT()
END_MESSAGE_MAP()
class Navigator:public CDockablePane {
  CTreeCtrl tree_;HWND title_=NULL,open_=NULL;ws::Snapshot snapshot_;ws::Handler handler_;std::vector<HTREEITEM> items_;bool updating_=false;
  int S(int value)const{return style::Scale(m_hWnd,value);}
protected:
  afx_msg void OnSize(UINT type,int cx,int cy){CDockablePane::OnSize(type,cx,cy);if(!tree_.m_hWnd)return;::MoveWindow(title_,S(12),S(8),max(1,cx-S(24)),S(28),TRUE);tree_.MoveWindow(S(6),S(42),max(1,cx-S(12)),max(1,cy-S(94)));::MoveWindow(open_,S(10),max(S(42),cy-S(42)),max(1,cx-S(20)),S(32),TRUE);}
  afx_msg BOOL OnEraseBkgnd(CDC* dc){CRect rect;GetClientRect(rect);dc->FillSolidRect(rect,style::Colors(m_hWnd).surface);return TRUE;}
  afx_msg void OnSelect(NMHDR* header,LRESULT* result){*result=0;if(updating_)return;auto* event=(NMTREEVIEW*)header;const size_t index=(size_t)event->itemNew.lParam;if(index<snapshot_.rows.size()){const auto row=snapshot_.rows[index];handler_(ws::Select,row.world,row.name);if(event->action==TVC_BYKEYBOARD)tree_.SetFocus();}}
  afx_msg LRESULT OnAppearance(WPARAM,LPARAM){if(tree_.m_hWnd){const auto colors=style::Colors(m_hWnd);tree_.SetBkColor(colors.surface);tree_.SetTextColor(colors.text);tree_.SetLineColor(colors.border);tree_.SetFont(CFont::FromHandle(style::UiFont(m_hWnd)));tree_.SetItemHeight(S(42));}return 0;}
  afx_msg void OnTreeDraw(NMHDR* header,LRESULT* result){auto* draw=(NMTVCUSTOMDRAW*)header;*result=CDRF_DODEFAULT;
    if(draw->nmcd.dwDrawStage==CDDS_PREPAINT){*result=CDRF_NOTIFYITEMDRAW;return;}
    if(draw->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT)){const auto colors=style::Colors(m_hWnd);draw->clrText=draw->clrTextBk=(draw->nmcd.uItemState&CDIS_SELECTED)?colors.hover:colors.surface;*result=CDRF_NOTIFYPOSTPAINT;return;}
    if(draw->nmcd.dwDrawStage!=CDDS_ITEMPOSTPAINT)return;const size_t index=(size_t)draw->nmcd.lItemlParam;if(index>=snapshot_.rows.size())return;
    const auto& row=snapshot_.rows[index];CRect rect,client;if(!tree_.GetItemRect((HTREEITEM)draw->nmcd.dwItemSpec,rect,TRUE))return;tree_.GetClientRect(client);rect.right=client.right-S(5);rect.left=max(S(4),rect.left-S(1));
    const auto colors=style::Colors(m_hWnd);const bool selected=(draw->nmcd.uItemState&CDIS_SELECTED)!=0;HDC dc=draw->nmcd.hdc;HBRUSH brush=CreateSolidBrush(selected?colors.hover:colors.surface);FillRect(dc,rect,brush);DeleteObject(brush);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,colors.text);const HGDIOBJ old=SelectObject(dc,style::UiFont(m_hWnd));CRect label=rect;label.left+=S(4);
    if(row.channel){const auto text=Caption(row);DrawTextW(dc,text.c_str(),(int)text.size(),label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);}
    else{label.top+=S(2);label.bottom=label.top+S(19);DrawTextW(dc,row.label.c_str(),(int)row.label.size(),label,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
      std::wstring status=row.connected?L"Connected":row.connecting?L"Connecting":row.disconnecting?L"Disconnecting":L"Offline";if(row.logging)status+=L"  - Logging";if(row.unread)status+=L"  - "+Count(row.unread)+L" unread";
      label.top=rect.top+S(22);label.bottom=rect.bottom;SetTextColor(dc,colors.muted);DrawTextW(dc,status.c_str(),(int)status.size(),label,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);}
    if(selected && ::GetFocus()==tree_.m_hWnd){CRect focus=rect;focus.DeflateRect(1,1);DrawFocusRect(dc,focus);}SelectObject(dc,old);
  }
  virtual BOOL OnCommand(WPARAM wp,LPARAM lp){if(LOWORD(wp)==TreeId+1 && HIWORD(wp)==BN_CLICKED){handler_(ws::OpenWorld,"","");return TRUE;}return CDockablePane::OnCommand(wp,lp);}
  afx_msg void OnUpdateOpen(CCmdUI* command){command->Enable(TRUE);}
  virtual void GetMinSize(CSize& size)const{size=CSize(S(190),S(200));}
  DECLARE_MESSAGE_MAP()
public:
  bool Create(CMDIFrameWndEx& frame,const ws::Handler& handler){handler_=handler;
    if(!CDockablePane::Create("Worlds and channels",&frame,CRect(0,0,260,600),TRUE,NavigatorId,WS_CHILD|WS_VISIBLE|CBRS_LEFT|CBRS_HIDE_INPLACE|CBRS_FLOAT_MULTI))return false;
    EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);ModifyStyleEx(0,WS_EX_CONTROLPARENT);style::ApplySurface(m_hWnd);
    title_=::CreateWindowExW(0,L"STATIC",L"Worlds & channels",WS_CHILD|WS_VISIBLE|SS_LEFT|SS_CENTERIMAGE|SS_NOPREFIX,0,0,1,1,m_hWnd,NULL,AfxGetInstanceHandle(),NULL);style::StyleControl(title_);
    if(!tree_.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS|TVS_FULLROWSELECT|TVS_INFOTIP|TVS_NOHSCROLL,CRect(0,0,1,1),this,TreeId))return false;
    style::StyleControl(tree_.m_hWnd);OnAppearance(0,0);
    open_=::CreateWindowExW(0,L"BUTTON",L"&Open a world...",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)(TreeId+1),AfxGetInstanceHandle(),NULL);style::StyleButton(open_,style::Icon::Add,false,L"Open a world file");CRect rect;GetClientRect(rect);OnSize(0,rect.Width(),rect.Height());return true;}
  void Update(const ws::Snapshot& snapshot){updating_=true;bool rebuild=snapshot.rows.size()!=snapshot_.rows.size();if(!rebuild)for(size_t i=0;i<snapshot.rows.size();++i)if(snapshot.rows[i].world!=snapshot_.rows[i].world || snapshot.rows[i].name!=snapshot_.rows[i].name || snapshot.rows[i].channel!=snapshot_.rows[i].channel){rebuild=true;break;}
    std::set<std::string> collapsed;if(rebuild)for(size_t i=0;i<snapshot_.rows.size();++i)if(!snapshot_.rows[i].channel && !(tree_.GetItemState(items_[i],TVIS_EXPANDED)&TVIS_EXPANDED))collapsed.insert(snapshot_.rows[i].world);
    if(rebuild){tree_.SetRedraw(FALSE);tree_.DeleteAllItems();items_.clear();std::map<std::string,HTREEITEM> worlds;
      for(size_t i=0;i<snapshot.rows.size();++i){const auto& row=snapshot.rows[i];HTREEITEM parent=row.channel && worlds.count(row.world)?worlds[row.world]:TVI_ROOT;
        HTREEITEM item=tree_.InsertItem("",parent);tree_.SetItemData(item,i);items_.push_back(item);if(!row.channel)worlds[row.world]=item;}
      for(const auto& world:worlds)if(!collapsed.count(world.first))tree_.Expand(world.second,TVE_EXPAND);
    }
    for(size_t i=0;i<snapshot.rows.size();++i){const std::wstring text=Caption(snapshot.rows[i]);TVITEMW item={0};item.mask=TVIF_TEXT;item.hItem=items_[i];wchar_t old[2048]={};item.pszText=old;item.cchTextMax=2048;::SendMessageW(tree_.m_hWnd,TVM_GETITEMW,0,(LPARAM)&item);
      if(text!=old){item.pszText=const_cast<wchar_t*>(text.c_str());::SendMessageW(tree_.m_hWnd,TVM_SETITEMW,0,(LPARAM)&item);}}
    if((rebuild || snapshot.world!=snapshot_.world || snapshot.name!=snapshot_.name) && ::GetFocus()!=tree_.m_hWnd)for(size_t i=0;i<snapshot.rows.size();++i)if(snapshot.rows[i].world==snapshot.world && snapshot.rows[i].name==snapshot.name){tree_.SelectItem(items_[i]);break;}
    if(rebuild){tree_.SetRedraw(TRUE);tree_.Invalidate();}snapshot_=snapshot;updating_=false;
  }
};
BEGIN_MESSAGE_MAP(Navigator,CDockablePane)
 ON_UPDATE_COMMAND_UI(TreeId+1,OnUpdateOpen)
 ON_WM_SIZE()
 ON_WM_ERASEBKGND()
 ON_NOTIFY(TVN_SELCHANGED,TreeId,OnSelect)
 ON_NOTIFY(NM_CUSTOMDRAW,TreeId,OnTreeDraw)
 ON_MESSAGE(WM_THEMECHANGED,OnAppearance)
END_MESSAGE_MAP()
}
struct CWorkspaceShell::Impl {CMDIFrameWndEx* frame=NULL;ActionBar bar;Navigator navigator;ws::Handler handler;};
CWorkspaceShell::CWorkspaceShell():impl_(new Impl){}
CWorkspaceShell::~CWorkspaceShell(){delete impl_;}
bool CWorkspaceShell::Create(CMDIFrameWndEx& frame,const ws::Handler& handler){impl_->frame=&frame;impl_->handler=handler;if(!impl_->bar.Create(frame,handler) || !impl_->navigator.Create(frame,handler))return false;frame.DockPane(&impl_->navigator,AFX_IDW_DOCKBAR_LEFT);return true;}
void CWorkspaceShell::Update(const ws::Snapshot& snapshot){impl_->bar.Update(snapshot);impl_->navigator.Update(snapshot);}
void CWorkspaceShell::ToggleNavigator(){ShowNavigator(!NavigatorVisible());}
bool CWorkspaceShell::NavigatorVisible()const{return impl_->navigator.GetSafeHwnd() && impl_->navigator.IsVisible();}
void CWorkspaceShell::ShowNavigator(bool show){impl_->navigator.ShowPane(show,FALSE,FALSE);impl_->frame->RecalcLayout();}
bool CWorkspaceShell::OwnsFocus()const{HWND focus=::GetFocus();return focus && (::IsChild(impl_->bar.m_hWnd,focus) || ::IsChild(impl_->navigator.m_hWnd,focus) || focus==impl_->navigator.m_hWnd);}
bool CWorkspaceShell::PreTranslate(MSG* message){if(!OwnsFocus())return false;const bool bar=::IsChild(impl_->bar.m_hWnd,message->hwnd);HWND container=bar?impl_->bar.m_hWnd:impl_->navigator.m_hWnd;
  if(message->message==WM_KEYDOWN && message->wParam==VK_RETURN){wchar_t name[32]={};::GetClassNameW(message->hwnd,name,32);if(!_wcsicmp(name,L"Button")){::SendMessageW(message->hwnd,BM_CLICK,0,0);return true;}}
  return ((message->message==WM_KEYDOWN && message->wParam==VK_TAB) || message->message==WM_SYSCHAR) && ::IsDialogMessageW(container,message)!=FALSE;}
