#include "stdafx.h"
#include <atomic>
#include <memory>
#include <string>
#define private public // synthetic decoded-image fixture, no network request
#include "InlineImage.h"
#undef private
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include <richole.h>
#include <fstream>

LRESULT CALLBACK ProbeToolProcedure(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
  if(message==WM_GETMINMAXINFO){MINMAXINFO* size=(MINMAXINFO*)lp;size->ptMinTrackSize.x=340;size->ptMinTrackSize.y=440;return 0;}
  return DefWindowProcW(window,message,wp,lp);
}

class ViewProbePane : public CWorkspacePane
{
public:
  size_t Count(const CString& id,const CString& name){Transcript* t=Find(id,name);return t?t->records.size():0;}
};
long ImageWidthPixels(HWND text)
{
  IRichEditOle* pictures=NULL;::SendMessageW(text,EM_GETOLEINTERFACE,0,(LPARAM)&pictures);
  if(!pictures)return 0;REOBJECT object;memset(&object,0,sizeof(object));object.cbStruct=sizeof(object);
  HRESULT status=pictures->GetObject(0,&object,REO_GETOBJ_NO_INTERFACES);pictures->Release();
  return SUCCEEDED(status)?MulDiv(object.sizel.cx,96,2540):0;
}
bool ProbeViewportImagesAndScroll(std::ofstream& receipt)
{
  WorkspaceTestFrame* frame=new WorkspaceTestFrame;ViewProbePane images,scrollPane;
  bool ok=frame->Create(NULL,"Viewport regression fixture",WS_OVERLAPPEDWINDOW,CRect(50,50,1000,800))!=FALSE;
  frame->EnableDocking(CBRS_ALIGN_ANY);
  ok=ok && images.Create(frame) && scrollPane.Create(frame,ID_WORKSPACE_BAR+1);
  images.EnableDocking(CBRS_ALIGN_ANY);scrollPane.EnableDocking(CBRS_ALIGN_ANY);
  frame->FloatControlBar(&images,CPoint(100,100),CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC);frame->ShowControlBar(&images,TRUE,FALSE);
  images.SetWindowPos(NULL,0,0,300,380,SWP_NOMOVE|SWP_NOZORDER);
  ok=ok && images.OpenTranscript("image-session","Images");
  WorkspaceRecord picture;picture.connectionId="image-session";picture.sequence=1;picture.image=CInlineImage::Request("synthetic:wide-fixture");
  for(int i=0;i<100 && picture.image->GetState()==CInlineImage::Loading;++i)::Sleep(5);
  BITMAPINFO info;memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1200;info.bmiHeader.biHeight=120;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  void* pixels=NULL;picture.image->bitmap_=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);
  if(!pixels){frame->DestroyWindow();return false;}for(int i=0;i<1200*120;++i)((DWORD*)pixels)[i]=0xFF8855AA;
  picture.image->width_=1200;picture.image->height_=120;picture.image->state_.store(CInlineImage::Ready);
  ok=ok && images.AppendRecord("Images",picture);
  HWND text=images.GetDlgItem(100)->m_hWnd;CRect viewport;::GetClientRect(text,&viewport);
  const long wide=ImageWidthPixels(text);ok=ok && wide>0 && wide<=viewport.Width()-12;
  unsigned callbacks=0;images.SetPruneHandler([&](const CString&,const CString&,const std::deque<WorkspaceRecord>&){++callbacks;});
  images.SetWindowPos(NULL,0,0,220,380,SWP_NOMOVE|SWP_NOZORDER);
  ::GetClientRect(text,&viewport);const long narrow=ImageWidthPixels(text);
  ok=ok && narrow>0 && narrow<wide && narrow<=viewport.Width()-12 && images.Count("image-session","Images")==1 && callbacks==0;
  receipt<<"wide decoded image viewport/reflow width="<<wide<<" -> "<<narrow<<" retained="<<images.Count("image-session","Images")<<" "<<(ok?"PASS":"FAIL")<<"\n";
  frame->FloatControlBar(&scrollPane,CPoint(150,150),CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC);frame->ShowControlBar(&scrollPane,TRUE,FALSE);
  scrollPane.SetWindowPos(NULL,0,0,440,320,SWP_NOMOVE|SWP_NOZORDER);ok=ok && scrollPane.OpenTranscript("scroll-session","Scroll");
  WorkspaceRecord record;record.connectionId="scroll-session";WorkspaceRun run;run.text=L"A completed long styled paragraph with Unicode \u03bb\U0001f409 for the scroll regression fixture.";record.runs.push_back(run);
  for(unsigned int i=1;i<=120;++i){record.sequence=i;ok=scrollPane.AppendRecord("Scroll",record)&&ok;}
  CRichEditCtrl* control=(CRichEditCtrl*)scrollPane.GetDlgItem(100);control->SetSel(-1,-1);control->SendMessage(EM_SCROLLCARET);
  for(int wheel=0;wheel<8;++wheel)::SendMessageW(control->m_hWnd,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
  long selectionStart,selectionEnd;control->GetSel(selectionStart,selectionEnd);GETTEXTLENGTHEX precise={GTL_PRECISE|GTL_NUMCHARS,1200};
  receipt<<"caret="<<selectionEnd<<" precise="<<::SendMessageW(control->m_hWnd,EM_GETTEXTLENGTHEX,(WPARAM)&precise,0)<<" wm="<<::SendMessageW(control->m_hWnd,WM_GETTEXTLENGTH,0,0)<<"\n";
  control->LineScroll(-30); // Same native scroll state as wheel movement; hidden windows suppress wheel dispatch.
  const int first=control->GetFirstVisibleLine();POINT before={0,0},after={0,0};::SendMessageW(control->m_hWnd,EM_GETSCROLLPOS,0,(LPARAM)&before);
  SCROLLINFO bar;memset(&bar,0,sizeof(bar));bar.cbSize=sizeof(bar);bar.fMask=SIF_ALL;::GetScrollInfo(control->m_hWnd,SB_VERT,&bar);
  receipt<<"scrollinfo pos="<<bar.nPos<<" max="<<bar.nMax<<" page="<<bar.nPage<<" first="<<first<<"\n";
  const bool scrolledUp=bar.nPos<bar.nMax-(int)bar.nPage+1;
  record.sequence=121;ok=scrollPane.AppendRecord("Scroll",record)&&ok;::SendMessageW(control->m_hWnd,EM_GETSCROLLPOS,0,(LPARAM)&after);
  receipt<<"newfirst="<<control->GetFirstVisibleLine()<<" scrollup="<<scrolledUp<<" beforepass="<<ok<<"\n";
  ok=ok && scrolledUp && control->GetFirstVisibleLine()==first && after.y==before.y;
  receipt<<"native scroll-up reader with caret at end preserves append anchor: first="<<first<<" y="<<before.y<<" -> "<<after.y<<" "<<(ok?"PASS":"FAIL")<<"\n";
  control->SetSel(-1,-1);control->SendMessage(EM_SCROLLCARET);
  record.sequence=122;ok=scrollPane.AppendRecord("Scroll",record)&&ok;
  bar.fMask=SIF_ALL;::GetScrollInfo(control->m_hWnd,SB_VERT,&bar);
  control->GetSel(selectionStart,selectionEnd);
  const bool followed=bar.nPos>=bar.nMax-(int)bar.nPage && selectionStart==selectionEnd && selectionEnd==::SendMessageW(control->m_hWnd,EM_GETTEXTLENGTHEX,(WPARAM)&precise,0);
  ok=ok && followed;
  receipt<<"bottom reader follows appended output: pos="<<bar.nPos<<" bottom="<<bar.nMax-(int)bar.nPage+1<<" "<<(followed?"PASS":"FAIL")<<"\n";
  picture.connectionId="scroll-session";picture.sequence=123;ok=scrollPane.AppendRecord("Scroll",picture)&&ok;
  control->SetSel(-1,-1);control->SendMessage(EM_SCROLLCARET);control->SetSel(selectionEnd-50,selectionEnd-30);control->LineScroll(-30);
  long savedStart,savedEnd;control->GetSel(savedStart,savedEnd);
  const long anchor=(long)::SendMessageW(control->m_hWnd,EM_LINEINDEX,control->GetFirstVisibleLine(),0);
  POINTL anchorBefore={0,0},anchorAfter={0,0};::SendMessageW(control->m_hWnd,EM_POSFROMCHAR,(WPARAM)&anchorBefore,anchor);
  scrollPane.SetWindowPos(NULL,0,0,320,320,SWP_NOMOVE|SWP_NOZORDER);
  ::SendMessageW(control->m_hWnd,EM_POSFROMCHAR,(WPARAM)&anchorAfter,anchor);control->GetSel(selectionStart,selectionEnd);
  const bool reflowAnchor=selectionStart==savedStart && selectionEnd==savedEnd && abs(anchorBefore.y-anchorAfter.y)<=1 && scrollPane.Count("scroll-session","Scroll")==123;
  ok=ok && reflowAnchor;
  receipt<<"image width reflow preserves selection and source anchor y="<<anchorBefore.y<<" -> "<<anchorAfter.y<<" retained="<<scrollPane.Count("scroll-session","Scroll")<<" "<<(reflowAnchor?"PASS":"FAIL")<<"\n";
  frame->DestroyWindow();return ok;
}

