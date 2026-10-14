#include "stdafx.h"
#include <atomic>
#include <memory>
#include <string>
#define private public // Own synthetic image, no remote endpoint.
#include "InlineImage.h"
#undef private
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include <fstream>
#include <richole.h>

class DeferredPane : public CWorkspacePane
{
public:
  CRichEditCtrl& Control(){return m_transcripts[0]->text;}
  size_t Count(){return m_transcripts[0]->records.size();}
  size_t Removed(){return m_transcripts[0]->deferredRemoved;}
  long Length(){return TextLength(Control());}
  long End(size_t index){return m_transcripts[0]->renderedEnds[index];}
  bool Follow(){return ShouldFollow(Control());}
  void Refresh(){RefreshDeferred();}
  void Pause(bool pause){m_pause.SetCheck(pause?BST_CHECKED:BST_UNCHECKED);OnPause();}
  void Reset(){ClearTranscript("fictional-A","Output");}
};

CHARRANGE FindRecord(DeferredPane& pane,int number)
{
  wchar_t needle[32];swprintf(needle,32,L"RECORD-%04d",number);
  FINDTEXTEXW find={};find.chrg.cpMin=0;find.chrg.cpMax=-1;find.lpstrText=needle;
  if(::SendMessageW(pane.Control().m_hWnd,EM_FINDTEXTEXW,FR_DOWN,(LPARAM)&find)==-1)
    {CHARRANGE missing={-1,-1};return missing;}
  return find.chrgText;
}
std::wstring Selection(DeferredPane& pane)
{
  long first,last;pane.Control().GetSel(first,last);std::vector<wchar_t> text(last-first+1,0);
  ::SendMessageW(pane.Control().m_hWnd,EM_GETSELTEXT,0,(LPARAM)&text[0]);return &text[0];
}
std::wstring TextAt(HWND control,long first,long count)
{
  std::vector<wchar_t> text(count+1,0);TEXTRANGEW range={{first,first+count},&text[0]};
  ::SendMessageW(control,EM_GETTEXTRANGE,0,(LPARAM)&range);return &text[0];
}
bool Check(std::ofstream& log,const char* name,bool ok)
{log<<(ok?"PASS ":"FAIL ")<<name<<"\n";return ok;}

void Append(DeferredPane& pane,int first,int last,bool varied=false,
  const std::shared_ptr<CInlineImage>& image=std::shared_ptr<CInlineImage>())
{
  // Include CRLF normalization, surrogate pairs, combining marks and native
  // links/styles: offsets must come from rendered ends, never source length.
  for(int i=first;i<=last;++i){
    WorkspaceRecord record;record.connectionId="fictional-A";record.sequence=i;
    wchar_t row[32];swprintf(row,32,L"RECORD-%04d",i);WorkspaceRun run;run.text=row;
    if(varied){run.text+=L" \u03bb\U0001f409 e\u0301 ";run.text+=std::wstring(i%39,L'x');
      if(i%11==0)run.text+=L"\r\nwrapped\nrow\rending";
      run.bold=i%2==0;run.italic=i%3==0;run.underline=true;run.actionType=0x0400;run.action="fictional action";}
    record.runs.push_back(run);
    if(varied){run.text=L" tail \u96ea";run.bold=false;run.action.Empty();run.actionType=0;record.runs.push_back(run);}
    if(image && (i==100 || i==1100 || i==1450))record.image=image;
    if(!pane.AppendRecord("Output",record))throw "Append rejected";
  }
}

