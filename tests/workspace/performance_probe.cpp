#include "stdafx.h"
#include <atomic>
#include <memory>
#include <string>
#define private public
#include "InlineImage.h"
#undef private
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include <fstream>
#include <algorithm>
#include <psapi.h>
double Now();
class ProfilePane:public CWorkspacePane {
public:
 void Pause(bool value){Find("profile","Output")->paused=value;}
 size_t Count(){return Find("profile","Output")->records.size();}
 bool Correctness(std::ofstream& log){
  ClearTranscript("profile","Output");bool ok=true;
  SCROLLINFO emptyScroll={sizeof(emptyScroll),SIF_RANGE|SIF_PAGE|SIF_POS,0,100,1,0,0};
  Find("profile","Output")->text.SetScrollInfo(SB_VERT,&emptyScroll,FALSE);
  ok=ShouldFollow(Find("profile","Output")->text)&&ok;
  log<<"empty transcript follows despite uninitialized scrollbar range "<<(ok?"PASS":"FAIL")<<"\n";
  WorkspaceRecord record;record.connectionId="profile";record.sequence=1;
  WorkspaceRun run;run.text=L"{\\} \u03bb\U0001f409\r\nnext";run.bold=true;run.foreground=RGB(220,30,70);record.runs.push_back(run);
  record.firstSourceCookie=10;
  ok=AppendRecord("Output",record)&&ok;
  ok=ok&&RetainedSourceAfter("profile","Output",10)&&!RetainedSourceAfter("profile","Output",11)&&RetainedSourceAfter("absent","Output",100);
  wchar_t actual[256]={0};GETTEXTEX get={sizeof(actual),GT_DEFAULT,1200,NULL,NULL};::SendMessageW(GetDlgItem(100)->m_hWnd,EM_GETTEXTEX,(WPARAM)&get,(LPARAM)actual);
  ok=ok&&std::wstring(actual)==L"{\\} \u03bb\U0001f409\rnext\r";
  CRichEditCtrl& text=Find("profile","Output")->text;text.SetSel(0,1);CHARFORMAT2W format;memset(&format,0,sizeof(format));format.cbSize=sizeof(format);::SendMessageW(text.m_hWnd,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);
  ok=ok&&(format.dwEffects&CFE_BOLD)&&format.crTextColor==RGB(220,30,70);
  log<<"escaped RTF, exact Unicode/newlines and style "<<(ok?"PASS":"FAIL")<<"\n";
  ClearTranscript("profile","Output");record.runs[0].text=L"row \u03bb\U0001f409";
  BeginRenderBatch();BeginRenderBatch();
  for(unsigned i=1;i<=2000;++i){record.sequence=i;ok=AppendRecord("Output",record)&&ok;}
  EndRenderBatch();EndRenderBatch();
  // A selected retained Unicode row must survive trimming inside a nested batch.
  text.SetSel(1999*8,1999*8+7);
  BeginRenderBatch();BeginRenderBatch();
  record.sequence=2001;record.runs[0].text=L"linked \u03bb\U0001f409";record.runs[0].action="look after-prune";record.runs[0].actionType=0x0400;ok=AppendRecord("Output",record)&&ok;
  Transcript* t=Find("profile","Output");ok=ok&&t->records.size()==1000&&t->links.size()==1;
  EndRenderBatch();ok=ok&&RenderBatchDepth()==1;EndRenderBatch();
  long selectedStart,selectedEnd;text.GetSel(selectedStart,selectedEnd);
  ok=ok&&selectedStart==998*8&&selectedEnd==998*8+7;
  ok=ok&&TextLength(t->text)==999*8+11;
  log<<"pruned projection characters="<<TextLength(t->text)<<" expected="<<999*8+11<<"\n";
  if(t->links.size()==1){const auto& link=t->links[0];TEXTRANGEW range={{link.first,link.last},actual};SendMessageW(text.m_hWnd,EM_GETTEXTRANGE,0,(LPARAM)&range);ok=ok&&std::wstring(actual)==L"linked \u03bb\U0001f409";
    CString action;SetActionHandler([&](const CString& id,unsigned short,const CString& value){if(id=="profile")action=value;});ENLINK event={0};event.msg=WM_LBUTTONUP;event.chrg.cpMin=link.first;event.chrg.cpMax=link.last;LRESULT result=0;OnLink(100,(NMHDR*)&event,&result);ok=ok&&result==1&&action=="look after-prune";
  }
  SetActionHandler(ActionHandler());
  log<<"prefix trim preserves exact retained hyperlink target and Unicode range "<<(ok?"PASS":"FAIL")<<"\n";
  ClearTranscript("profile","Output");return ok;
 }
 bool ImageUpdate(std::ofstream& log,const std::shared_ptr<CInlineImage>& image){
  ClearTranscript("profile","Output");WorkspaceRecord row;row.connectionId="profile";row.sequence=1;row.image=image;row.imageAction="https://fixture.invalid/image";WorkspaceRun fallback;fallback.text=L"[https://fixture.invalid/image]";row.runs.push_back(fallback);
  image->state_.store(CInlineImage::Loading);bool ok=AppendRecord("Output",row);row.sequence=2;row.image.reset();row.imageAction.Empty();row.runs[0].text=L"following \u03bb\U0001f409";row.runs[0].action="look following";row.runs[0].actionType=0x0400;ok=AppendRecord("Output",row)&&ok;
  Transcript* t=Find("profile","Output");t->text.SetSel(t->renderedEnds[0],t->renderedEnds[1]-1);
  image->state_.store(CInlineImage::Ready);const double before=Now();SendMessage(WM_TIMER,1,0);log<<"single image update us="<<(Now()-before)*1e6<<"\n";
  long start,end;t->text.GetSel(start,end);wchar_t actual[256]={0};TEXTRANGEW selected={{start,end},actual};SendMessageW(t->text.m_hWnd,EM_GETTEXTRANGE,0,(LPARAM)&selected);ok=ok&&std::wstring(actual)==L"following \u03bb\U0001f409"&&t->records.size()==2&&t->renderedEnds.size()==2&&t->imageStates[0]==CInlineImage::Ready;
  const auto link=std::find_if(t->links.begin(),t->links.end(),[](const LinkRange& l){return l.action=="look following";});
  if(link==t->links.end())ok=false;else{TEXTRANGEW range={{link->first,link->last},actual};SendMessageW(t->text.m_hWnd,EM_GETTEXTRANGE,0,(LPARAM)&range);ok=ok&&std::wstring(actual)==L"following \u03bb\U0001f409";}
  log<<"in-place ready image preserves following Unicode selection and link range "<<(ok?"PASS":"FAIL")<<"\n";
  t->text.SetSel(-1,-1);t->text.SendMessage(EM_SCROLLCARET);
  image->state_.store(CInlineImage::Loading);SendMessage(WM_TIMER,1,0);
  t->text.SetSel(-1,-1);t->text.SendMessage(EM_SCROLLCARET);
  t->text.LineScroll(100000); // Native hidden fixtures suppress caret scrolling.
  SCROLLINFO atBottom={sizeof(atBottom),SIF_ALL};t->text.GetScrollInfo(SB_VERT,&atBottom);
  atBottom.fMask=SIF_POS;atBottom.nPos=atBottom.nMax;t->text.SetScrollInfo(SB_VERT,&atBottom,FALSE);
  const bool wasFollowing=ShouldFollow(t->text);
  image->state_.store(CInlineImage::Ready);SendMessage(WM_TIMER,1,0);
  t->text.GetSel(start,end);ok=ok&&wasFollowing&&start==end&&end==TextLength(t->text)&&ShouldFollow(t->text);
  if(!ok){SCROLLINFO scroll={sizeof(scroll),SIF_ALL};t->text.GetScrollInfo(SB_VERT,&scroll);log<<"follow diagnostic before="<<wasFollowing<<" start="<<start<<" end="<<end<<" length="<<TextLength(t->text)<<" scroll="<<scroll.nPos<<" max="<<scroll.nMax<<" page="<<scroll.nPage<<"\n";}
  log<<"ready image keeps automatic following at the transcript end "<<(ok?"PASS":"FAIL")<<"\n";
  ClearTranscript("profile","Output");return ok;
 }
 bool DeferredCheck(WorkspaceTestFrame* frame,ProfilePane& other,std::ofstream& log){
  ClearTranscript("profile","Output");bool ok=other.Create(frame,0xe946)&&other.OpenTranscript("profile","Output");other.EnableDocking(CBRS_ALIGN_ANY);other.AttachToTabWnd(this,DM_SHOW,TRUE);
  WorkspaceRecord row;row.connectionId="profile";row.sequence=1;WorkspaceRun run;run.text=L"inactive \u03bb\U0001f409";row.runs.push_back(run);ok=AppendRecord("Output",row)&&ok;
  Transcript* t=Find("profile","Output");ok=ok&&t->deferred&&t->records.size()==1&&TextLength(t->text)==0;
  CBaseTabbedPane* group=GetParentTabbedPane();auto* tabs=group->GetUnderlyingWindow();tabs->SetActiveTab(tabs->GetTabFromHwnd(m_hWnd));RefreshDeferred();
  wchar_t actual[128]={0};GETTEXTEX get={sizeof(actual),GT_DEFAULT,1200,NULL,NULL};SendMessageW(t->text.m_hWnd,EM_GETTEXTEX,(WPARAM)&get,(LPARAM)actual);
  ok=ok&&!t->deferred&&std::wstring(actual)==L"inactive \u03bb\U0001f409\r"&&t->records.size()==1;
  log<<"inactive native tab keeps model, renders exactly once on activation "<<(ok?"PASS":"FAIL")<<"\n";
  return ok;
 }
 void Inspect(std::ofstream& csv,const char* phase,int sequence){
  LARGE_INTEGER a,b,f;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
  const ViewState state=SaveView(Find("profile","Output")->text);(void)state;
  QueryPerformanceCounter(&b);csv<<phase<<","<<sequence<<",view,"<<(b.QuadPart-a.QuadPart)*1e6/f.QuadPart<<"\n";
 }
};
double Now(){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return (double)t.QuadPart/f.QuadPart;}
class ProfileApp:public CWinApp {
 int result=1;
 BOOL InitInstance(){
  AfxOleInit();AfxInitRichEdit2();std::ofstream log("WinRel/workspace-performance.txt");log.setf(std::ios::unitbuf);
  std::ofstream csv("WinRel/workspace-performance.csv");csv<<"phase,sequence,operation,microseconds\n";
  WorkspaceTestFrame* frame=new WorkspaceTestFrame;bool ok=frame->Create(NULL,"Owned performance fixture",WS_OVERLAPPEDWINDOW,CRect(40,40,1200,850))!=FALSE;
  frame->EnableWorkspaceDocking();ProfilePane pane,other;ok=ok&&pane.Create(frame);pane.EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(&pane);ok=ok&&pane.OpenTranscript("profile","Output");
  ok=pane.Correctness(log)&&ok;
  auto image=CInlineImage::Request("synthetic:performance");for(int i=0;i<100&&image->GetState()==CInlineImage::Loading;++i)Sleep(5);
  BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=640;info.bmiHeader.biHeight=240;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* pixels=NULL;
  image->bitmap_=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);ok=ok&&pixels;
  if(!ok){log<<"FAIL setup\n";return FALSE;}
  for(int i=0;i<640*240;++i)((DWORD*)pixels)[i]=0xffaa6622;image->width_=640;image->height_=240;image->state_.store(CInlineImage::Ready);
  unsigned inputCount=0;pane.SetInputHandler([&](const CString& id,const std::wstring& text){++inputCount;return id=="profile"&&text==L"input \u03bb\U0001f409";});
  const char* phases[]={"plain","paused","styled","images","batched-plain","batched-styled"};
  for(int phase=0;phase<6;++phase){
   pane.ClearTranscript("profile","Output");pane.Pause(phase==1);const double began=Now();std::vector<double> latency;
   for(unsigned i=1;i<=5000;++i){
    if(phase>=4 && i%25==1){pane.BeginRenderBatch();pane.BeginRenderBatch();}
    WorkspaceRecord record;record.connectionId="profile";record.sequence=i;
    for(int j=0;j<((phase==2||phase==5)?8:1);++j){WorkspaceRun run;run.text=std::wstring((phase==2||phase==5)?25:200,L'x');run.foreground=RGB(100,j*20,200);run.bold=j%2!=0;record.runs.push_back(run);}
    if(phase==3&&i%40==0){record.image=image;record.imageAction="https://fixture.invalid/image.png";}
    double before=Now();ok=pane.AppendRecord("Output",record)&&ok;double micros=(Now()-before)*1e6;latency.push_back(micros);csv<<phases[phase]<<","<<i<<",append,"<<micros<<"\n";
    if(phase>=4 && i%25==0){pane.EndRenderBatch();ok=ok&&pane.RenderBatchDepth()==1;pane.EndRenderBatch();ok=ok&&pane.RenderBatchDepth()==0;}
    if(i%100==0){pane.Inspect(csv,phases[phase],i);::SetWindowTextW(pane.GetDlgItem(2)->m_hWnd,L"input \u03bb\U0001f409");before=Now();pane.SendMessage(WM_COMMAND,MAKEWPARAM(3,BN_CLICKED),(LPARAM)pane.GetDlgItem(3)->m_hWnd);csv<<phases[phase]<<","<<i<<",input-dispatch,"<<(Now()-before)*1e6<<"\n";}
   }
   std::sort(latency.begin(),latency.end());PROCESS_MEMORY_COUNTERS_EX m={0};m.cb=sizeof(m);GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&m,sizeof(m));
   log<<phases[phase]<<" records=5000 seconds="<<Now()-began<<" append-p50-us="<<latency[2500]<<" p95="<<latency[4750]<<" p99="<<latency[4950]<<" max="<<latency.back()<<" retained="<<pane.Count()<<" private="<<m.PrivateUsage<<" gdi="<<GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<<"\n";
  }
  ok=ok&&inputCount==300;frame->ShowWindow(SW_SHOWNA);frame->AdjustDockingLayout();frame->UpdateWindow();
  ok=pane.ImageUpdate(log,image)&&ok;ok=pane.DeferredCheck(frame,other,log)&&ok;pane.ClearTranscript("profile","Output");frame->DestroyWindow();log<<(ok?"PASS":"FAIL")<<" component performance: rendering, view capture and own-pane human dispatch; no game transport\n";result=ok?0:1;return FALSE;
 }
 int ExitInstance(){return result;}
} app;
