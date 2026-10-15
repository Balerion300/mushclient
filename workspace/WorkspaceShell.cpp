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
std::wstring Caption(const ws::Row& row){std::wstring text=!row.channel && !row.characterLabel.empty()?row.characterLabel:row.label;
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
    const std::wstring status=exists?(world->gameLabel.empty()?L"":world->gameLabel+L" / ")+Caption(*world):L"Open a character profile to get started";wchar_t current[2048]={};::GetWindowTextW(status_,current,2048);if(status!=current)::SetWindowTextW(status_,status.c_str());
  }
  HWND Button(ws::Action action)const{const int index=action==ws::Logging?3:action==ws::Tools?5:0;return buttons_.empty()?NULL:buttons_[index];}
};
BEGIN_MESSAGE_MAP(ActionBar,CControlBar)
 ON_WM_SIZE()
 ON_WM_ERASEBKGND()
 ON_WM_PAINT()
END_MESSAGE_MAP()
class Navigator:public CDockablePane {
  CTreeCtrl tree_;HWND title_=NULL,open_=NULL;ws::Snapshot snapshot_;ws::Handler handler_;std::vector<HTREEITEM> items_;std::map<std::string,HTREEITEM> games_;bool updating_=false;
  CListBox rail_;std::vector<std::pair<std::string,std::wstring>> railGames_;HWND editor_=NULL,review_=NULL,brand_=NULL;
  int S(int value)const{return style::Scale(m_hWnd,value);}
protected:
  afx_msg void OnSize(UINT type,int cx,int cy){CDockablePane::OnSize(type,cx,cy);if(!tree_.m_hWnd)return;
    const int left=S(62);::MoveWindow(brand_,S(11),S(10),S(32),S(32),TRUE);rail_.MoveWindow(S(5),S(58),S(46),max(1,cy-S(72)));
    ::MoveWindow(title_,left+S(8),S(10),max(1,cx-left-S(16)),S(28),TRUE);tree_.MoveWindow(left,S(48),max(1,cx-left-S(6)),max(1,cy-S(174)));
    ::MoveWindow(editor_,left+S(5),max(S(48),cy-S(115)),max(1,cx-left-S(12)),S(32),TRUE);
    ::MoveWindow(review_,left+S(5),max(S(48),cy-S(78)),max(1,cx-left-S(12)),S(32),TRUE);
    ::MoveWindow(open_,left+S(5),max(S(48),cy-S(41)),max(1,cx-left-S(12)),S(32),TRUE);}
  afx_msg BOOL OnEraseBkgnd(CDC* dc){CRect rect;GetClientRect(rect);dc->FillSolidRect(rect,style::Colors(m_hWnd).surface);return TRUE;}
  afx_msg void OnSelect(NMHDR* header,LRESULT* result){*result=0;if(updating_)return;auto* event=(NMTREEVIEW*)header;const size_t index=(size_t)event->itemNew.lParam;if(index<snapshot_.rows.size()){const auto row=snapshot_.rows[index];handler_(ws::Select,row.world,row.name);if(event->action==TVC_BYKEYBOARD)tree_.SetFocus();}}
  afx_msg LRESULT OnAppearance(WPARAM,LPARAM){if(tree_.m_hWnd){const auto colors=style::Colors(m_hWnd);tree_.SetBkColor(colors.surface);tree_.SetTextColor(colors.text);tree_.SetLineColor(colors.border);tree_.SetFont(CFont::FromHandle(style::UiFont(m_hWnd)));tree_.SetItemHeight(S(42));}return 0;}
  afx_msg void OnTreeDraw(NMHDR* header,LRESULT* result){auto* draw=(NMTVCUSTOMDRAW*)header;*result=CDRF_DODEFAULT;
    if(draw->nmcd.dwDrawStage==CDDS_PREPAINT){*result=CDRF_NOTIFYITEMDRAW;return;}
    if(draw->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT)){const auto colors=style::Colors(m_hWnd);if((size_t)draw->nmcd.lItemlParam>=snapshot_.rows.size()){draw->clrText=colors.muted;draw->clrTextBk=colors.surface;return;}draw->clrText=draw->clrTextBk=(draw->nmcd.uItemState&CDIS_SELECTED)?colors.hover:colors.surface;*result=CDRF_NOTIFYPOSTPAINT;return;}
    if(draw->nmcd.dwDrawStage!=CDDS_ITEMPOSTPAINT)return;const size_t index=(size_t)draw->nmcd.lItemlParam;if(index>=snapshot_.rows.size())return;
    const auto& row=snapshot_.rows[index];CRect rect,client;if(!tree_.GetItemRect((HTREEITEM)draw->nmcd.dwItemSpec,rect,TRUE))return;tree_.GetClientRect(client);rect.right=client.right-S(5);rect.left=max(S(4),rect.left-S(1));
    const auto colors=style::Colors(m_hWnd);const bool selected=(draw->nmcd.uItemState&CDIS_SELECTED)!=0;HDC dc=draw->nmcd.hdc;HBRUSH brush=CreateSolidBrush(selected?colors.hover:colors.surface);FillRect(dc,rect,brush);DeleteObject(brush);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,colors.text);const HGDIOBJ old=SelectObject(dc,style::UiFont(m_hWnd));CRect label=rect;label.left+=S(4);
    if(row.channel){const auto text=Caption(row);DrawTextW(dc,text.c_str(),(int)text.size(),label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);}
    else{const auto& character=row.characterLabel.empty()?row.label:row.characterLabel;label.top+=S(2);label.bottom=label.top+S(19);DrawTextW(dc,character.c_str(),(int)character.size(),label,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
      std::wstring status=row.connected?L"Connected":row.connecting?L"Connecting":row.disconnecting?L"Disconnecting":L"Offline";if(row.logging)status+=L"  - Logging";if(row.unread)status+=L"  - "+Count(row.unread)+L" unread";
      label.top=rect.top+S(22);label.bottom=rect.bottom;SetTextColor(dc,colors.muted);DrawTextW(dc,status.c_str(),(int)status.size(),label,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);}
    if(selected && ::GetFocus()==tree_.m_hWnd){CRect focus=rect;focus.DeflateRect(1,1);DrawFocusRect(dc,focus);}SelectObject(dc,old);
  }
  afx_msg void OnGame(){const int selected=rail_.GetCurSel();if(updating_ || selected<0 || selected>=(int)railGames_.size())return;const auto game=games_.find(railGames_[selected].first);if(game==games_.end())return;
    tree_.Expand(game->second,TVE_EXPAND);
    // Game selection is a real navigation action. Select an exact original
    // character before updating the title; never leave old-game commands live.
    const auto first=std::find_if(snapshot_.rows.begin(),snapshot_.rows.end(),[&](const ws::Row& row){return !row.channel && row.game==railGames_[selected].first;});
    if(first==snapshot_.rows.end())return;
    const size_t index=(size_t)(first-snapshot_.rows.begin());
    const std::string world=first->world;
    updating_=true;tree_.SelectItem(items_[index]);tree_.SelectSetFirstVisible(game->second);updating_=false;
    handler_(ws::Select,world,"");
    rail_.Invalidate();}
  afx_msg void OnMeasureItem(int id,LPMEASUREITEMSTRUCT measure){
    if(id==TreeId+2){measure->itemHeight=S(52);return;}
    CDockablePane::OnMeasureItem(id,measure);
  }
  afx_msg void OnDrawItem(int id,LPDRAWITEMSTRUCT draw){if(id!=TreeId+2){CDockablePane::OnDrawItem(id,draw);return;}if(draw->itemID>=railGames_.size())return;const auto p=style::Colors(m_hWnd);const bool active=(draw->itemState&ODS_SELECTED)!=0;CRect r=draw->rcItem;HBRUSH base=CreateSolidBrush(p.background);FillRect(draw->hDC,r,base);DeleteObject(base);r.DeflateRect(S(3),S(4));HBRUSH fill=CreateSolidBrush(active?p.pressed:p.surface);FillRect(draw->hDC,r,fill);DeleteObject(fill);
    if(active){CRect line=r;line.right=line.left+S(3);HBRUSH brush=CreateSolidBrush(p.accent);FillRect(draw->hDC,line,brush);DeleteObject(brush);}const auto label=railGames_[draw->itemID].second;std::wstring initials;if(!label.empty())initials+=towupper(label[0]);for(size_t i=1;i<label.size()&&initials.size()<2;++i)if(label[i-1]==L' '&&label[i]!=L' ')initials+=towupper(label[i]);if(initials.size()<2&&label.size()>1)initials+=towupper(label[1]);const auto old=SelectObject(draw->hDC,style::UiFont(m_hWnd));SetBkMode(draw->hDC,TRANSPARENT);SetTextColor(draw->hDC,active?p.accent:p.muted);DrawTextW(draw->hDC,initials.c_str(),(int)initials.size(),r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);if(draw->itemState&ODS_FOCUS){r.DeflateRect(2,2);DrawFocusRect(draw->hDC,r);}SelectObject(draw->hDC,old);}
  virtual BOOL OnCommand(WPARAM wp,LPARAM lp){if(HIWORD(wp)==BN_CLICKED){if(LOWORD(wp)==TreeId+1){handler_(ws::OpenWorld,"","");return TRUE;}if(LOWORD(wp)==TreeId+3 || LOWORD(wp)==TreeId+4){handler_(LOWORD(wp)==TreeId+3?ws::Editor:ws::Review,snapshot_.world,snapshot_.name);return TRUE;}}return CDockablePane::OnCommand(wp,lp);}
  afx_msg void OnUpdateOpen(CCmdUI* command){command->Enable(command->m_nID!=TreeId+3 || !snapshot_.world.empty());}
  virtual void GetMinSize(CSize& size)const{size=CSize(S(250),S(260));}
  DECLARE_MESSAGE_MAP()