class DeferredApp : public CWinApp
{
  int result_=1;
  BOOL InitInstance()
  {
    AfxOleInit();AfxInitRichEdit2();std::ofstream log("deferred-view-results.txt");log.setf(std::ios::unitbuf);
    WorkspaceTestFrame* frame=new WorkspaceTestFrame;DeferredPane a;CWorkspacePane b;
    bool ok=frame->Create(NULL,"Fictional deferred transcript fixture",WS_OVERLAPPEDWINDOW,CRect(50,50,1000,800))!=FALSE;
    m_pMainWnd=frame;frame->EnableWorkspaceDocking();
    ok=a.Create(frame,0xe900)&&b.Create(frame,0xe901)&&ok;
    a.EnableDocking(CBRS_ALIGN_ANY);b.EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(&a);
    ok=a.OpenTranscript("fictional-A","Output")&&b.OpenTranscript("fictional-B","Output")&&ok;
    frame->ShowWindow(SW_SHOWNOACTIVATE);frame->RecalcLayout();
    Append(a,1,1500);CHARRANGE selected=FindRecord(a,1200);a.Control().SetSel(selected);a.Control().SendMessage(EM_SCROLLCARET);
    const long anchor=(long)::SendMessageW(a.Control().m_hWnd,EM_LINEINDEX,a.Control().GetFirstVisibleLine(),0);
    const std::wstring anchorText=TextAt(a.Control().m_hWnd,anchor,11);
    POINTL before={0,0};::SendMessageW(a.Control().m_hWnd,EM_POSFROMCHAR,(WPARAM)&before,anchor);
    CDockablePane* holder=NULL;b.AttachToTabWnd(&a,DM_SHOW,TRUE,&holder);frame->RecalcLayout();
    CTabbedPane* group=DYNAMIC_DOWNCAST(CTabbedPane,holder);
    if(!group){log<<"FAIL native tab creation\n";frame->DestroyWindow();return FALSE;}
    CMFCTabCtrl* tabs=group->GetTabWnd();
    const auto activate=[&](bool original){tabs->SetActiveTab(tabs->GetTabFromHwnd(original?a.m_hWnd:b.m_hWnd));if(original)a.Refresh();};
    const long oldLength=a.Length();Append(a,1501,2200);
    ok=Check(log,"native inactive appends prune model without changing native text",a.Count()==1199 && a.Length()==oldLength && a.Removed()==1001)&&ok;
    activate(true);selected=FindRecord(a,1200);long first,last;a.Control().GetSel(first,last);
    ok=Check(log,"RECORD-1200 selection survives native tab background pruning",Selection(a)==L"RECORD-1200" && first==selected.cpMin && last==selected.cpMax)&&ok;
    const long afterAnchor=(long)::SendMessageW(a.Control().m_hWnd,EM_LINEINDEX,a.Control().GetFirstVisibleLine(),0);
    POINTL after={0,0};::SendMessageW(a.Control().m_hWnd,EM_POSFROMCHAR,(WPARAM)&after,afterAnchor);
    ok=Check(log,"surviving reading anchor retains text and pixel offset",TextAt(a.Control().m_hWnd,afterAnchor,11)==anchorText && abs(before.y-after.y)<=1)&&ok;
    activate(false);Append(a,2201,4400);activate(true);a.Control().GetSel(first,last);
    ok=Check(log,"multiple prunes clamp genuinely evicted selection and reading anchor to start",first==0 && last==0 && a.Control().GetFirstVisibleLine()==0 && a.Removed()==0)&&ok;

    a.Reset();Append(a,1,1500,true);a.Pause(true);Append(a,1501,2200,true);
    selected=FindRecord(a,1300);a.Control().SetSel(selected);a.Control().SendMessage(EM_SCROLLCARET);
    a.Pause(false);
    ok=Check(log,"paused view uses latest user selection after model pruning with Unicode/styles",Selection(a)==L"RECORD-1300")&&ok;
    const long unchanged=a.Length();a.Pause(true);a.Pause(false);
    ok=Check(log,"pause/resume without incoming records preserves selection",a.Length()==unchanged && Selection(a)==L"RECORD-1300")&&ok;

    a.Reset();Append(a,1,1500,true);a.Pause(true);
    const CHARRANGE evicted=FindRecord(a,900),surviving=FindRecord(a,1200);
    a.Control().SetSel(evicted.cpMin,surviving.cpMax);Append(a,1501,2200,true);a.Pause(false);
    const CHARRANGE rebased=FindRecord(a,1200);a.Control().GetSel(first,last);
    ok=Check(log,"partially evicted selection clamps only its evicted endpoint",first==0 && last==rebased.cpMax)&&ok;

    a.Reset();Append(a,1,1500,true);a.Control().SetSel(-1,-1);a.Control().SendMessage(EM_SCROLLCARET);a.Control().LineScroll(-40);
    const bool reading=!a.Follow();activate(false);Append(a,1501,2200,true);activate(true);a.Control().GetSel(first,last);
    ok=Check(log,"caret at old end while reading above bottom stays at surviving old record end",reading && first==a.End(498) && last==first && last<a.Length() && !a.Follow())&&ok;
    a.Control().SetSel(-1,-1);a.Control().SendMessage(EM_SCROLLCARET);const bool following=a.Follow();
    activate(false);Append(a,2201,4400,true);activate(true);a.Control().GetSel(first,last);
    ok=Check(log,"bottom-follow survives repeated background prune cycles",following && first==last && last==a.Length() && a.Follow())&&ok;

    // Three shared synthetic loading objects become real, locally constructed
    // OLE images before replay. An earlier image shortens the surviving prefix.
    std::shared_ptr<CInlineImage> image=CInlineImage::Request("");
    // Empty source fails synchronously before queueing any image worker.
    image->state_.store(CInlineImage::Loading);
    a.Reset();Append(a,1,1500,true,image);selected=FindRecord(a,1200);a.Control().SetSel(selected);a.Control().SendMessage(EM_SCROLLCARET);
    activate(false);Append(a,1501,2200,true);
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=40;info.bmiHeader.biHeight=20;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* pixels=NULL;image->bitmap_=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);
    if(pixels)for(int i=0;i<800;++i)((DWORD*)pixels)[i]=0xFF7755AA;
    image->width_=40;image->height_=20;image->state_.store(CInlineImage::Ready);activate(true);
    IRichEditOle* ole=NULL;::SendMessageW(a.Control().m_hWnd,EM_GETOLEINTERFACE,0,(LPARAM)&ole);
    const int objects=ole?ole->GetObjectCount():0;if(ole)ole->Release();
    ok=Check(log,"selection survives UTF-16/style replay and loading-placeholder to OLE length changes",pixels && objects==2 && Selection(a)==L"RECORD-1200")&&ok;
    // Selection ending at the old document end must map to the old record end
    // when that final image shrinks, rather than into newly appended text.
    a.Reset();Append(a,1,1450,true,image);a.Control().SetSel(a.End(1448),a.End(1449));
    a.Pause(true);Append(a,1451,1455,true);a.Pause(false);a.Control().GetSel(first,last);
    ok=Check(log,"record boundary and old document-end selection endpoints remain exact",first==a.End(1448) && last==a.End(1449))&&ok;

    a.Reset();Append(a,1,1500);a.Control().SetSel(FindRecord(a,1200));
    a.BeginRenderBatch();Append(a,1501,1505);activate(false);Append(a,1506,2200);a.EndRenderBatch();activate(true);
    ok=Check(log,"batch view remains valid when native tab becomes inactive mid-batch",Selection(a)==L"RECORD-1200")&&ok;
    a.Reset();ok=Check(log,"clear resets deferred pruning bookkeeping",a.Removed()==0 && a.Length()==0)&&ok;
    log<<(ok?"PASS":"FAIL")<<" fictional native deferred selection/scroll regression\n";
    frame->DestroyWindow();result_=ok?0:1;return FALSE;
  }
  int ExitInstance(){return result_;}
};
DeferredApp app;