bool ProbeDockBounds(std::ofstream& receipt,int width,int height)
{
  WorkspaceTestFrame* frame=new WorkspaceTestFrame;CWorkspacePane primary,tool,bottom;
  bool ok=frame->Create(NULL,"Dock bounds fixture",WS_OVERLAPPEDWINDOW,CRect(50,50,50+width,50+height))!=FALSE;
  frame->EnableDocking(CBRS_ALIGN_ANY);
  ok=ok && primary.Create(frame) && tool.Create(frame,ID_WORKSPACE_BAR+1) && bottom.Create(frame,ID_WORKSPACE_BAR+2);
  primary.EnableDocking(CBRS_ALIGN_ANY);tool.EnableDocking(CBRS_ALIGN_ANY);bottom.EnableDocking(CBRS_ALIGN_ANY);
  HWND child=::CreateWindowExW(0,L"WorkspaceMinimumProbe",L"Docked tool",WS_POPUP,0,0,340,440,NULL,NULL,AfxGetInstanceHandle(),NULL);
  ok=ok && tool.HostTool(child,"Dock bounds tool") && primary.OpenTranscript("bounds-session","Right") && bottom.OpenTranscript("bounds-session","Bottom");
  frame->DockControlBar(&primary,AFX_IDW_DOCKBAR_RIGHT);frame->ShowControlBar(&primary,TRUE,FALSE);
  frame->DockControlBar(&tool,AFX_IDW_DOCKBAR_RIGHT);frame->ShowControlBar(&tool,TRUE,FALSE);
  frame->DockControlBar(&bottom,AFX_IDW_DOCKBAR_BOTTOM);frame->ShowControlBar(&bottom,TRUE,FALSE);
  frame->RecalcLayout();frame->RecalcLayout();
  CRect client;frame->GetClientRect(client);frame->ClientToScreen(client);
  const HWND panes[]={primary.m_hWnd,tool.m_hWnd,bottom.m_hWnd};
  for(size_t i=0;i<3;++i)
    {CRect rectangle;::GetClientRect(panes[i],&rectangle);::MapWindowPoints(panes[i],NULL,(POINT*)&rectangle,2);
     bool bounded=rectangle.left>=client.left-2 && rectangle.top>=client.top-2 && rectangle.right<=client.right+2 && rectangle.bottom<=client.bottom+2;
     receipt<<"dock frame="<<width<<"x"<<height<<" pane="<<i<<" rect="<<rectangle.left<<","<<rectangle.top<<","<<rectangle.right<<","<<rectangle.bottom<<" frameclient="<<client.left<<","<<client.top<<","<<client.right<<","<<client.bottom<<" "<<(bounded?"PASS":"FAIL")<<"\n";ok=ok&&bounded;}
  CRect hosted;::GetWindowRect(child,&hosted);ok=ok && hosted.left>=client.left && hosted.top>=client.top && hosted.right<=client.right && hosted.bottom<=client.bottom;
  frame->SetWindowPos(NULL,0,0,width*3/4,height*3/4,SWP_NOMOVE|SWP_NOZORDER);frame->RecalcLayout();frame->RecalcLayout();
  frame->GetClientRect(client);frame->ClientToScreen(client);::GetWindowRect(child,&hosted);
  ok=ok && hosted.left>=client.left && hosted.top>=client.top && hosted.right<=client.right && hosted.bottom<=client.bottom;
  receipt<<"actual right/bottom docking and shrinking child within frame: "<<(ok?"PASS":"FAIL")<<"\n";
  frame->DestroyWindow();return ok;
}

