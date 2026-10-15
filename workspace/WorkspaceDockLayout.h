#pragma once

#include "../WorkspaceResources.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxmdiframewndex.h>
#include <afxpanedivider.h>
#include <afxpanecontainer.h>
#include <afxpanecontainermanager.h>
#include <afxmultipaneframewnd.h>
#include <afxautohidebar.h>
#include <afxtabbedpane.h>
#pragma pop_macro("Translate")
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// A portable, data-only forest. MFC archives contain runtime pane IDs and native
// object graphs; this format stores only persistent world/name identities.
namespace workspace_dock {
const size_t MaxPanes=256,MaxNodes=512,MaxBytes=262144,MaxDepth=128;
struct Key {
  std::string world,name;
  bool operator<(const Key& other)const{return world<other.world || (world==other.world && name<other.name);}
  bool operator==(const Key& other)const{return world==other.world && name==other.name;}
};
struct Entry {Key key;CDockablePane* pane;};
struct Node {
  int left=-1,right=-1,ratio=500,active=0;
  bool horizontal=false; // Divider is horizontal: top/bottom children.
  std::vector<Key> tabs;
};
struct Root {
  int node=-1;
  DWORD alignment=CBRS_ALIGN_RIGHT;
  bool floating=false,autoHide=false,visible=true;
  CRect bounds=CRect(80,120,580,470);
};
struct Forest {std::vector<Node> nodes;std::vector<Root> roots;};
inline bool Alignment(DWORD side){return side==CBRS_ALIGN_LEFT || side==CBRS_ALIGN_RIGHT || side==CBRS_ALIGN_TOP || side==CBRS_ALIGN_BOTTOM;}
inline bool ValidKey(const Key& key){return !key.world.empty() && key.world.size()<=128 && !key.name.empty() && key.name.size()<=512 &&
  key.world.find(":runtime:")==std::string::npos && key.world.find('\0')==std::string::npos && key.name.find('\0')==std::string::npos;}
inline bool Validate(const Forest& forest)
{
  if(forest.nodes.size()>MaxNodes || forest.roots.size()>MaxPanes)return false;
  std::set<Key> keys;std::set<int> visited;
  std::function<bool(int,size_t)> node=[&](int index,size_t depth){
    if(index<0 || (size_t)index>=forest.nodes.size() || depth>MaxDepth || !visited.insert(index).second)return false;
    const Node& value=forest.nodes[index];
    if(value.tabs.empty())return value.ratio>=1 && value.ratio<=999 && node(value.left,depth+1) && node(value.right,depth+1);
    if(value.left!=-1 || value.right!=-1 || value.tabs.size()>MaxPanes || value.active<0 || (size_t)value.active>=value.tabs.size())return false;
    for(const auto& key:value.tabs)if(!ValidKey(key) || !keys.insert(key).second || keys.size()>MaxPanes)return false;
    return true;
  };
  for(const auto& root:forest.roots){
    if(!Alignment(root.alignment) || (root.floating && root.autoHide) || root.bounds.left<-1000000 || root.bounds.left>1000000 ||
       root.bounds.top<-1000000 || root.bounds.top>1000000 || root.bounds.right<-1000000 || root.bounds.right>1000000 ||
       root.bounds.bottom<-1000000 || root.bounds.bottom>1000000 || root.bounds.Width()<1 || root.bounds.Width()>16384 ||
       root.bounds.Height()<1 || root.bounds.Height()>16384 || !node(root.node,0))return false;
    if(root.autoHide && forest.nodes[root.node].tabs.empty())return false;
  }
  return visited.size()==forest.nodes.size();
}
inline std::string Encode(const Forest& forest)
{
  if(!Validate(forest))return "";
  std::ostringstream out;out<<"WDL3\n"<<forest.nodes.size()<<' '<<forest.roots.size()<<'\n';
  const auto key=[&](const std::string& value){const char hex[]="0123456789abcdef";for(unsigned char c:value)out<<hex[c>>4]<<hex[c&15];out<<'\n';};
  for(const auto& node:forest.nodes){out<<node.left<<' '<<node.right<<' '<<node.ratio<<' '<<node.active<<' '<<node.horizontal<<' '<<node.tabs.size()<<'\n';for(const auto& tab:node.tabs){key(tab.world);key(tab.name);}}
  for(const auto& root:forest.roots)out<<root.node<<' '<<root.alignment<<' '<<root.floating<<' '<<root.autoHide<<' '<<root.visible<<' '<<root.bounds.left<<' '<<root.bounds.top<<' '<<root.bounds.right<<' '<<root.bounds.bottom<<'\n';
  const std::string result=out.str();return result.size()<=MaxBytes?result:"";
}
inline bool Decode(const std::string& data,Forest& result)
{
  if(data.size()>MaxBytes || data.compare(0,5,"WDL3\n")!=0)return false;
  std::istringstream in(data.substr(5));size_t nodes=0,roots=0;
  if(!(in>>nodes>>roots) || nodes>MaxNodes || roots>MaxPanes)return false;
  Forest forest;forest.nodes.resize(nodes);forest.roots.resize(roots);
  const auto key=[&](std::string& value){std::string hex;if(!(in>>hex) || hex.size()>1024 || hex.size()%2)return false;value.clear();
    const auto digit=[](char c){return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:-1;};
    for(size_t i=0;i<hex.size();i+=2){int a=digit(hex[i]),b=digit(hex[i+1]);if(a<0 || b<0 || (!a&&!b))return false;value+=(char)(a*16+b);}return true;};
  for(auto& node:forest.nodes){size_t count=0;int horizontal=0;
    if(!(in>>node.left>>node.right>>node.ratio>>node.active>>horizontal>>count) || count>MaxPanes || horizontal<0 || horizontal>1)return false;
    node.horizontal=horizontal!=0;node.tabs.resize(count);for(auto& tab:node.tabs)if(!key(tab.world) || !key(tab.name))return false;}
  for(auto& root:forest.roots){int floating=0,autoHide=0,visible=0;
    if(!(in>>root.node>>root.alignment>>floating>>autoHide>>visible>>root.bounds.left>>root.bounds.top>>root.bounds.right>>root.bounds.bottom) ||
       floating<0 || floating>1 || autoHide<0 || autoHide>1 || visible<0 || visible>1)return false;
    root.floating=floating!=0;root.autoHide=autoHide!=0;root.visible=visible!=0;}
  in>>std::ws;if(!in.eof() || !Validate(forest))return false;result=forest;return true;
}
inline std::set<Key> Keys(const Forest& forest,int index)
{
  std::set<Key> result;const Node& node=forest.nodes[index];for(const auto& key:node.tabs)result.insert(key);
  if(node.tabs.empty()){const auto a=Keys(forest,node.left),b=Keys(forest,node.right);result.insert(a.begin(),a.end());result.insert(b.begin(),b.end());}return result;
}
inline int ProjectNode(const Forest& source,int index,const std::set<Key>& keep,Forest& target)
{
  Node node=source.nodes[index];
  if(node.tabs.empty()){node.left=ProjectNode(source,node.left,keep,target);node.right=ProjectNode(source,node.right,keep,target);
    if(node.left<0)return node.right;if(node.right<0)return node.left;}
  else{const Key active=node.tabs[node.active];node.tabs.erase(std::remove_if(node.tabs.begin(),node.tabs.end(),[&](const Key& key){return !keep.count(key);}),node.tabs.end());
    if(node.tabs.empty())return -1;const auto found=std::find(node.tabs.begin(),node.tabs.end(),active);node.active=found==node.tabs.end()?0:(int)(found-node.tabs.begin());}
  target.nodes.push_back(node);return (int)target.nodes.size()-1;
}
inline Forest Project(const Forest& source,const std::set<Key>& keep)
{Forest result;for(auto root:source.roots){root.node=ProjectNode(source,root.node,keep,result);if(root.node>=0)result.roots.push_back(root);}return result;}
inline void Append(Forest& target,const Forest& source)
{const int offset=(int)target.nodes.size();for(auto node:source.nodes){if(node.left>=0)node.left+=offset;if(node.right>=0)node.right+=offset;target.nodes.push_back(node);}for(auto root:source.roots){root.node+=offset;target.roots.push_back(root);}}

inline CDockablePane* Owner(CDockablePane* pane)
{CBaseTabbedPane* group=pane->GetParentTabbedPane();return group?static_cast<CDockablePane*>(group):pane;}
inline CPaneContainer* LeafContainer(CDockablePane* pane)
{
  BOOL left=FALSE;CPaneContainer* node=NULL;auto* mini=DYNAMIC_DOWNCAST(CMultiPaneFrameWnd,pane->GetParentMiniFrame());
  if(mini)node=mini->GetPaneContainerManager().FindPaneContainer(pane,left);
  else if(pane->GetDefaultPaneDivider())node=pane->GetDefaultPaneDivider()->FindPaneContainer(pane,left);
  return node;
}
inline CPaneContainer* Container(CDockablePane* pane)
{auto* node=LeafContainer(pane);while(node && node->GetParentPaneContainer())node=node->GetParentPaneContainer();return node;}
inline int CaptureLeaf(CDockablePane* pane,const std::map<CDockablePane*,Key>& entries,Forest& forest)
{
  Node node;auto* group=DYNAMIC_DOWNCAST(CBaseTabbedPane,pane);
  if(group && group->GetUnderlyingWindow()){auto* tabs=group->GetUnderlyingWindow();
    for(int i=0;i<tabs->GetTabsNum();++i){auto* leaf=DYNAMIC_DOWNCAST(CDockablePane,tabs->GetTabWnd(i));const auto key=entries.find(leaf);
      if(key!=entries.end()){if(i==tabs->GetActiveTab())node.active=(int)node.tabs.size();node.tabs.push_back(key->second);}}}
  else{const auto key=entries.find(pane);if(key!=entries.end())node.tabs.push_back(key->second);}
  if(node.tabs.empty())return -1;forest.nodes.push_back(node);return (int)forest.nodes.size()-1;
}
inline int CaptureNode(const CPaneContainer* source,const std::map<CDockablePane*,Key>& entries,Forest& forest,size_t depth=0)
{
  if(!source || depth>MaxDepth || forest.nodes.size()>=MaxNodes)return -1;
  const int left=source->GetLeftPane()?CaptureLeaf(const_cast<CDockablePane*>(source->GetLeftPane()),entries,forest):CaptureNode(source->GetLeftPaneContainer(),entries,forest,depth+1);
  const int right=source->GetRightPane()?CaptureLeaf(const_cast<CDockablePane*>(source->GetRightPane()),entries,forest):CaptureNode(source->GetRightPaneContainer(),entries,forest,depth+1);
  if(left<0)return right;if(right<0)return left;
  Node node;node.left=left;node.right=right;node.horizontal=source->GetPaneDivider()?source->GetPaneDivider()->IsHorizontal()!=FALSE:(source->GetRecentPaneDividerStyle()&CPaneDivider::SS_HORZ)!=0;
  CRect all,part;source->GetWindowRect(all,TRUE);
  if(source->GetLeftPane())source->GetLeftPane()->GetWindowRect(part);else if(source->GetLeftPaneContainer())source->GetLeftPaneContainer()->GetWindowRect(part,TRUE);
  const int total=node.horizontal?all.Height():all.Width(),first=node.horizontal?part.Height():part.Width();
  node.ratio=max(1,min(999,total>0?first*1000/total:500));forest.nodes.push_back(node);return (int)forest.nodes.size()-1;
}
inline Forest Capture(const std::vector<Entry>& entries)
{
  Forest forest;std::map<CDockablePane*,Key> keys;std::set<Key> identities;if(entries.size()>MaxPanes)return forest;
  for(const auto& entry:entries)if(!ValidKey(entry.key) || !entry.pane || !entry.pane->GetSafeHwnd() || !keys.insert(std::make_pair(entry.pane,entry.key)).second || !identities.insert(entry.key).second)return Forest();
  std::set<const void*> captured;std::vector<Entry> hidden;
  for(const auto& entry:entries){auto* pane=entry.pane;if(!pane || !pane->GetSafeHwnd())continue;
    if(pane->IsAutoHideMode()){hidden.push_back(entry);continue;}
    auto* owner=Owner(pane);const auto* container=Container(owner);const void* identity=container?(const void*)container:(const void*)owner;
    if(!captured.insert(identity).second)continue;
    Root root;root.node=container?CaptureNode(container,keys,forest):CaptureLeaf(owner,keys,forest);if(root.node<0)continue;
    root.alignment=owner->GetCurrentAlignment();if(!Alignment(root.alignment))root.alignment=CBRS_ALIGN_RIGHT;
    // MFC IsFloating() returns FALSE for a pane sharing a floating mini-frame.
    root.floating=owner->GetParentMiniFrame()!=NULL;root.visible=owner->IsVisible()!=FALSE;
    if(root.floating && owner->GetParentMiniFrame())owner->GetParentMiniFrame()->GetWindowRect(root.bounds);
    else if(container)container->GetWindowRect(root.bounds,TRUE);else owner->GetWindowRect(root.bounds);
    root.bounds.right=root.bounds.left+max(1,min(16384,root.bounds.Width()));root.bounds.bottom=root.bounds.top+max(1,min(16384,root.bounds.Height()));forest.roots.push_back(root);
  }
  // Native auto-hide groups are sibling toolbar buttons, not CTabbedPane
  // windows. Preserve their order, grouping and selected member explicitly.
  std::sort(hidden.begin(),hidden.end(),[](const Entry& a,const Entry& b){auto* x=a.pane->GetAutoHideToolBar();auto* y=b.pane->GetAutoHideToolBar();
    if(a.pane->GetCurrentAlignment()!=b.pane->GetCurrentAlignment())return a.pane->GetCurrentAlignment()<b.pane->GetCurrentAlignment();
    CRect p,q;x->GetWindowRect(p);y->GetWindowRect(q);return p.top!=q.top?p.top<q.top:p.left<q.left;});
  int current=-1;for(const auto& entry:hidden){auto* pane=entry.pane;auto* bar=pane->GetAutoHideToolBar();if(!bar)continue;
    if(current<0 || bar->m_bFirstInGroup){Node node;forest.nodes.push_back(node);Root root;root.node=(int)forest.nodes.size()-1;root.alignment=pane->GetCurrentAlignment();root.autoHide=true;root.visible=true;root.bounds=pane->GetAHRestoredRect();
      if(root.bounds.IsRectEmpty())pane->GetWindowRect(root.bounds);root.bounds.right=root.bounds.left+max(220,min(16384,root.bounds.Width()));root.bounds.bottom=root.bounds.top+max(140,min(16384,root.bounds.Height()));forest.roots.push_back(root);current=(int)forest.roots.size()-1;}
    Node& node=forest.nodes[forest.roots[current].node];if(bar->m_bActiveInGroup)node.active=(int)node.tabs.size();node.tabs.push_back(entry.key);if(bar->m_bLastInGroup)current=-1;
  }
  return Validate(forest)?forest:Forest();
}
inline CRect VisibleBounds(CRect bounds)
{
  HMONITOR monitor=MonitorFromRect(bounds,MONITOR_DEFAULTTONEAREST);MONITORINFO info={sizeof(info)};
  if(GetMonitorInfo(monitor,&info)){const int width=max(220,min(bounds.Width(),info.rcWork.right-info.rcWork.left-32));const int height=max(140,min(bounds.Height(),info.rcWork.bottom-info.rcWork.top-48));
    bounds.left=max(info.rcWork.left,min(bounds.left,info.rcWork.right-width));bounds.top=max(info.rcWork.top,min(bounds.top,info.rcWork.bottom-height));bounds.right=bounds.left+width;bounds.bottom=bounds.top+height;}
  return bounds;
}
inline bool Detach(CDockablePane* pane)
{if(pane->IsAutoHideMode())pane->SetAutoHideMode(FALSE,CBRS_ALIGN_ANY);auto* group=pane->GetParentTabbedPane();return !group || group->DetachPane(pane,TRUE)!=FALSE;}
class NativeAccess:public CDockablePane {
public:
  static void Reparent(CDockablePane& pane,CWnd* target){(pane.*&NativeAccess::RemoveFromMiniframe)(target,DM_UNKNOWN);}
};
inline bool SplitPane(CDockablePane* right,CDockablePane* left,DWORD side,const CRect& desired)
{
  if(!left->GetParentMiniFrame())return right->DockToWindow(left,side,desired)!=FALSE;
  auto* mini=DYNAMIC_DOWNCAST(CMultiPaneFrameWnd,left->GetParentMiniFrame());if(!mini)return false;
  // The public mini-frame DockPane method chooses its target from the mouse.
  // Use the same native container insertion with the explicit saved target.
  right->UndockPane(FALSE);NativeAccess::Reparent(*right,mini);
  if(!mini->GetPaneContainerManager().InsertPane(right,left,side,desired))return false;
  CPaneFrameWnd::AddRemovePaneFromGlobalList(right,TRUE);mini->CheckGripperVisibility();mini->OnPaneRecalcLayout();return true;
}
inline bool Restore(CMDIFrameWndEx& frame,const Forest& forest,const std::vector<Entry>& entries)
{
  if(!Validate(forest))return false;WorkspaceModernResources resources;
  std::map<Key,CDockablePane*> panes;std::set<CDockablePane*> unique;
  for(const auto& entry:entries){if(!entry.pane || !entry.pane->GetSafeHwnd() || !unique.insert(entry.pane).second || !panes.insert(std::make_pair(entry.key,entry.pane)).second)return false;}
  for(const auto& root:forest.roots)for(const auto& key:Keys(forest,root.node))if(!panes.count(key))return false;
  for(const auto& root:forest.roots)for(const auto& key:Keys(forest,root.node))if(!Detach(panes[key]))return false;
  std::vector<CDockablePane*> owners(forest.nodes.size(),NULL);
  for(size_t i=0;i<forest.nodes.size();++i){const Node& node=forest.nodes[i];if(node.tabs.empty())continue;
    CDockablePane* first=panes[node.tabs[0]];frame.DockPane(first,AFX_IDW_DOCKBAR_RIGHT);if(!first->GetDefaultPaneDivider())return false;first->ShowPane(TRUE,FALSE,FALSE);
    for(size_t j=1;j<node.tabs.size();++j){auto* pane=panes[node.tabs[j]];if(!pane->AttachToTabWnd(first,DM_SHOW,FALSE))return false;}
    owners[i]=Owner(first);if(auto* group=first->GetParentTabbedPane())group->GetUnderlyingWindow()->SetActiveTab(node.active);
  }
  std::function<CDockablePane*(int)> representative=[&](int index){return forest.nodes[index].tabs.empty()?representative(forest.nodes[index].left):owners[index];};
  std::function<bool(int)> split=[&](int index){const Node& node=forest.nodes[index];if(!node.tabs.empty())return true;
    auto* left=representative(node.left);auto* right=representative(node.right);CRect bounds;left->GetWindowRect(bounds);CRect desired=bounds;
    if(node.horizontal)desired.top=bounds.top+bounds.Height()*node.ratio/1000;else desired.left=bounds.left+bounds.Width()*node.ratio/1000;
    return SplitPane(right,left,node.horizontal?CBRS_ALIGN_BOTTOM:CBRS_ALIGN_RIGHT,desired) && split(node.left) && split(node.right);};
  std::function<bool(int)> proportions=[&](int index){const Node& node=forest.nodes[index];if(!node.tabs.empty())return true;
    auto* first=LeafContainer(representative(node.left));auto* second=LeafContainer(representative(node.right));CPaneContainer* common=NULL;
    for(auto* a=first;a && !common;a=a->GetParentPaneContainer())for(auto* b=second;b;b=b->GetParentPaneContainer())if(a==b){common=a;break;}
    if(!common || !common->GetPaneDivider())return false;
    CRect all,part;common->GetWindowRect(all,TRUE);
    if(common->GetLeftPane())common->GetLeftPane()->GetWindowRect(part);else common->GetLeftPaneContainer()->GetWindowRect(part,TRUE);
    CSize minimumLeft,minimumRight;common->GetMinSizeLeft(minimumLeft);common->GetMinSizeRight(minimumRight);
    auto* divider=const_cast<CPaneDivider*>(common->GetPaneDivider());const int total=node.horizontal?all.Height():all.Width();
    const int lower=node.horizontal?minimumLeft.cy:minimumLeft.cx,upper=total-divider->GetWidth()-(node.horizontal?minimumRight.cy:minimumRight.cx);
    const int wanted=max(lower,min(upper,total*node.ratio/1000));const int offset=wanted-(node.horizontal?part.Height():part.Width());
    CPoint move(node.horizontal?0:offset,node.horizontal?offset:0);divider->Move(move,FALSE);
    return proportions(node.left) && proportions(node.right);};
  // Split before descending: inserting beside a completed child subtree would
  // split its first leaf, losing arbitrary nested sibling relationships.
  for(const auto& root:forest.roots){auto* first=representative(root.node);if(!first)return false;
    if(root.floating){if(!first->FloatPane(VisibleBounds(root.bounds),DM_SHOW))return false;}
    else{UINT side=root.alignment==CBRS_ALIGN_LEFT?AFX_IDW_DOCKBAR_LEFT:root.alignment==CBRS_ALIGN_TOP?AFX_IDW_DOCKBAR_TOP:root.alignment==CBRS_ALIGN_BOTTOM?AFX_IDW_DOCKBAR_BOTTOM:AFX_IDW_DOCKBAR_RIGHT;CRect bounds=root.bounds;frame.ScreenToClient(bounds);frame.DockPane(first,side,bounds);if(!first->GetDefaultPaneDivider() || first->GetParentMiniFrame())return false;}
    first->ShowPane(TRUE,FALSE,FALSE);frame.AdjustDockingLayout();if(!split(root.node))return false;
    if(root.autoHide){auto* owner=Owner(panes[forest.nodes[root.node].tabs[0]]);owner->SetAutoHideMode(TRUE,root.alignment,NULL,FALSE);
      for(const auto& key:Keys(forest,root.node))if(!panes[key]->IsAutoHideMode())return false;}
    else if(!root.visible)for(const auto& key:Keys(forest,root.node))panes[key]->ShowPane(FALSE,FALSE,FALSE);
  }
  // Other roots are still docked temporarily during construction. Apply saved
  // ratios only after their final floating/auto-hide placement frees that space.
  frame.AdjustDockingLayout();
  for(const auto& root:forest.roots)if(!root.floating && !root.autoHide){auto* pane=representative(root.node);auto* divider=pane->GetDefaultPaneDivider();auto* container=Container(pane);if(!divider || !container)return false;
    CRect bounds,client;container->GetWindowRect(bounds,TRUE);frame.GetClientRect(client);CSize minimum;container->GetMinSize(minimum);
    const bool vertical=root.alignment==CBRS_ALIGN_LEFT || root.alignment==CBRS_ALIGN_RIGHT;
    const int requested=vertical?root.bounds.Width():root.bounds.Height(),current=vertical?bounds.Width():bounds.Height();
    const int wanted=max(vertical?minimum.cx:minimum.cy,min(requested,max(1,(vertical?client.Width():client.Height())-80)));
    const int offset=(root.alignment==CBRS_ALIGN_RIGHT || root.alignment==CBRS_ALIGN_BOTTOM)?current-wanted:wanted-current;
    CPoint move(vertical?offset:0,vertical?0:offset);divider->Move(move,FALSE);}
  for(const auto& root:forest.roots)if(!proportions(root.node))return false;
  frame.AdjustDockingLayout();return true;
}
// Geometry and the selected tab may legitimately change while another world
// loads. Detect manual structural changes, without replaying unrelated roots.
inline std::string Structure(Forest forest)
{for(auto& node:forest.nodes){node.ratio=500;node.active=0;}for(auto& root:forest.roots){root.bounds=CRect(0,0,500,350);root.visible=true;}return Encode(forest);}
class Session {
  Forest saved;
  bool writable=true;
  struct Applied {std::string structure;bool abandoned=false;};
  std::map<size_t,Applied> applied;
public:
  enum Arrival {NotSaved,Restored,Standalone,Failed};
  bool loaded=false;
  bool Load(const std::string& data){loaded=true;applied.clear();saved=Forest();writable=Decode(data,saved);return writable;}
  const Forest& Saved()const{return saved;}
  bool Contains(const Key& key)const{for(const auto& root:saved.roots)if(Keys(saved,root.node).count(key))return true;return false;}
  void Observe(const std::vector<Entry>& entries)
  {
    std::set<Key> present;for(const auto& entry:entries)present.insert(entry.key);
    for(size_t i=0;i<saved.roots.size();++i){std::set<Key> available;for(const auto& key:Keys(saved,saved.roots[i].node))if(present.count(key))available.insert(key);
      if(!available.empty() && !applied[i].abandoned)applied[i].structure=Structure(CaptureSubset(entries,available));}
  }
  Arrival ArriveResult(CMDIFrameWndEx& frame,const Key& key,const std::vector<Entry>& entries)
  {
    std::set<Key> present;std::set<CDockablePane*> physical;if(entries.size()>MaxPanes)return Failed;
    for(const auto& entry:entries)if(!entry.pane || !entry.pane->GetSafeHwnd() || !ValidKey(entry.key) || !physical.insert(entry.pane).second || !present.insert(entry.key).second)return Failed;
    for(size_t index=0;index<saved.roots.size();++index){const auto keys=Keys(saved,saved.roots[index].node);if(!keys.count(key))continue;
      Applied& previous=applied[index];if(previous.abandoned)return Standalone;
      std::set<Key> existing=keys;existing.erase(key);std::set<Key> available;for(const auto& candidate:existing)if(present.count(candidate))available.insert(candidate);
      if(!previous.structure.empty() && Structure(CaptureSubset(entries,available))!=previous.structure){previous.abandoned=true;return Standalone;}
      if(!previous.structure.empty())for(const auto& entry:entries)if(available.count(entry.key)){
        auto* owner=Owner(entry.pane);auto* container=Container(owner);
        for(const auto& other:entries)if(!keys.count(other.key) &&
          (Owner(other.pane)==owner || (container && !entry.pane->IsAutoHideMode() && Container(Owner(other.pane))==container))){previous.abandoned=true;return Standalone;}
      }
      Forest component;Root root=saved.roots[index];root.node=ProjectNode(saved,root.node,present,component);if(root.node<0)return Failed;component.roots.push_back(root);
      if(!Restore(frame,component,entries))return Failed;
      std::set<Key> current;for(const auto& candidate:keys)if(present.count(candidate))current.insert(candidate);
      previous.structure=Structure(CaptureSubset(entries,current));return Restored;
    }return NotSaved;
  }
  bool Arrive(CMDIFrameWndEx& frame,const Key& key,const std::vector<Entry>& entries){return ArriveResult(frame,key,entries)==Restored;}
  static Forest CaptureSubset(const std::vector<Entry>& entries,const std::set<Key>& keys)
  {std::vector<Entry> subset;for(const auto& entry:entries)if(keys.count(entry.key))subset.push_back(entry);return Capture(subset);}
  std::string Save(const std::vector<Entry>& entries)
  {
    if(!writable)return ""; // Preserve unrecognized/malformed rows for recovery.
    Forest current=Capture(entries),retained;std::set<Key> present,reserved;
    if(!entries.empty() && current.nodes.empty())return "";
    for(const auto& entry:entries)present.insert(entry.key);
    for(size_t i=0;i<saved.roots.size();++i){const auto keys=Keys(saved,saved.roots[i].node);std::set<Key> missing;for(const auto& key:keys)if(!present.count(key))missing.insert(key);if(missing.empty())continue;
      std::set<Key> existing;for(const auto& key:keys)if(present.count(key))existing.insert(key);
      const bool moved=applied.count(i) && (applied[i].abandoned || (!applied[i].structure.empty() && Structure(CaptureSubset(entries,existing))!=applied[i].structure));const auto& keep=moved?missing:keys;
      Root root=saved.roots[i];root.node=ProjectNode(saved,root.node,keep,retained);if(root.node>=0){retained.roots.push_back(root);reserved.insert(keep.begin(),keep.end());}}
    std::set<Key> keep;for(const auto& key:present)if(!reserved.count(key))keep.insert(key);Append(retained,Project(current,keep));
    const std::string data=Encode(retained);if(!data.empty()){saved=retained;applied.clear();Observe(entries);}return data;
  }
};
} // namespace workspace_dock
