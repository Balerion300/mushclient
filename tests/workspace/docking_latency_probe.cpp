#include "stdafx.h"
#include <atomic>
#include <memory>
#include <string>
#define private public // Decoded synthetic image fixture; no network or personal data.
#include "InlineImage.h"
#undef private
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include <richole.h>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace {
double Now() { LARGE_INTEGER time, frequency; QueryPerformanceCounter(&time); QueryPerformanceFrequency(&frequency); return double(time.QuadPart)/frequency.QuadPart; }
struct Snapshot {
 HWND window; std::wstring text; std::deque<long> ends; std::vector<std::string> links;
 size_t records; long start,end,anchor; LONG anchorY; bool follow; int objects;
};
class LatencyPane: public CWorkspacePane {
public:
 Transcript& Current() { return *Find("fictional-latency","Output"); }
 void PrepareReader() {
  Transcript& t=Current();
  const long first=t.renderedEnds[1199]; t.text.SetSel(first,first+15);
  t.text.LineScroll(100000);t.text.LineScroll(-180);
 }
 Snapshot Capture() {
  Transcript& t=Current(); Snapshot s; s.window=t.text.m_hWnd;s.records=t.records.size();s.ends=t.renderedEnds;
  std::vector<wchar_t> buffer(TextLength(t.text)+2);GETTEXTEX get={DWORD(buffer.size()*sizeof(wchar_t)),GT_DEFAULT,1200,NULL,NULL};
  SendMessageW(s.window,EM_GETTEXTEX,(WPARAM)&get,(LPARAM)&buffer[0]);s.text=&buffer[0];
  const ViewState view=SaveView(t.text);s.start=view.start;s.end=view.end;s.anchor=view.anchor;s.anchorY=view.anchorY;s.follow=view.follow;
  for(const auto& link:t.links){std::ostringstream key;key<<link.first<<":"<<link.last<<":"<<link.type<<":"<<(LPCSTR)link.action;s.links.push_back(key.str());}
  std::sort(s.links.begin(),s.links.end());
  IRichEditOle* ole=NULL;SendMessageW(s.window,EM_GETOLEINTERFACE,0,(LPARAM)&ole);s.objects=ole?ole->GetObjectCount():-1;if(ole)ole->Release();return s;
 }
 bool Validate(const Snapshot& original,const Snapshot& before,std::ofstream& log,const char* operation) {
  const Snapshot after=Capture();Transcript& t=Current();
  POINTL position={0,0};SendMessageW(after.window,EM_POSFROMCHAR,(WPARAM)&position,before.anchor);
  const bool anchor=before.follow || std::abs(position.y-before.anchorY)<=1;
  bool ok=original.window==after.window && original.text==after.text && original.records==after.records && original.ends==after.ends && original.links==after.links && original.objects==after.objects && before.start==after.start && before.end==after.end && anchor;
  IRichEditOle* ole=NULL;SendMessageW(after.window,EM_GETOLEINTERFACE,0,(LPARAM)&ole);
  CRect viewport;t.text.GetClientRect(viewport);long maxWidth=0;
  const long expectedWidth=max(1,min(512,viewport.Width()-12)),expectedHeight=max(1,int(expectedWidth*200.0/640));
  if(ole){for(int i=0;i<ole->GetObjectCount();++i){REOBJECT object={0};object.cbStruct=sizeof(object);const bool valid=SUCCEEDED(ole->GetObject(i,&object,REO_GETOBJ_NO_INTERFACES));const long width=MulDiv(object.sizel.cx,96,2540),height=MulDiv(object.sizel.cy,96,2540);ok=valid&&std::abs(width-expectedWidth)<=1&&std::abs(height-expectedHeight)<=1&&ok;maxWidth=max(maxWidth,width);}ole->Release();}else ok=false;
  // Read a bold style and dispatch an exact retained text link after every transition.
  const long first=t.renderedEnds[1199];t.text.SetSel(first,first+8);CHARFORMAT2W format;memset(&format,0,sizeof(format));format.cbSize=sizeof(format);SendMessageW(after.window,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);
  ok=ok&&(format.dwEffects&CFE_BOLD)&&format.crTextColor==RGB(190,80,50);t.text.SetSel(after.start,after.end);
  CString action;SetActionHandler([&](const CString& id,unsigned short,const CString& value){if(id=="fictional-latency")action=value;});
  auto link=std::find_if(t.links.begin(),t.links.end(),[](const LinkRange& l){return l.action=="look fictional-latency-1201";});
  if(link==t.links.end())ok=false;else {ENLINK event={0};event.msg=WM_LBUTTONUP;event.chrg.cpMin=link->first;event.chrg.cpMax=link->last;LRESULT result=0;OnLink(100,(NMHDR*)&event,&result);ok=ok&&result==1&&action=="look fictional-latency-1201";}
  SetActionHandler(ActionHandler());
  log<<operation<<" retained="<<after.records<<" objects="<<after.objects<<" viewport="<<viewport.Width()<<" image-width="<<maxWidth<<" selection="<<before.start<<","<<before.end<<" -> "<<after.start<<","<<after.end<<" anchor-y="<<before.anchorY<<" -> "<<position.y<<" "<<(ok?"PASS":"FAIL")<<"\n";return ok;
 }
};
std::shared_ptr<CInlineImage> SyntheticImage() {
 auto image=CInlineImage::Request("synthetic:docking-latency");for(int i=0;i<100 && image->GetState()==CInlineImage::Loading;++i)Sleep(5);
 BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=640;info.bmiHeader.biHeight=200;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels=NULL;
 image->bitmap_=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);if(!pixels)return std::shared_ptr<CInlineImage>();
 for(int y=0;y<200;++y)for(int x=0;x<640;++x)((DWORD*)pixels)[y*640+x]=0xff000000|((x/20)%2?0xb87333:0x357899);
 image->width_=640;image->height_=200;image->bitmapBytes_=640*200*4;image->state_.store(CInlineImage::Ready);return image;
}
class LatencyApp:public CWinApp {
 BOOL InitInstance() {
  AfxOleInit();AfxInitRichEdit2();const char* prefix=getenv("MUSH_DOCK_LATENCY_OUTPUT");const std::string path=prefix?prefix:"WinRel/docking-latency";
  std::ofstream log(path+".txt");log.setf(std::ios::unitbuf);std::ofstream csv(path+".csv");csv.setf(std::ios::unitbuf);csv<<"operation,cycle,milliseconds\n";
  WorkspaceTestFrame* frame=new WorkspaceTestFrame;bool ok=frame->Create(NULL,"Owned fictional docking latency fixture",WS_OVERLAPPEDWINDOW,CRect(40,40,1300,950))!=FALSE;
  m_pMainWnd=frame;frame->EnableWorkspaceDocking();LatencyPane pane;CWorkspacePane companion;
  ok=pane.Create(frame,0xe960)&&companion.Create(frame,0xe961)&&ok;pane.EnableDocking(CBRS_ALIGN_ANY);companion.EnableDocking(CBRS_ALIGN_ANY);
  frame->DockPane(&pane);ok=pane.OpenTranscript("fictional-latency","Output")&&companion.OpenTranscript("fictional-companion","Second")&&ok;
  frame->ShowWindow(SW_SHOWNOACTIVATE);frame->AdjustDockingLayout();frame->UpdateWindow();pane.FloatPane(CRect(80,80,700,700),DM_SHOW);
  auto image=SyntheticImage();ok=image!=NULL&&ok;if(!ok){log<<"FAIL fixture setup\n";ExitProcess(1);}
  const double fill=Now();pane.BeginRenderBatch();
  for(unsigned row=1;row<=1800;++row){WorkspaceRecord record;record.connectionId="fictional-latency";record.sequence=row;
   for(int run=0;run<4;++run){WorkspaceRun style;style.text=run?L" fictional styled transcript fragment \u03bb\U0001f409 ":L"Fictional row ";style.bold=run==0;style.italic=run==1;style.underline=run==2;style.foreground=run==0?RGB(190,80,50):RGB(80+run*35,140,180);if(run==0){style.action.Format("look fictional-latency-%u",row);style.actionType=0x0400;}record.runs.push_back(style);}
   if(row%225==0){record.image=image;record.imageAction="https://fixture.invalid/fictional-image";}
   ok=pane.AppendRecord("Output",record)&&ok;
  }pane.EndRenderBatch();pane.PrepareReader();const Snapshot original=pane.Capture();
  log<<"fictional records="<<original.records<<" decoded images="<<original.objects<<" fill-ms="<<(Now()-fill)*1000<<" text-chars="<<original.text.size()<<"\n";ok=ok&&original.records==1800&&original.objects==8&&!original.follow;
  std::vector<double> timings;
  auto measure=[&](const char* name,int cycle,const std::function<void()>& operation){const Snapshot before=pane.Capture();const double started=Now();operation();frame->AdjustDockingLayout();frame->UpdateWindow();const double elapsed=(Now()-started)*1000;timings.push_back(elapsed);csv<<name<<","<<cycle<<","<<elapsed<<"\n";log<<name<<" cycle="<<cycle<<" ms="<<elapsed<<"\n";ok=pane.Validate(original,before,log,name)&&ok;};
  for(int cycle=0;cycle<3;++cycle){
   measure("resize-narrow",cycle,[&](){pane.SetWindowPos(NULL,0,0,280,560,SWP_NOMOVE|SWP_NOZORDER);});
   measure("resize-wide",cycle,[&](){pane.SetWindowPos(NULL,0,0,760,560,SWP_NOMOVE|SWP_NOZORDER);});
   measure("dock",cycle,[&](){frame->DockPane(&pane,AFX_IDW_DOCKBAR_RIGHT);});
   measure("float",cycle,[&](){ok=pane.FloatPane(CRect(100,100,440,710),DM_SHOW)!=FALSE&&ok;ok=pane.IsFloating()!=FALSE&&ok;});
   measure("dock-for-group",cycle,[&](){frame->DockPane(&pane,AFX_IDW_DOCKBAR_RIGHT);ok=pane.IsFloating()==FALSE&&ok;});
   measure("group-attach",cycle,[&](){companion.AttachToTabWnd(&pane,DM_SHOW,FALSE);});
   measure("group-float",cycle,[&](){CBaseTabbedPane* group=pane.GetParentTabbedPane();if(group)static_cast<CDockablePane*>(group)->FloatPane(CRect(80,80,800,750),DM_SHOW);else ok=false;});
   measure("group-detach",cycle,[&](){pane.FloatPane(CRect(100,100,780,680),DM_SHOW);});
  }
  std::sort(timings.begin(),timings.end());log<<"latency operations="<<timings.size()<<" p50-ms="<<timings[timings.size()/2]<<" p95-ms="<<timings[size_t((timings.size()-1)*.95)]<<" max-ms="<<timings.back()<<"\n";
  log<<(ok?"PASS":"FAIL")<<" retained long transcript native API docking/resize correctness; timing is measured, not a hardware-independent gate; mouse-drag acceptance is separate\n";
  frame->DestroyWindow();ExitProcess(ok?0:1);return FALSE;
 }
} app;
}