// Focused native probe: uses genuine MFC windows in an isolated process.
// This tests API docking transitions; user drag / DPI acceptance is separate.
class ProbeApp : public CWinApp
{
public:
  int result;
  ProbeApp():result(1){}
  virtual BOOL InitInstance()
    {
    AfxOleInit(); AfxInitRichEdit2();
    WorkspaceTestFrame* ownedFrame=new WorkspaceTestFrame;
    WorkspaceTestFrame& frame=*ownedFrame;
    CWorkspacePane pane;
    std::ofstream receipt("WinRel/workspace-native-probe.txt");receipt.setf(std::ios::unitbuf);
    bool ok=frame.Create(NULL,"Workspace native probe",WS_OVERLAPPEDWINDOW,CRect(50,50,1100,800))!=FALSE;
    frame.EnableWorkspaceDocking();ok=ok && pane.Create(&frame)!=FALSE;
    if (!ok) { receipt<<"FAIL creation\n"; return FALSE; }
    pane.EnableDocking(CBRS_ALIGN_ANY);
    frame.DockControlBar(&pane,AFX_IDW_DOCKBAR_RIGHT); frame.ShowControlBar(&pane,TRUE,FALSE); frame.RecalcLayout();
    ok=ok && pane.OpenTranscript("session-A","Output");
    WorkspaceRecord record; record.connectionId="session-A"; record.sequence=1;
    WorkspaceRun run; run.text=L"Native Unicode \u00e5\u03bb\U0001f409"; run.foreground=RGB(31,201,81); run.bold=true;
    run.action="destination-a";run.actionType=0x0C00;record.runs.push_back(run);
    ok=ok && pane.AppendRecord("Output",record) && !pane.AppendRecord("Output",record);
    HWND text=pane.GetDlgItem(100)->m_hWnd;
    wchar_t before[256]={0},after[256]={0}; ::GetWindowTextW(text,before,256);
    receipt<<"textchars="<<wcslen(before)<<" nativeunicode="<<::IsWindowUnicode(text)<<" open="<<ok<<"\n";
    for(size_t i=0;i<wcslen(before);++i) receipt<<std::hex<<(unsigned)before[i]<<","; receipt<<std::dec<<"\n";
    ok=ok && wcsstr(before,L"\u03bb\U0001f409")!=NULL;
    CHARRANGE selection={0,1}; ::SendMessageW(text,EM_EXSETSEL,0,(LPARAM)&selection);
    CHARFORMAT2W format;memset(&format,0,sizeof(format));format.cbSize=sizeof(format);
    ::SendMessageW(text,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);
    receipt<<"structsize="<<sizeof(format)<<" mask="<<std::hex<<format.dwMask<<std::dec<<"\n";
    receipt<<"color="<<std::hex<<format.crTextColor<<" effects="<<format.dwEffects<<std::dec<<"\n";
    ok=ok && format.crTextColor==RGB(31,201,81) && (format.dwEffects&CFE_BOLD)!=0;
    receipt<<"native Unicode/style/duplicate sequence: "<<(ok?"PASS":"FAIL")<<"\n";
    frame.FloatControlBar(&pane,CPoint(150,150),CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC);frame.RecalcLayout();
    ok=ok && pane.IsFloating();
    pane.GetParentMiniFrame()->SetWindowPos(NULL,160,160,660,460,SWP_NOZORDER);frame.RecalcLayout();
    CRect rect;pane.GetWindowRect(rect);
    ok=ok && rect.Width()>=620 && rect.Height()>=400;
    ::GetWindowTextW(text,after,256);ok=ok && wcscmp(before,after)==0;
    receipt<<"float/resize/text retention: "<<(ok?"PASS":"FAIL")<<" size="<<rect.Width()<<"x"<<rect.Height()<<"\n";
    frame.DockControlBar(&pane,AFX_IDW_DOCKBAR_BOTTOM);frame.RecalcLayout();
    ok=ok && !pane.IsFloating(); ::GetWindowTextW(text,after,256);ok=ok && wcscmp(before,after)==0;
    CWorkspacePane separate;ok=ok && separate.Create(&frame,ID_WORKSPACE_BAR+1);separate.EnableDocking(CBRS_ALIGN_ANY);
    ok=ok && separate.OpenTranscript("session-B","Output");separate.AttachToTabWnd(&pane,DM_SHOW);
    int clicks=0;CString clickedId;
    pane.SetActionHandler([&](const CString& id,unsigned short type,const CString& action)
      {if(type==0x0C00 && action=="destination-a") { ++clicks;clickedId=id; }});
    ENLINK link;memset(&link,0,sizeof(link));link.nmhdr.hwndFrom=text;link.nmhdr.idFrom=100;link.nmhdr.code=EN_LINK;
    link.msg=WM_LBUTTONUP;link.chrg.cpMin=0;link.chrg.cpMax=1;
    pane.SendMessage(WM_NOTIFY,100,(LPARAM)&link);
    ok=ok && clicks==1 && clickedId=="session-A";
    record.connectionId="session-A";record.sequence=2;ok=ok && pane.AppendRecord("Output",record);
    ok=ok && pane.UnreadCount("session-A","Output")>0 && pane.FocusTranscript(0) && pane.UnreadCount("session-A","Output")==0;
    separate.FocusTranscript(0);
    receipt<<"unread count and deliberate native pane focus acknowledgment: "<<(ok?"PASS":"FAIL")<<"\n";
    frame.FloatControlBar(&separate,CPoint(200,200),CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC);
    record.connectionId="session-B";record.sequence=2;
    ok=ok && separate.AppendRecord("Output",record) && !pane.AppendRecord("Output",record);
    std::wstring reviewed;CString reviewIdentity;
    separate.SetInputHandler([&](const CString& identity,const std::wstring& command){reviewIdentity=identity;reviewed=command;return true;});
    separate.SetInputPrefix("session-B","Output",L"+channel ");
    ::SetWindowTextW(separate.GetDlgItem(2)->m_hWnd,L"\u03bb\U0001f409");
    separate.SendMessage(WM_COMMAND,MAKEWPARAM(3,BN_CLICKED),(LPARAM)separate.GetDlgItem(3)->m_hWnd);
    ok=ok && reviewIdentity=="session-B" && reviewed==L"+channel \u03bb\U0001f409" && separate.InputPrefix("session-B","Output")==L"+channel ";
    ::SetWindowTextW(separate.GetDlgItem(2)->m_hWnd,L"unsent draft");
    MSG historyMessage={0};historyMessage.hwnd=separate.GetDlgItem(2)->m_hWnd;historyMessage.message=WM_KEYDOWN;historyMessage.wParam=VK_UP;separate.PreTranslateMessage(&historyMessage);
    ::GetWindowTextW(separate.GetDlgItem(2)->m_hWnd,after,256);ok=ok && wcscmp(after,L"\u03bb\U0001f409")==0;
    historyMessage.wParam=VK_DOWN;separate.PreTranslateMessage(&historyMessage);::GetWindowTextW(separate.GetDlgItem(2)->m_hWnd,after,256);ok=ok && wcscmp(after,L"unsent draft")==0;
    separate.SetInputHandler([&](const CString&,const std::wstring&){return false;});separate.SendMessage(WM_COMMAND,MAKEWPARAM(3,BN_CLICKED),(LPARAM)separate.GetDlgItem(3)->m_hWnd);
    ::GetWindowTextW(separate.GetDlgItem(2)->m_hWnd,after,256);ok=ok && wcscmp(after,L"unsent draft")==0;
    bool connected=false;separate.SetConnectionHandler([&](const CString& id){return id=="session-B" && connected;});separate.SendMessage(WM_TIMER,1,0);
    ok=ok && !separate.GetDlgItem(3)->IsWindowEnabled();connected=true;separate.SendMessage(WM_TIMER,1,0);::GetWindowTextW(separate.GetDlgItem(8)->m_hWnd,after,256);
    ok=ok && separate.GetDlgItem(3)->IsWindowEnabled() && wcscmp(after,L"Ready")==0;
    receipt<<"disconnected/reconnected Send state and status: "<<(ok?"PASS":"FAIL")<<"\n";
    receipt<<"successful human submit history and failed submit retains draft: "<<(ok?"PASS":"FAIL")<<"\n";
    receipt<<"separate floating transcript retains routing owner: "<<(ok?"PASS":"FAIL")<<"\n";
    pane.CloseConnection("session-A");
    record.connectionId="session-A";record.sequence=2;ok=ok && !pane.AppendRecord("Output",record);
    record.connectionId="session-B";record.sequence=3;ok=ok && separate.AppendRecord("Output",record);
    receipt<<"redock/tab isolation/link owner/closed-session rejection: "<<(ok?"PASS":"FAIL")<<"\n";
    WorkspaceRoute route; route.connectionId="session-B";route.transcriptName="Tells";route.pattern=L"\u03bb";
    CString error;ok=ok && route.Compile(error) && route.Matches(record);
    route.regex=true;route.pattern=L"^Native.*\u03bb";ok=ok && route.Compile(error) && route.Matches(record);
    route.pattern=L"(a+)+$";ok=ok && route.Compile(error);
    record.runs[0].text=std::wstring(1000,L'a')+L"b";
    DWORD started=GetTickCount();ok=ok && !route.Matches(record) && GetTickCount()-started<1000;
    route.pattern=L"(";ok=ok && !route.Compile(error);
    route.pattern=L"safe";route.transcriptName="oUtPuT";ok=ok && !route.Compile(error) && error.Find("reserved")>=0;
    WorkspaceRecord imageRecord; imageRecord.connectionId="session-B";imageRecord.sequence=4;
    imageRecord.image=CInlineImage::Request("synthetic:4x4");
    for(int attempt=0;attempt<100 && imageRecord.image->GetState()==CInlineImage::Loading;++attempt) ::Sleep(5);
    BITMAPINFO bitmapInfo;memset(&bitmapInfo,0,sizeof(bitmapInfo));bitmapInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth=4;bitmapInfo.bmiHeader.biHeight=4;bitmapInfo.bmiHeader.biPlanes=1;bitmapInfo.bmiHeader.biBitCount=32;
    void* pixels=NULL;imageRecord.image->bitmap_=CreateDIBSection(NULL,&bitmapInfo,DIB_RGB_COLORS,&pixels,NULL,0);
    for(int pixel=0;pixel<16;++pixel) ((DWORD*)pixels)[pixel]=0xFFFF00FF;
    imageRecord.image->width_=4;imageRecord.image->height_=4;imageRecord.image->state_.store(CInlineImage::Loading);
    ok=ok && separate.AppendRecord("Output",imageRecord);
    imageRecord.image->state_.store(CInlineImage::Ready);separate.SendMessage(WM_TIMER,1,0);
    IRichEditOle* objects=NULL;::SendMessageW(separate.GetDlgItem(100)->m_hWnd,EM_GETOLEINTERFACE,0,(LPARAM)&objects);
    int objectCount=objects?objects->GetObjectCount():0;if(objects)objects->Release();
    receipt<<"image fixture state="<<imageRecord.image->GetState()<<" appended="<<ok<<"\n";
    ok=ok && objectCount==1;
    receipt<<"async inline image imports native object count="<<objectCount<<" "<<(ok?"PASS":"FAIL")<<"\n";
    receipt<<"literal/regex Unicode route and pathological-match bounds: "<<(ok?"PASS":"FAIL")<<"\n";
    HWND transcriptText=separate.GetDlgItem(100)->m_hWnd;
    separate.GetDlgItem(6)->SendMessage(BM_SETCHECK,BST_CHECKED,0);
    separate.SendMessage(WM_COMMAND,MAKEWPARAM(6,BN_CLICKED),(LPARAM)separate.GetDlgItem(6)->m_hWnd);
    WorkspaceRecord pausedRecord;pausedRecord.connectionId="session-B";pausedRecord.sequence=5;
    WorkspaceRun pausedRun;pausedRun.text=L"pausedstream";pausedRecord.runs.push_back(pausedRun);
    ok=ok && separate.AppendRecord("Output",pausedRecord);
    ::GetWindowTextW(transcriptText,after,256);ok=ok && wcsstr(after,L"pausedstream")==NULL;
    separate.GetDlgItem(6)->SendMessage(BM_SETCHECK,BST_UNCHECKED,0);
    separate.SendMessage(WM_COMMAND,MAKEWPARAM(6,BN_CLICKED),(LPARAM)separate.GetDlgItem(6)->m_hWnd);
    ::GetWindowTextW(transcriptText,after,256);ok=ok && wcsstr(after,L"pausedstream")!=NULL;
    ::SetWindowTextW(separate.GetDlgItem(4)->m_hWnd,L"pausedstream");
    separate.SendMessage(WM_COMMAND,MAKEWPARAM(5,BN_CLICKED),(LPARAM)separate.GetDlgItem(5)->m_hWnd);
    CHARRANGE match;::SendMessageW(transcriptText,EM_EXGETSEL,0,(LPARAM)&match);ok=ok && match.cpMax-match.cpMin==12;
    ok=ok && separate.ClearTranscript("session-B","Output") && ::GetWindowTextLengthW(transcriptText)==0;
    receipt<<"pause retains records, resume, native search, scoped clear: "<<(ok?"PASS":"FAIL")<<"\n";
    WorkspacePaneLayout invalid;invalid.width=-500;invalid.height=999999;
    separate.ApplyLayout(invalid);CRect boundedRect;separate.GetWindowRect(boundedRect);const CSize bounded=boundedRect.Size();
    ok=ok && bounded.cx==220 && bounded.cy==1000;
    std::vector<WorkspacePaneLayout> captured;separate.GetLayouts(captured,true);
    ok=ok && captured.size()==1 && captured[0].connectionId=="session-B" && captured[0].detached;
    CWorkspacePane staff;ok=ok && staff.Create(&frame,0xe902) && staff.OpenTranscript("session-B","Staff");staff.EnableDocking(CBRS_ALIGN_ANY);frame.DockControlBar(&staff,AFX_IDW_DOCKBAR_RIGHT);
    unsigned int prunes=0;size_t retained=0;unsigned __int64 retainedLast=0;
    staff.SetPruneHandler([&](const CString& id,const CString& name,const std::deque<WorkspaceRecord>& records)
      {if(id=="session-B" && name=="Staff") {++prunes;retained=records.size();retainedLast=records.empty()?0:records.back().sequence;}});
    WorkspaceRecord boundedRecord;boundedRecord.connectionId="session-B";WorkspaceRun boundedRun;boundedRun.text=L"bounded";boundedRecord.runs.push_back(boundedRun);
    for(unsigned int sequence=1;sequence<=2001;++sequence)
      {boundedRecord.sequence=sequence;ok=ok && staff.AppendRecord("Staff",boundedRecord);}
    ok=ok && prunes==1 && retained==1000 && retainedLast==2001;
    ok=ok && staff.ClearTranscript("session-B","Staff") && prunes==2 && retained==0;
    receipt<<"named pane prune and clear callbacks use exact retained records: "<<(ok?"PASS":"FAIL")<<"\n";
    CWorkspacePane constrained;ok=ok && constrained.Create(&frame,0xE831);constrained.EnableDocking(CBRS_ALIGN_ANY);
    WNDCLASSW toolClass;memset(&toolClass,0,sizeof(toolClass));toolClass.lpfnWndProc=ProbeToolProcedure;toolClass.hInstance=AfxGetInstanceHandle();toolClass.lpszClassName=L"WorkspaceMinimumProbe";RegisterClassW(&toolClass);
    HWND constrainedWindow=::CreateWindowExW(0,toolClass.lpszClassName,L"Minimum probe",WS_POPUP,0,0,340,440,NULL,NULL,AfxGetInstanceHandle(),NULL);
    ok=ok && constrained.HostTool(constrainedWindow,"Constrained tool");
    const CSize toolMinimum=constrained.WorkspaceMinimumSize();
    constrained.ApplyLayout(invalid);const CSize toolPersisted=constrained.WorkspaceMinimumSize();
    const CSize toolDocked=constrained.WorkspaceMinimumSize();
    ok=ok && toolMinimum.cx>=352 && toolMinimum.cy>=470 && toolPersisted.cx>=352 && toolPersisted.cy>=470 && toolDocked.cy>=470;
    receipt<<"hosted child minimum survives float/dock/persisted clamping: "<<(ok?"PASS":"FAIL")<<"\n";
    CWorkspacePane toolHost;ok=ok && toolHost.Create(&frame,0xE830);toolHost.EnableDocking(CBRS_ALIGN_ANY);
    HWND tool=::CreateWindowExW(0,L"EDIT",L"tool retained",WS_POPUP|WS_CAPTION,0,0,200,200,NULL,NULL,AfxGetInstanceHandle(),NULL);
    ok=ok && toolHost.HostTool(tool,"Synthetic tool") && ::GetParent(tool)==toolHost.m_hWnd;
    frame.FloatControlBar(&toolHost,CPoint(100,100),CBRS_ALIGN_TOP|CBRS_SIZE_DYNAMIC);
    toolHost.SetWindowPos(NULL,0,0,620,410,SWP_NOMOVE|SWP_NOZORDER);
    CRect toolRect;::GetWindowRect(tool,&toolRect);
    ok=ok && toolRect.Width()==608 && toolRect.Height()==398;
    ::GetWindowTextW(tool,after,256);ok=ok && wcscmp(after,L"tool retained")==0;
    receipt<<"bounded layout and native tool reparent/resize/content: "<<(ok?"PASS":"FAIL")<<"\n";
    ok=ProbeViewportImagesAndScroll(receipt)&&ok;
    ok=ProbeDockBounds(receipt,1024,768)&&ok;
    ok=ProbeDockBounds(receipt,1536,1152)&&ok;
    receipt<<(ok?"PASS":"FAIL")<<" native workspace probe\n";receipt.close();
    frame.DestroyWindow();result=ok?0:1;return FALSE;
    }
  virtual int ExitInstance(){return result;}
} app;