public:
  bool Create(CMDIFrameWndEx& frame,const ws::Handler& handler){handler_=handler;
    if(!CDockablePane::Create("Valyria",&frame,CRect(0,0,314,600),TRUE,NavigatorId,WS_CHILD|WS_VISIBLE|CBRS_LEFT|CBRS_HIDE_INPLACE|CBRS_FLOAT_MULTI))return false;
    EnableDocking(CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);ModifyStyleEx(0,WS_EX_CONTROLPARENT);style::ApplySurface(m_hWnd);
    title_=::CreateWindowExW(0,L"STATIC",L"Your characters",WS_CHILD|WS_VISIBLE|SS_LEFT|SS_CENTERIMAGE|SS_NOPREFIX,0,0,1,1,m_hWnd,NULL,AfxGetInstanceHandle(),NULL);style::StyleControl(title_);
    brand_=::CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_ICON|SS_CENTERIMAGE,0,0,1,1,m_hWnd,NULL,AfxGetInstanceHandle(),NULL);
    HICON mark=(HICON)::LoadImage(::GetModuleHandle(NULL),MAKEINTRESOURCE(128),IMAGE_ICON,S(32),S(32),LR_SHARED);if(mark)::SendMessage(brand_,STM_SETICON,(WPARAM)mark,0);
    if(!rail_.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT|WS_VSCROLL,CRect(0,0,1,1),this,TreeId+2))return false;
    rail_.SetItemHeight(0,S(52));style::StyleControl(rail_.m_hWnd);::SetWindowTextW(rail_.m_hWnd,L"Games");
    if(!tree_.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|TVS_HASBUTTONS|TVS_SHOWSELALWAYS|TVS_FULLROWSELECT|TVS_INFOTIP|TVS_NOHSCROLL,CRect(0,0,1,1),this,TreeId))return false;
    style::StyleControl(tree_.m_hWnd);OnAppearance(0,0);
    open_=::CreateWindowExW(0,L"BUTTON",L"&Open profile...",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)(TreeId+1),AfxGetInstanceHandle(),NULL);style::StyleButton(open_,style::Icon::Add,false,L"Open a world profile for your character");
    editor_=::CreateWindowExW(0,L"BUTTON",L"Script editor",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)(TreeId+3),AfxGetInstanceHandle(),NULL);style::StyleButton(editor_,style::Icon::Editor);
    review_=::CreateWindowExW(0,L"BUTTON",L"Agent review",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,1,1,m_hWnd,(HMENU)(UINT_PTR)(TreeId+4),AfxGetInstanceHandle(),NULL);style::StyleButton(review_,style::Icon::Agent);
    CRect rect;GetClientRect(rect);OnSize(0,rect.Width(),rect.Height());return true;}
  void Update(const ws::Snapshot& snapshot){updating_=true;bool rebuild=snapshot.rows.size()!=snapshot_.rows.size();if(!rebuild)for(size_t i=0;i<snapshot.rows.size();++i)if(snapshot.rows[i].world!=snapshot_.rows[i].world || snapshot.rows[i].name!=snapshot_.rows[i].name || snapshot.rows[i].channel!=snapshot_.rows[i].channel || snapshot.rows[i].game!=snapshot_.rows[i].game || snapshot.rows[i].gameLabel!=snapshot_.rows[i].gameLabel){rebuild=true;break;}
    std::set<std::string> collapsed;if(rebuild)for(size_t i=0;i<snapshot_.rows.size();++i)if(!snapshot_.rows[i].channel && !(tree_.GetItemState(items_[i],TVIS_EXPANDED)&TVIS_EXPANDED))collapsed.insert(snapshot_.rows[i].world);
    std::set<std::string> collapsedGames;if(rebuild)for(const auto& game:games_)if(!(tree_.GetItemState(game.second,TVIS_EXPANDED)&TVIS_EXPANDED))collapsedGames.insert(game.first);
    if(rebuild){tree_.SetRedraw(FALSE);tree_.DeleteAllItems();items_.clear();games_.clear();std::map<std::string,HTREEITEM> worlds;
      for(size_t i=0;i<snapshot.rows.size();++i){const auto& row=snapshot.rows[i];HTREEITEM parent=row.channel && worlds.count(row.world)?worlds[row.world]:TVI_ROOT;
        if(!row.channel && !row.game.empty()){auto game=games_.find(row.game);if(game==games_.end()){HTREEITEM group=tree_.InsertItem("",TVI_ROOT);tree_.SetItemData(group,(DWORD_PTR)-1);std::wstring label=row.gameLabel.empty()?L"Game":row.gameLabel;TVITEMW data={};data.mask=TVIF_TEXT;data.hItem=group;data.pszText=const_cast<wchar_t*>(label.c_str());::SendMessageW(tree_.m_hWnd,TVM_SETITEMW,0,(LPARAM)&data);game=games_.insert(std::make_pair(row.game,group)).first;}parent=game->second;}
        HTREEITEM item=tree_.InsertItem("",parent);tree_.SetItemData(item,i);items_.push_back(item);if(!row.channel)worlds[row.world]=item;}
      for(const auto& world:worlds)if(!collapsed.count(world.first))tree_.Expand(world.second,TVE_EXPAND);
      for(const auto& game:games_)if(!collapsedGames.count(game.first))tree_.Expand(game.second,TVE_EXPAND);
    }
    for(size_t i=0;i<snapshot.rows.size();++i){const std::wstring text=Caption(snapshot.rows[i]);TVITEMW item={0};item.mask=TVIF_TEXT;item.hItem=items_[i];wchar_t old[2048]={};item.pszText=old;item.cchTextMax=2048;::SendMessageW(tree_.m_hWnd,TVM_GETITEMW,0,(LPARAM)&item);
      if(text!=old){item.pszText=const_cast<wchar_t*>(text.c_str());::SendMessageW(tree_.m_hWnd,TVM_SETITEMW,0,(LPARAM)&item);}}
    if((rebuild || snapshot.world!=snapshot_.world || snapshot.name!=snapshot_.name) && ::GetFocus()!=tree_.m_hWnd)for(size_t i=0;i<snapshot.rows.size();++i)if(snapshot.rows[i].world==snapshot.world && snapshot.rows[i].name==snapshot.name){tree_.SelectItem(items_[i]);break;}
    if(rebuild){rail_.ResetContent();railGames_.clear();for(const auto& entry:games_){for(const auto& row:snapshot.rows)if(!row.channel && row.game==entry.first){railGames_.push_back({entry.first,row.gameLabel});::SendMessageW(rail_.m_hWnd,LB_ADDSTRING,0,(LPARAM)row.gameLabel.c_str());break;}}
      tree_.SetRedraw(TRUE);tree_.Invalidate();}
    if(rebuild || snapshot.world!=snapshot_.world){for(const auto& row:snapshot.rows)if(!row.channel && row.world==snapshot.world){for(size_t i=0;i<railGames_.size();++i)if(railGames_[i].first==row.game){rail_.SetCurSel((int)i);::SetWindowTextW(title_,row.gameLabel.c_str());}break;}}
    ::EnableWindow(editor_,!snapshot.world.empty());snapshot_=snapshot;updating_=false;
  }
};
BEGIN_MESSAGE_MAP(Navigator,CDockablePane)
 ON_UPDATE_COMMAND_UI_RANGE(TreeId+1,TreeId+4,OnUpdateOpen)
 ON_LBN_SELCHANGE(TreeId+2,OnGame)
 ON_WM_DRAWITEM()
 ON_WM_MEASUREITEM()
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
