#include "stdafx.h"
#include "WorkspacePane.h"
#include "ModernFixture.h"
#include <fstream>

// Fictional records only; this fixture never opens a world or a network endpoint.
class BackgroundPane : public CWorkspacePane
{
public:
  CRichEditCtrl& Control() { return Find("background-fixture","Output")->text; }
  long End(size_t index) { return Find("background-fixture","Output")->renderedEnds[index]; }
  void Replay() { Rebuild(*Find("background-fixture","Output")); }
};

void SourceBackground(WorkspaceRecord& record,COLORREF color)
{
#ifndef WORKSPACE_BACKGROUND_BASELINE
  record.hasSurfaceColors=true;record.surfaceForeground=RGB(10,10,10);record.surfaceBackground=color;
#else
  (void)record;(void)color;
#endif
}

CHARFORMAT2W CharacterFormat(CRichEditCtrl& control,long first,long last)
{
  control.SetSel(first,last);
  CHARFORMAT2W result;memset(&result,0,sizeof(result));result.cbSize=sizeof(result);
  ::SendMessageW(control.m_hWnd,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&result);
  return result;
}
POINT CharacterPosition(HWND control,long position)
{
  POINT result={0,0};::SendMessageW(control,EM_POSFROMCHAR,(WPARAM)&result,position);return result;
}
bool FormatBackground(std::ofstream& receipt,CRichEditCtrl& control,const char* name,
  long first,long last,COLORREF expected,bool requirePlain)
{
  const CHARFORMAT2W format=CharacterFormat(control,first,last);
  const bool background=(format.dwMask&CFM_BACKCOLOR)!=0 &&
    !(format.dwEffects&CFE_AUTOBACKCOLOR) && format.crBackColor==expected;
  const bool plain=!(format.dwEffects&(CFE_LINK|CFE_BOLD|CFE_ITALIC|CFE_UNDERLINE));
  const bool ok=background && (!requirePlain || plain);
  receipt<<name<<" range="<<first<<":"<<last<<" background="<<format.crBackColor
    <<" effects="<<format.dwEffects<<" "<<(ok?"PASS":"FAIL")<<"\n";
  return ok;
}

// WM_PRINTCLIENT paints the actual native control into a DIB without relying on
// the desktop's foreground window, occlusion, display scaling, or cursor.
class NativePixels
{
  HWND window_;HDC target_;HBITMAP bitmap_;HGDIOBJ old_;DWORD* pixels_;
public:
  int width,height;
  NativePixels(HWND window):window_(window),target_(NULL),bitmap_(NULL),old_(NULL),pixels_(NULL)
  {
    RECT bounds;::GetClientRect(window,&bounds);width=bounds.right;height=bounds.bottom;
    BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    target_=CreateCompatibleDC(NULL);bitmap_=CreateDIBSection(target_,&info,DIB_RGB_COLORS,(void**)&pixels_,NULL,0);
    if(bitmap_ && target_){old_=SelectObject(target_,bitmap_);PatBlt(target_,0,0,width,height,BLACKNESS);
      ::SendMessageW(window_,WM_PRINTCLIENT,(WPARAM)target_,PRF_CLIENT|PRF_ERASEBKGND);GdiFlush();}
  }
  ~NativePixels(){if(old_)SelectObject(target_,old_);if(bitmap_)DeleteObject(bitmap_);if(target_)DeleteDC(target_);}
  COLORREF At(int x,int y) const
  {
    if(!pixels_ || x<0 || y<0 || x>=width || y>=height)return CLR_INVALID;
    const DWORD pixel=pixels_[y*width+x];return RGB((pixel>>16)&255,(pixel>>8)&255,pixel&255);
  }
  bool Patch(std::ofstream& receipt,const char* name,int x,int y,COLORREF expected) const
  {
    int matches=0;for(int dy=0;dy<4;++dy)for(int dx=0;dx<4;++dx)if(At(x+dx,y+dy)==expected)++matches;
    receipt<<name<<" at="<<x<<","<<y<<" RGB="<<At(x,y)<<" matching="<<matches<<"/16 "<<(matches==16?"PASS":"FAIL")<<"\n";
    return matches==16;
  }
  void Save(const char* path) const
  {
    if(!pixels_)return;BITMAPFILEHEADER header={0};BITMAPINFOHEADER info={0};
    info.biSize=sizeof(info);info.biWidth=width;info.biHeight=-height;info.biPlanes=1;info.biBitCount=32;
    header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+width*height*4;
    std::ofstream out(path,std::ios::binary);out.write((const char*)&header,sizeof(header));out.write((const char*)&info,sizeof(info));out.write((const char*)pixels_,width*height*4);
  }
};

