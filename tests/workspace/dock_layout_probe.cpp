#include "stdafx.h"
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include "workspace/WorkspaceDockLayout.h"
#include <fstream>

namespace wd=workspace_dock;
class DockPane:public CWorkspacePane {
public:
  CBasePane* dropTarget=NULL;
  AFX_CS_STATUS IsChangeState(int sensitivity,CBasePane** target) const override {
    if(dropTarget){*target=dropTarget;return CS_DELAY_DOCK;}
    return CWorkspacePane::IsChangeState(sensitivity,target);
  }
  CRichEditCtrl& Text(){return m_transcripts[0]->text;}
  long End(size_t index){return m_transcripts[0]->renderedEnds[index];}
};
bool Check(std::ofstream& log,const char* label,bool ok){log<<(ok?"PASS ":"FAIL ")<<label<<"\n";return ok;}
int Leaf(wd::Forest& f,const std::vector<wd::Key>& tabs,int active=0){wd::Node n;n.tabs=tabs;n.active=active;f.nodes.push_back(n);return (int)f.nodes.size()-1;}
int Split(wd::Forest& f,int a,int b,bool horizontal,int ratio=500){wd::Node n;n.left=a;n.right=b;n.horizontal=horizontal;n.ratio=ratio;f.nodes.push_back(n);return (int)f.nodes.size()-1;}
std::string ShapeNode(const wd::Forest& f,int i){const auto& n=f.nodes[i];if(n.tabs.empty())return std::string(n.horizontal?"H(":"V(")+ShapeNode(f,n.left)+","+ShapeNode(f,n.right)+")";
 std::string s="[";for(const auto& k:n.tabs)s+=k.world+"/"+k.name+";";return s+"]@"+std::to_string(n.active);}
std::string Shape(const wd::Forest& f){std::vector<std::string> rows;for(const auto& r:f.roots)rows.push_back(std::to_string(r.floating?0:r.alignment)+(r.autoHide?"AH":r.floating?"FLOAT":"DOCK")+ShapeNode(f,r.node));std::sort(rows.begin(),rows.end());std::string s;for(const auto& r:rows)s+=r+"\n";return s;}
bool Ratios(std::ofstream& log,const wd::Forest& expected,const wd::Forest& actual){bool ok=true;for(size_t i=0;i<expected.nodes.size();++i){const auto& n=expected.nodes[i];if(!n.tabs.empty())continue;
 const auto shape=ShapeNode(expected,(int)i);bool found=false;for(size_t j=0;j<actual.nodes.size();++j)if(ShapeNode(actual,(int)j)==shape){found=true;log<<"ratio requested "<<n.ratio<<" actual "<<actual.nodes[j].ratio<<"\n";ok=abs(n.ratio-actual.nodes[j].ratio)<=15&&ok;break;}ok=found&&ok;}return ok;}
std::wstring Content(HWND window){GETTEXTLENGTHEX length={GTL_PRECISE|GTL_NUMCHARS,1200};long n=(long)SendMessageW(window,EM_GETTEXTLENGTHEX,(WPARAM)&length,0);std::vector<wchar_t> text(n+2,0);GETTEXTEX get={DWORD(text.size()*2),GT_DEFAULT,1200,NULL,NULL};SendMessageW(window,EM_GETTEXTEX,(WPARAM)&get,(LPARAM)&text[0]);return &text[0];}
struct Surface {HWND window;std::wstring text;long first,last,anchor,y;};
Surface Save(DockPane& pane){Surface s;s.window=pane.Text().m_hWnd;s.text=Content(s.window);pane.Text().GetSel(s.first,s.last);s.anchor=(long)pane.Text().SendMessage(EM_LINEINDEX,pane.Text().GetFirstVisibleLine());POINTL p={0,0};SendMessageW(s.window,EM_POSFROMCHAR,(WPARAM)&p,s.anchor);s.y=p.y;return s;}
bool Same(DockPane& pane,const Surface& saved){long a,b;pane.Text().GetSel(a,b);return pane.Text().m_hWnd==saved.window && Content(saved.window)==saved.text && a==saved.first && b==saved.last;}
bool SameReading(DockPane& pane,const Surface& saved){POINTL p={0,0};SendMessageW(pane.Text().m_hWnd,EM_POSFROMCHAR,(WPARAM)&p,saved.anchor);return abs(p.y-saved.y)<=1;}

