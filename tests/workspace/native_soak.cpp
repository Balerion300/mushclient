#include "stdafx.h"
#include <atomic>
#include <memory>
#include <string>
#define private public
#include "InlineImage.h"
#undef private
#include "WorkspacePane.h"
#include <psapi.h>
#include <fstream>

class InspectPane : public CWorkspacePane
{
public:
  size_t Records(const CString& id,const CString& name){Transcript* t=Find(id,name);return t?t->records.size():0;}
  unsigned __int64 First(const CString& id,const CString& name){Transcript* t=Find(id,name);return t&&!t->records.empty()?t->records.front().sequence:0;}
  unsigned __int64 Last(const CString& id,const CString& name){Transcript* t=Find(id,name);return t&&!t->records.empty()?t->records.back().sequence:0;}
};
struct Resources { SIZE_T bytes;DWORD gdi; };
Resources Sample()
{
  PROCESS_MEMORY_COUNTERS_EX memory;memset(&memory,0,sizeof(memory));memory.cb=sizeof(memory);
  GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&memory,sizeof(memory));
  Resources result={memory.PrivateUsage,GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)};return result;
}
class SoakApp : public CWinApp
{
  int result;
public:
  SoakApp():result(1){}
  virtual BOOL InitInstance()
    {
    AfxOleInit();AfxInitRichEdit2();
    CFrameWnd* allocated=new CFrameWnd;CFrameWnd& frame=*allocated;
    InspectPane tabs,split,mixed;
    std::ofstream log("WinRel/workspace-native-soak.txt");
    bool ok=frame.Create(NULL,"Isolated native workspace soak",WS_OVERLAPPEDWINDOW,CRect(50,50,1100,800))!=FALSE;
    ok=ok && tabs.Create(&frame) && split.Create(&frame,ID_WORKSPACE_BAR+1) && mixed.Create(&frame,ID_WORKSPACE_BAR+2);
    if(!ok){log<<"FAIL native creation\n";return FALSE;}
    frame.EnableDocking(CBRS_ALIGN_ANY);
    tabs.EnableDocking(CBRS_ALIGN_ANY);split.EnableDocking(CBRS_ALIGN_ANY);mixed.EnableDocking(CBRS_ALIGN_ANY);
    frame.DockControlBar(&tabs,AFX_IDW_DOCKBAR_RIGHT);frame.DockControlBar(&mixed,AFX_IDW_DOCKBAR_BOTTOM);
    frame.ShowControlBar(&tabs,TRUE,FALSE);frame.ShowControlBar(&mixed,TRUE,FALSE);frame.RecalcLayout();
    ok=ok && tabs.OpenTranscript("session-A","Plain") && tabs.OpenTranscript("session-B","Plain") && mixed.OpenTranscript("session-C","Mixed");
    unsigned int pruneCount=0,clearCount=0;
    const auto pruned=[&](const CString&,const CString&,const std::deque<WorkspaceRecord>& retained)
      {if(retained.empty())++clearCount;else {++pruneCount;if(retained.size()>2000)ok=false;}};
    tabs.SetPruneHandler(pruned);split.SetPruneHandler(pruned);mixed.SetPruneHandler(pruned);
    std::shared_ptr<CInlineImage> image=CInlineImage::Request("synthetic:soak4x4");
    for(int attempts=0;attempts<100 && image->GetState()==CInlineImage::Loading;++attempts)::Sleep(5);
    BITMAPINFO info;memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=4;info.bmiHeader.biHeight=4;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* pixels=NULL;image->bitmap_=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);
    if(!image->bitmap_ || !pixels){log<<"FAIL image fixture\n";return FALSE;}
    for(int i=0;i<16;++i)((DWORD*)pixels)[i]=0xFF32A0FF;
    image->width_=4;image->height_=4;image->state_.store(CInlineImage::Ready);
    std::wstring reviewed;CString reviewId;
    const auto review=[&](const CString& id,const std::wstring& command){reviewId=id;reviewed=command;};
    tabs.SetInputHandler(review);split.SetInputHandler(review);mixed.SetInputHandler(review);
    tabs.SetInputPrefix("session-B","Plain",L"+staff ");
    InspectPane* bHost=&tabs;
    std::vector<Resources> samples;
    const DWORD began=GetTickCount();
    const unsigned int total=8000;
    for(unsigned int sequence=1;sequence<=total;++sequence)
      {
      WorkspaceRecord record;record.sequence=sequence;record.receivedAtMillis=1700000000000ULL+sequence;
      WorkspaceRun run;run.text=L"Record \u03bb \u00e5 \U0001f409 with styled output ";
      wchar_t number[32];swprintf_s(number,L"%u",sequence);run.text+=number;
      run.foreground=RGB(sequence%255,(sequence*3)%255,(sequence*7)%255);run.bold=sequence%2==0;run.italic=sequence%3==0;run.underline=sequence%5==0;
      record.runs.push_back(run);
      record.connectionId="session-A";ok=tabs.AppendRecord("Plain",record)&&ok;
      record.connectionId="session-B";ok=bHost->AppendRecord("Plain",record)&&ok;
      record.connectionId="session-C";
      if(sequence%25==0){record.image=image;record.imageWidth=16;record.imageHeight=16;}
      ok=mixed.AppendRecord("Mixed",record)&&ok;
      if(sequence==2500)
        {tabs.FocusTranscript(1);ok=tabs.MoveSelectedTo(split)&&ok;bHost=&split;
         frame.FloatControlBar(&split,CPoint(200,160),CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC);frame.ShowControlBar(&split,TRUE,FALSE);}
      if(sequence%1000==0)
        {
        ok=tabs.Records("session-A","Plain")<=2000 && bHost->Records("session-B","Plain")<=2000 && mixed.Records("session-C","Mixed")<=2000 && ok;
        ok=tabs.Last("session-A","Plain")==sequence && bHost->Last("session-B","Plain")==sequence && mixed.Last("session-C","Mixed")==sequence && ok;
        if(sequence>2000)ok=tabs.First("session-A","Plain")>sequence-2000 && ok;
        frame.DockControlBar(bHost,AFX_IDW_DOCKBAR_LEFT);frame.RecalcLayout();
        frame.FloatControlBar(bHost,CPoint(210,170),CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC);
        bHost->CalcDynamicLayout(520+(sequence/1000)%2*80,LM_HORZ|LM_COMMIT);bHost->GetDockingFrame()->RecalcLayout();
        Resources resource=Sample();samples.push_back(resource);
        log<<"records-per-stream="<<sequence<<" retained="<<tabs.Records("session-A","Plain")<<","<<bHost->Records("session-B","Plain")<<","<<mixed.Records("session-C","Mixed")
           <<" private="<<resource.bytes<<" gdi="<<resource.gdi<<" elapsed-ms="<<GetTickCount()-began<<"\n";log.flush();
        if(sequence==4000){ok=mixed.ClearTranscript("session-C","Mixed")&&ok;ok=mixed.Records("session-C","Mixed")==0&&ok;}
        }
      }
    ok=ok && pruneCount>0 && clearCount==1 && bHost->InputPrefix("session-B","Plain")==L"+staff ";
    ::SetWindowTextW(bHost->GetDlgItem(2)->m_hWnd,L"\u03bb\U0001f409");
    bHost->SendMessage(WM_COMMAND,MAKEWPARAM(3,BN_CLICKED),(LPARAM)bHost->GetDlgItem(3)->m_hWnd);
    ok=ok && reviewId=="session-B" && reviewed==L"+staff \u03bb\U0001f409" && bHost->UnreadCount("session-B","Plain")>0;
    ok=ok && bHost->FocusTranscript(0) && bHost->UnreadCount("session-B","Plain")==0;
    SIZE_T privateLow=(SIZE_T)-1,privateHigh=0;DWORD gdiLow=0xFFFFFFFF,gdiHigh=0;
    for(size_t i=4;i<samples.size();++i){privateLow=min(privateLow,samples[i].bytes);privateHigh=max(privateHigh,samples[i].bytes);gdiLow=min(gdiLow,samples[i].gdi);gdiHigh=max(gdiHigh,samples[i].gdi);}
    const bool plateau=samples.size()==8 && privateHigh-privateLow<32*1024*1024 && gdiHigh-gdiLow<=32;
    ok=ok && plateau;
    log<<"late private spread="<<privateHigh-privateLow<<" late GDI spread="<<gdiHigh-gdiLow<<" prunes="<<pruneCount<<" clears="<<clearCount<<"\n";
    log<<(ok?"PASS":"FAIL")<<" native soak: "<<total*3<<" appended records, hidden isolated windows, synthetic decoded images\n";log.flush();
    frame.DestroyWindow();result=ok?0:1;return FALSE;
    }
  virtual int ExitInstance(){return result;}
} app;