bool Check(std::ofstream& receipt,BackgroundPane& pane,COLORREF background,const char* phase)
{
  CRichEditCtrl& control=pane.Control();const long firstEnd=pane.End(0),blankEnd=pane.End(1);
  bool ok=FormatBackground(receipt,control,"ordinary span",0,1,background,false);
  ok=FormatBackground(receipt,control,"explicit ANSI span",19,20,RGB(180,35,55),false)&&ok;
  const CHARFORMAT2W explicitFormat=CharacterFormat(control,19,20);
  ok=ok && (explicitFormat.dwEffects&(CFE_BOLD|CFE_ITALIC|CFE_UNDERLINE|CFE_LINK))==(CFE_BOLD|CFE_ITALIC|CFE_UNDERLINE|CFE_LINK);
  ok=FormatBackground(receipt,control,"first paragraph break",firstEnd-1,firstEnd,background,true)&&ok;
  ok=FormatBackground(receipt,control,"empty record paragraph",firstEnd,blankEnd,background,true)&&ok;
  control.SetSel(0,0);control.LineScroll(-100000);control.UpdateWindow();
  const POINT first=CharacterPosition(control.m_hWnd,0),ansi=CharacterPosition(control.m_hWnd,20),blank=CharacterPosition(control.m_hWnd,firstEnd);
  NativePixels pixels(control.m_hWnd);
  ok=pixels.Patch(receipt,"first row trailing area",pixels.width-36,first.y+3,background)&&ok;
  ok=pixels.Patch(receipt,"blank row",pixels.width/2,blank.y+3,background)&&ok;
  ok=pixels.Patch(receipt,"unused viewport",pixels.width/2,pixels.height-20,background)&&ok;
  ok=pixels.Patch(receipt,"colored ANSI spaces",ansi.x+2,ansi.y+3,RGB(180,35,55))&&ok;
  std::string path=std::string("WinRel/workspace-background-")+phase+".bmp";pixels.Save(path.c_str());
  receipt<<phase<<" native character formats and rendered pixels "<<(ok?"PASS":"FAIL")<<"\n";return ok;
}

bool Scenario(std::ofstream& receipt,BackgroundPane& pane,COLORREF background,const char* phase)
{
  bool ok=pane.ClearTranscript("background-fixture","Output");
  WorkspaceRecord record;record.connectionId="background-fixture";record.sequence=1;SourceBackground(record,background);
  WorkspaceRun run;run.text=L"Ordinary text row: ";run.foreground=RGB(10,10,10);run.background=background;record.runs.push_back(run);
  run.text=L"            ";run.background=RGB(180,35,55);run.bold=run.italic=run.underline=true;
  run.action="look fictional-red-area";run.actionType=0x0400;record.runs.push_back(run);
  ok=pane.AppendRecord("Output",record)&&ok;
  record.sequence=2;record.runs.clear();ok=pane.AppendRecord("Output",record)&&ok;
  record.sequence=3;run.text=L"Another ordinary row";run.background=background;run.bold=run.italic=run.underline=false;
  run.action.Empty();run.actionType=0;record.runs.push_back(run);ok=pane.AppendRecord("Output",record)&&ok;
  ok=Check(receipt,pane,background,phase)&&ok;
  pane.Replay();std::string replay=std::string(phase)+"-replay";ok=Check(receipt,pane,background,replay.c_str())&&ok;
  return ok;
}

bool IntentionalBleed(std::ofstream& receipt,BackgroundPane& pane)
{
  bool ok=pane.ClearTranscript("background-fixture","Output");
  WorkspaceRecord record;record.connectionId="background-fixture";record.sequence=1;
  SourceBackground(record,RGB(255,255,255));
#ifndef WORKSPACE_BACKGROUND_BASELINE
  record.bleedBackground=true;
#endif
  WorkspaceRun run;run.text=L"Colored background at end";run.background=RGB(180,35,55);record.runs.push_back(run);
  ok=pane.AppendRecord("Output",record)&&ok;
  record.sequence=2;record.runs.clear();ok=pane.AppendRecord("Output",record)&&ok;
  const long end=pane.End(0),blankEnd=pane.End(1);
  ok=FormatBackground(receipt,pane.Control(),"intentional bleed paragraph",end-1,end,RGB(180,35,55),true)&&ok;
  ok=FormatBackground(receipt,pane.Control(),"intentional bleed blank row",end,blankEnd,RGB(180,35,55),true)&&ok;
  receipt<<"configured source bleed retains colored paragraph and empty-row background "<<(ok?"PASS":"FAIL")<<"\n";
  return ok;
}

class BackgroundApp : public CWinApp
{
  int result_=1;
  BOOL InitInstance()
  {
    AfxOleInit();AfxInitRichEdit2();std::ofstream receipt("WinRel/workspace-background-probe.txt");receipt.setf(std::ios::unitbuf);
    WorkspaceTestFrame* frame=new WorkspaceTestFrame;BackgroundPane pane;
    bool ok=frame->Create(NULL,"Fictional background fixture",WS_OVERLAPPEDWINDOW,CRect(40,40,1100,800))!=FALSE;
    frame->EnableWorkspaceDocking();ok=pane.Create(frame)&&ok;pane.EnableDocking(CBRS_ALIGN_ANY);frame->DockPane(&pane);
    ok=pane.OpenTranscript("background-fixture","Output")&&ok;
    frame->RecalcLayout();pane.ShowPane(TRUE,FALSE,FALSE);pane.SetWindowPos(NULL,0,0,460,480,SWP_NOMOVE|SWP_NOZORDER);
    if(ok){ok=Scenario(receipt,pane,RGB(255,255,255),"white")&&ok;
      ok=Scenario(receipt,pane,RGB(8,14,22),"dark")&&ok;
      ok=Scenario(receipt,pane,RGB(240,229,201),"custom")&&ok;
      ok=IntentionalBleed(receipt,pane)&&ok;}
    receipt<<(ok?"PASS":"FAIL")<<" fictional native transcript background regression\n";
    frame->DestroyWindow();result_=ok?0:1;return FALSE;
  }
  int ExitInstance(){return result_;}
};
BackgroundApp app;