class DockApp:public CWinApp {
 int result_=1;
 BOOL InitInstance(){
  AfxOleInit();AfxInitRichEdit2();std::ofstream log("dock-layout-results.txt");log.setf(std::ios::unitbuf);bool ok=true;
  WorkspaceTestFrame* frame=new WorkspaceTestFrame;std::vector<DockPane*> panes;std::vector<wd::Entry> entries;
  ok=frame->Create(NULL,"Fictional split-tree layout fixture",WS_OVERLAPPEDWINDOW,CRect(40,40,1840,1240))!=FALSE;
  m_pMainWnd=frame;frame->EnableWorkspaceDocking();frame->ShowWindow(SW_SHOWNOACTIVATE);
  for(int i=0;i<9;++i){DockPane* pane=new DockPane;panes.push_back(pane);CString world;world.Format("fictional-%d",i);CString name;name.Format("Transcript-%d",i);
   ok=pane->Create(frame,0xe980+i)&&pane->OpenTranscript(world,name)&&ok;pane->EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(pane);pane->ShowPane(TRUE,FALSE,FALSE);
   wd::Entry entry;entry.key={i==8?"@tools":(LPCSTR)world,(LPCSTR)name};entry.pane=pane;entries.push_back(entry);
   WorkspaceRecord r;r.connectionId=world;WorkspaceRun run;run.text=L"Fictional preserved text \u03bb\U0001f409 e\u0301";run.bold=true;run.actionType=0x0400;run.action="fictional-only";r.runs.push_back(run);
   for(int j=1;j<=400;++j){r.sequence=j;ok=pane->AppendRecord(name,r)&&ok;}pane->Text().SetSel(pane->End(149),pane->End(149)+12);pane->Text().SendMessage(EM_SCROLLCARET);
  }
  frame->AdjustDockingLayout();
  panes[1]->AttachToTabWnd(panes[0],DM_SHOW,FALSE);panes[2]->AttachToTabWnd(panes[0],DM_SHOW,FALSE);
  const Surface original=Save(*panes[0]);
  auto* old=panes[0]->GetParentTabbedPane();ok=Check(log,"initial native tab group has three unique transcript panes",old && old->GetTabsNum()==3)&&ok;
  ok=Check(log,"native tab host retains resource-aware class after pane creation",old && !strcmp(old->GetRuntimeClass()->m_lpszClassName,"WorkspaceNativeTabs"))&&ok;
  ok=frame->FloatWorkspacePane(*panes[0],CRect(120,160,820,660))&&ok;
  old=panes[1]->GetParentTabbedPane();ok=Check(log,"native detach removes the original tab before floating: three -> two plus float",old && old->GetTabsNum()==2 && !panes[0]->IsTabbed() && panes[0]->IsFloating() && Same(*panes[0],original))&&ok;
  ok=Check(log,"floating host retains resource-aware class after pane creation",panes[0]->GetParentMiniFrame() && !strcmp(panes[0]->GetParentMiniFrame()->GetRuntimeClass()->m_lpszClassName,"WorkspaceNativeFloat"))&&ok;
  ok=panes[0]->DockToRecentPos()!=FALSE&&ok;old=panes[0]->GetParentTabbedPane();
  ok=Check(log,"native recent redock returns exactly three tabs and preserves original transcript HWND/text/selection",old && old->GetTabsNum()==3 && Same(*panes[0],original))&&ok;
  frame->FloatWorkspacePane(*panes[5],CRect(120,160,820,660));
  frame->FloatWorkspacePane(*panes[6],CRect(850,160,1450,660));
  panes[6]->dropTarget=panes[5];BOOL dropped=FALSE;
  CPane* accepted=panes[6]->DockPaneStandard(dropped);panes[6]->dropTarget=NULL;
  ok=Check(log,"native mouse-docking path accepts a floating split target instead of rejecting it",accepted==panes[6])&&ok;

  wd::Forest model;const int ab=Leaf(model,{entries[0].key,entries[1].key},1),c=Leaf(model,{entries[2].key}),d=Leaf(model,{entries[3].key}),e=Leaf(model,{entries[4].key});
  const int top=Split(model,ab,c,false,600),bottom=Split(model,d,e,false,400),main=Split(model,top,bottom,true,550);
  wd::Root root;root.node=main;root.bounds=CRect(600,160,1740,1140);model.roots.push_back(root);
  const int f=Leaf(model,{entries[5].key}),g=Leaf(model,{entries[6].key});root.node=Split(model,f,g,true,450);root.floating=true;root.bounds=CRect(120,160,820,960);model.roots.push_back(root);
  root.node=Leaf(model,{entries[7].key,entries[8].key},1);root.floating=false;root.autoHide=true;root.alignment=CBRS_ALIGN_LEFT;root.bounds=CRect(80,120,560,620);model.roots.push_back(root);
  ok=Check(log,"validated portable model contains nested sibling subtrees, active tabs, floating split and auto-hide group",wd::Validate(model))&&ok;
  auto aliases=entries;aliases[1].pane=aliases[0].pane;ok=Check(log,"two identities cannot attach a physical native pane to itself",!wd::Restore(*frame,model,aliases))&&ok;
  std::vector<Surface> surfaces;for(auto* p:panes)surfaces.push_back(Save(*p));
  ok=Check(log,"restore complete native split forest",wd::Restore(*frame,model,entries))&&ok;
  wd::Forest actual=wd::Capture(entries);log<<"EXPECTED\n"<<Shape(model)<<"ACTUAL\n"<<Shape(actual);
  ok=Check(log,"native capture retains both nested sibling subtrees, ordered/active tabs, float and auto-hide semantics",Shape(model)==Shape(actual))&&ok;
  ok=Check(log,"native divider ratios survive restoration within pixel rounding",Ratios(log,model,actual))&&ok;
  bool intact=true;for(size_t i=0;i<panes.size();++i)intact=Same(*panes[i],surfaces[i])&&intact;
  ok=Check(log,"restoration reparents original transcript controls without text replay or selection reset",intact)&&ok;
  bool reading=true;for(size_t i=0;i<panes.size();++i)reading=SameReading(*panes[i],surfaces[i])&&reading;
  ok=Check(log,"reading anchors survive native split/float/auto-hide resizing",reading)&&ok;
  const std::string encoded=wd::Encode(actual);wd::Forest decoded;ok=Check(log,"portable data round trip preserves exact forest",!encoded.empty() && wd::Decode(encoded,decoded) && wd::Encode(decoded)==encoded)&&ok;
  {
   wd::Forest bad=model;bad.nodes[main].left=main;ok=Check(log,"cyclic or aliased split nodes rejected",!wd::Validate(bad))&&ok;
   bad=model;bad.nodes[c].tabs[0]=entries[0].key;ok=Check(log,"duplicate persistent identities rejected",!wd::Validate(bad))&&ok;
   bad=model;bad.roots[0].bounds.right=LONG_MIN;ok=Check(log,"extreme coordinates rejected before rectangle arithmetic",!wd::Validate(bad))&&ok;
   bad=model;bad.nodes[main].ratio=1000;ok=Check(log,"out-of-range divider ratio rejected",!wd::Validate(bad))&&ok;
   bad=model;bad.nodes[ab].active=2;ok=Check(log,"invalid selected tab index rejected",!wd::Validate(bad))&&ok;
   bad=model;bad.nodes[ab].tabs[0].name=std::string(513,'x');ok=Check(log,"overlong tab identity rejected",!wd::Validate(bad))&&ok;
   wd::Forest deep;int chain=Leaf(deep,{{"deep-0","test"}});for(int i=1;i<131;++i)chain=Split(deep,chain,Leaf(deep,{{"deep-"+std::to_string(i),"test"}}),true);wd::Root depthRoot;depthRoot.node=chain;deep.roots.push_back(depthRoot);
   ok=Check(log,"excessive nesting depth rejected without native mutation",!wd::Validate(deep))&&ok;
   bad=model;bad.nodes[ab].tabs[0].world+=" :runtime:secret";ok=Check(log,"runtime session identities rejected",!wd::Validate(bad))&&ok;
   ok=Check(log,"unknown versions, malformed bytes and excessive node counts rejected",!wd::Decode("WDL99\n0 0\n",bad) && !wd::Decode(encoded+"junk",bad) && !wd::Decode("WDL3\n999999 0\n",bad))&&ok;
   wd::Session invalid;invalid.Load("unknown future version");ok=Check(log,"malformed prior snapshot is preserved rather than overwritten",invalid.Save(entries).empty())&&ok;
   CRect bounds=wd::VisibleBounds(CRect(-999999,-999999,-998999,-999399));MONITORINFO monitor={sizeof(monitor)};GetMonitorInfo(MonitorFromRect(bounds,MONITOR_DEFAULTTONEAREST),&monitor);
   ok=Check(log,"offscreen floating rectangles clamp into current monitor work area",bounds.left>=monitor.rcWork.left && bounds.top>=monitor.rcWork.top && bounds.right<=monitor.rcWork.right && bounds.bottom<=monitor.rcWork.bottom)&&ok;
  }
  // Reuse controls, but erase all native relationships first. Arrival order is
  // deliberately different from world creation order, including a late tool.
  for(auto* pane:panes){wd::Detach(pane);frame->DockPane(pane);pane->ShowPane(TRUE,FALSE,FALSE);}
  wd::Session session;session.Load(encoded);std::vector<wd::Entry> arrived;
  for(int i:{4,2,0,7,6,1,5,3,8}){arrived.push_back(entries[i]);ok=session.Arrive(*frame,entries[i].key,arrived)&&ok;}
  actual=wd::Capture(entries);log<<"AFTER LATE ARRIVAL\n"<<Shape(actual);
  ok=Check(log,"out-of-order worlds and a late tool restore the same nested forest deterministically",Shape(actual)==Shape(decoded))&&ok;
  // A closed world is absent, rather than a hidden pane. Preserve its stable
  // place while saving and reinsert it after a later reopen.
  wd::Detach(panes[2]);frame->DockPane(panes[2]);panes[2]->ShowPane(FALSE,FALSE,FALSE);
  std::vector<wd::Entry> open;for(size_t i=0;i<entries.size();++i)if(i!=2)open.push_back(entries[i]);session.Observe(open);
  const std::string closed=session.Save(open);wd::Forest closedForest;
  ok=Check(log,"closing a world retains every persistent identity in the saved forest",wd::Decode(closed,closedForest) && Shape(closedForest)==Shape(decoded))&&ok;
  open.push_back(entries[2]);ok=Check(log,"reopening a world restores its original nested position",session.Arrive(*frame,entries[2].key,open) && Shape(wd::Capture(entries))==Shape(decoded))&&ok;
  for(auto* pane:panes){wd::Detach(pane);frame->DockPane(pane);pane->ShowPane(TRUE,FALSE,FALSE);}
  wd::Session moved;moved.Load(encoded);arrived={entries[0]};ok=moved.Arrive(*frame,entries[0].key,arrived)&&ok;
  frame->FloatWorkspacePane(*panes[0],CRect(300,200,1000,800));CRect manual;panes[0]->GetParentMiniFrame()->GetWindowRect(manual);
  arrived.push_back(entries[2]);const bool joined=moved.Arrive(*frame,entries[2].key,arrived);CRect untouched;panes[0]->GetParentMiniFrame()->GetWindowRect(untouched);
  ok=Check(log,"a later world cannot regroup a reader manually moved after partial restoration",!joined && panes[0]->IsFloating() && manual==untouched && !panes[2]->GetParentTabbedPane())&&ok;
  const std::string partial=moved.Save(arrived);wd::Forest partialForest;std::set<wd::Key> retained;bool standalone=false;
  const bool partialValid=wd::Decode(partial,partialForest);for(const auto& r:partialForest.roots){const auto keys=wd::Keys(partialForest,r.node);retained.insert(keys.begin(),keys.end());if(keys.count(entries[0].key))standalone=r.floating && keys.size()==1;}
  ok=Check(log,"saving a partial layout preserves all nine identities and the manually moved standalone pane",partialValid && retained.size()==9 && standalone)&&ok;
  auto duplicate=arrived;duplicate.push_back(arrived[0]);wd::Session failure;failure.Load(encoded);
  ok=Check(log,"native failure remains distinct from a deliberate manual standalone opt-out",failure.ArriveResult(*frame,entries[0].key,duplicate)==wd::Session::Failed && moved.ArriveResult(*frame,entries[2].key,arrived)==wd::Session::Standalone)&&ok;
  for(auto* pane:panes){wd::Detach(pane);frame->DockPane(pane);pane->ShowPane(TRUE,FALSE,FALSE);}
  wd::Session unrelated;unrelated.Load(encoded);arrived={entries[0],entries[5]};ok=unrelated.Arrive(*frame,entries[0].key,arrived)&&ok;
  panes[5]->DockToWindow(panes[0],CBRS_ALIGN_BOTTOM);const auto* manualContainer=wd::Container(panes[0]);arrived.push_back(entries[2]);
  ok=Check(log,"late arrivals preserve a user's split with an unrelated component",unrelated.ArriveResult(*frame,entries[2].key,arrived)==wd::Session::Standalone && wd::Container(panes[0])==manualContainer && wd::Container(panes[5])==manualContainer)&&ok;
  auto hiddenModel=model;hiddenModel.roots[1].visible=false;ok=wd::Restore(*frame,hiddenModel,entries)&&ok;
  const auto hiddenCapture=wd::Capture(entries);bool explicitHidden=false,explicitAutoHide=false;
  for(const auto& r:hiddenCapture.roots){const auto keys=wd::Keys(hiddenCapture,r.node);if(keys.count(entries[5].key))explicitHidden=!r.visible && !r.autoHide;if(keys.count(entries[7].key))explicitAutoHide=r.autoHide && panes[7]->IsAutoHideMode() && panes[8]->IsAutoHideMode();}
  ok=Check(log,"hidden floating roots and native auto-hide buttons retain distinct semantics",explicitHidden && explicitAutoHide)&&ok;
  log<<(ok?"PASS":"FAIL")<<" fictional native dock layout regression; native API simulation, not physical mixed-DPI mouse acceptance\n";
  frame->DestroyWindow();for(auto* pane:panes)delete pane;result_=ok?0:1;return FALSE;
 }
 int ExitInstance(){return result_;}
};DockApp app;
