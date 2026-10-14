#include "stdafx.h"
#include "WorkspacePane.h"
#include "pcre/pcre.h"
#include <richole.h>

// RichEdit requests independent structured storage for each imported picture.
class WorkspaceOleCallback : public IRichEditOleCallback
{
  LONG refs;
public:
  WorkspaceOleCallback():refs(1){}
  STDMETHOD(QueryInterface)(REFIID iid,void** result)
    { if(!result)return E_POINTER;*result=NULL;
      if(iid==IID_IUnknown || iid==IID_IRichEditOleCallback) { *result=static_cast<IRichEditOleCallback*>(this);AddRef();return S_OK; }
      return E_NOINTERFACE; }
  STDMETHOD_(ULONG,AddRef)(){return InterlockedIncrement(&refs);}
  STDMETHOD_(ULONG,Release)(){ULONG remaining=InterlockedDecrement(&refs);if(!remaining)delete this;return remaining;}
  STDMETHOD(GetNewStorage)(LPSTORAGE* storage)
    { if(!storage)return E_POINTER;*storage=NULL;ILockBytes* bytes=NULL;
      HRESULT status=CreateILockBytesOnHGlobal(NULL,TRUE,&bytes);
      if(SUCCEEDED(status)) { status=StgCreateDocfileOnILockBytes(bytes,STGM_CREATE|STGM_READWRITE|STGM_SHARE_EXCLUSIVE,0,storage);bytes->Release(); }
      return status; }
  STDMETHOD(GetInPlaceContext)(LPOLEINPLACEFRAME*,LPOLEINPLACEUIWINDOW*,LPOLEINPLACEFRAMEINFO){return E_NOTIMPL;}
  STDMETHOD(ShowContainerUI)(BOOL){return S_OK;}
  STDMETHOD(QueryInsertObject)(LPCLSID,LPSTORAGE,LONG){return S_OK;}
  STDMETHOD(DeleteObject)(LPOLEOBJECT){return S_OK;}
  STDMETHOD(QueryAcceptData)(LPDATAOBJECT,CLIPFORMAT*,DWORD,BOOL,HGLOBAL){return S_OK;}
  STDMETHOD(ContextSensitiveHelp)(BOOL){return E_NOTIMPL;}
  STDMETHOD(GetClipboardData)(CHARRANGE*,DWORD,LPDATAOBJECT*){return E_NOTIMPL;}
  STDMETHOD(GetDragDropEffect)(BOOL,DWORD,LPDWORD effect){if(effect)*effect=DROPEFFECT_NONE;return S_OK;}
  STDMETHOD(GetContextMenu)(WORD,LPOLEOBJECT,CHARRANGE*,HMENU* menu){if(menu)*menu=NULL;return E_NOTIMPL;}
};

namespace {
const wchar_t* WorkspaceDockLayoutProperty=L"MUSHclientWorkspaceDockLayout";
LRESULT CALLBACK WorkspaceDockLayoutHook(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR)
{
  if(message==WM_SIZEPARENT && lp)
    {const AFX_SIZEPARENTPARAMS* parent=(const AFX_SIZEPARENTPARAMS*)lp;
     const CRect available(parent->rect);HANDLE previous=GetPropW(window,WorkspaceDockLayoutProperty);
     SetPropW(window,WorkspaceDockLayoutProperty,(HANDLE)&available);
     LRESULT result=DefSubclassProc(window,message,wp,lp);
     if(previous)SetPropW(window,WorkspaceDockLayoutProperty,previous);else RemovePropW(window,WorkspaceDockLayoutProperty);
     return result;}
  if(message==WM_NCDESTROY){RemovePropW(window,WorkspaceDockLayoutProperty);RemoveWindowSubclass(window,WorkspaceDockLayoutHook,id);}
  return DefSubclassProc(window,message,wp,lp);
}
void WorkspaceHookDockBars(CFrameWnd* frame)
{
  if(!frame)return;
  const UINT bars[]={AFX_IDW_DOCKBAR_TOP,AFX_IDW_DOCKBAR_BOTTOM,AFX_IDW_DOCKBAR_LEFT,AFX_IDW_DOCKBAR_RIGHT};
  for(size_t i=0;i<4;++i){CControlBar* bar=frame->GetControlBar(bars[i]);if(bar && bar->GetSafeHwnd())SetWindowSubclass(bar->m_hWnd,WorkspaceDockLayoutHook,1,0);}
}
}

IMPLEMENT_DYNAMIC(CWorkspacePane,CControlBar)

BEGIN_MESSAGE_MAP(CWorkspacePane, CControlBar)
  ON_WM_CREATE()
  ON_WM_TIMER()
  ON_BN_CLICKED(3, OnReviewInput)
  ON_BN_CLICKED(5, OnFindNext)
  ON_BN_CLICKED(6, OnPause)
  ON_NOTIFY_RANGE(EN_LINK, 100, 131, OnLink)
  ON_WM_SIZE()
  ON_WM_PAINT()
  ON_WM_LBUTTONDOWN()
  ON_WM_LBUTTONUP()
  ON_WM_MOUSEMOVE()
  ON_WM_CAPTURECHANGED()
  ON_WM_LBUTTONDBLCLK()
  ON_NOTIFY(TCN_SELCHANGE, 1, OnTabChanged)
END_MESSAGE_MAP()

CWorkspacePane::CWorkspacePane() : m_floatSize(500,350), m_dockWidth(360),
  m_dockHeight(250), m_resizing(false), m_resizeOriginal(0), m_toolWindow(NULL), m_selectedInput(-1) {}
CWorkspacePane::~CWorkspacePane()
{
  for (size_t i=0; i<m_transcripts.size(); ++i) delete m_transcripts[i];
}
BOOL CWorkspacePane::Create(CFrameWnd* frame,UINT barId)
{
  WorkspaceHookDockBars(frame);
  m_dwStyle = CBRS_RIGHT | CBRS_SIZE_DYNAMIC;
  return CWnd::Create(AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(NULL, IDC_ARROW),
    (HBRUSH)(COLOR_BTNFACE+1)), "Transcripts", WS_CHILD | WS_CLIPSIBLINGS |
    WS_CLIPCHILDREN | CBRS_RIGHT, CRect(0,0,360,350), frame, barId);
}
int CWorkspacePane::OnCreate(LPCREATESTRUCT cs)
{
  if (CControlBar::OnCreate(cs)==-1) return -1;
  if (!m_tabs.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP, CRect(0,0,1,1), this, 1)) return -1;
  m_tabs.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
  HWND input=::CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
    0,0,1,1,m_hWnd,(HMENU)2,AfxGetInstanceHandle(),NULL);
  if(!input || !m_input.Attach(input)) return -1;
  ::SendMessageW(input,EM_SETLIMITTEXT,16384,0);
  ::SendMessageW(input,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
  if(!m_reviewInput.Create("Review command",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,CRect(0,0,1,1),this,3)) return -1;
  m_reviewInput.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
  HWND search=::CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
    0,0,1,1,m_hWnd,(HMENU)4,AfxGetInstanceHandle(),NULL);
  if(!search || !m_search.Attach(search))return -1;
  ::SendMessageW(search,EM_SETLIMITTEXT,1024,0);::SendMessageW(search,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
  if(!m_findNext.Create("Find next",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,CRect(0,0,1,1),this,5) ||
     !m_pause.Create("Pause",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,CRect(0,0,1,1),this,6))return -1;
  m_findNext.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
  m_pause.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
  SetTimer(1,500,NULL);
  return 0;
}
CSize CWorkspacePane::CalcFixedLayout(BOOL stretch, BOOL horizontal)
{
  return CalcDynamicLayout(-1, horizontal ? LM_HORZDOCK|LM_HORZ : LM_VERTDOCK);
}
CSize CWorkspacePane::CalcDynamicLayout(int length, DWORD mode)
{
  const CSize minimum=MinimumSize();
  m_floatSize.cx=max(minimum.cx,m_floatSize.cx);m_floatSize.cy=max(minimum.cy,m_floatSize.cy);
  m_dockWidth=max(minimum.cx,m_dockWidth);m_dockHeight=max(minimum.cy,m_dockHeight);
  if(mode&(LM_HORZDOCK|LM_VERTDOCK))
    {WorkspaceHookDockBars(m_pDockSite);const CSize available=DockAvailableSize();
     const bool horizontal=(mode&LM_HORZDOCK)!=0;
     int preferred=horizontal?m_dockHeight:m_dockWidth,total=0,peers=0;
     if(m_pDockBar && !IsFloating())
       for(int i=0;i<m_pDockBar->m_arrBars.GetSize();++i)
         {CControlBar* bar=(CControlBar*)m_pDockBar->m_arrBars.GetAt(i);
          if((UINT_PTR)bar<=0xFFFF || !(bar->GetStyle()&WS_VISIBLE))continue;
          CWorkspacePane* other=DYNAMIC_DOWNCAST(CWorkspacePane,bar);
          if(other){const CSize peerMinimum=other->MinimumSize();total+=horizontal?max(peerMinimum.cy,other->m_dockHeight):max(peerMinimum.cx,other->m_dockWidth);++peers;}}
     const int bound=horizontal?available.cy:available.cx;
     if(total>bound && peers>0)preferred=max(1,(int)((double)preferred*max(1,bound-peers*4)/total));
     return horizontal?CSize(available.cx,min(preferred,available.cy)):CSize(min(preferred,available.cx),available.cy);
    }
  if (length > 0 && !(mode & LM_MRUWIDTH))
    {
    if (mode & LM_LENGTHY) m_floatSize.cy = max(minimum.cy,length);
    else m_floatSize.cx = max(minimum.cx,length);
    }
  return m_floatSize;
}
CWorkspacePane::Transcript* CWorkspacePane::Find(const CString& id, const CString& name)
{
  for (size_t i=0; i<m_transcripts.size(); ++i)
    if (m_transcripts[i]->connectionId==id && m_transcripts[i]->name==name) return m_transcripts[i];
  return NULL;
}
bool CWorkspacePane::OpenTranscript(const CString& id, const CString& name)
{
  if (id.IsEmpty() || name.IsEmpty()) return false;
  SaveInputDraft();
  Transcript* transcript=Find(id,name);
  if (!transcript)
    {
    if (m_transcripts.size()>=32) return false;
    transcript=new Transcript;
    transcript->connectionId=id; transcript->name=name;
    static HMODULE richEditLibrary=LoadLibraryW(L"Msftedit.dll");
    if (!richEditLibrary) { delete transcript; return false; }
    UINT controlId=100;
    for (;controlId<132;++controlId)
      { bool used=false; for (size_t k=0;k<m_transcripts.size();++k)
          if (m_transcripts[k]->text.GetDlgCtrlID()==controlId) used=true;
        if (!used) break; }
    HWND textWindow=::CreateWindowExW(0,L"RICHEDIT50W",L"",
      WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | ES_MULTILINE |
      ES_READONLY | ES_AUTOVSCROLL | ES_NOHIDESEL, 0,0,1,1,m_hWnd,
      (HMENU)(UINT_PTR)controlId,AfxGetInstanceHandle(),NULL);
    if (!textWindow || !transcript->text.Attach(textWindow))
      { if (textWindow) ::DestroyWindow(textWindow); delete transcript; return false; }
    WorkspaceOleCallback* oleCallback=new WorkspaceOleCallback;
    ::SendMessageW(textWindow,EM_SETOLECALLBACK,0,(LPARAM)oleCallback);oleCallback->Release();
    transcript->text.SetBackgroundColor(FALSE,RGB(25,25,28));
    transcript->text.LimitText(4*1024*1024);
    transcript->text.SetEventMask(transcript->text.GetEventMask()|ENM_LINK);
    CHARFORMAT2W format; memset(&format,0,sizeof(format)); format.cbSize=sizeof(format);
    format.dwMask=CFM_FACE|CFM_SIZE; format.yHeight=200;
    wcscpy_s(format.szFaceName, L"Consolas");
    ::SendMessageW(textWindow,EM_SETCHARFORMAT,0,(LPARAM)&format);
    CString label=name+" ["+id.Left(8)+"]";
    m_tabs.InsertItem((int)m_transcripts.size(),label);
    m_transcripts.push_back(transcript);
    }
  for (size_t i=0; i<m_transcripts.size(); ++i)
    if (m_transcripts[i]==transcript) m_tabs.SetCurSel((int)i);
  if(!transcript->paused){transcript->unread=0;for(size_t i=0;i<m_transcripts.size();++i)if(m_transcripts[i]==transcript)UpdateTabLabel(i);}
  LayoutChildren();
  return true;
}
void CWorkspacePane::RenderRecord(Transcript& transcript, const WorkspaceRecord& record)
{
  CRichEditCtrl& control=transcript.text;
  control.SetReadOnly(FALSE);
  for (size_t i=0; i<record.runs.size(); ++i)
    {
    const WorkspaceRun& run=record.runs[i];
    CHARFORMAT2W format; memset(&format,0,sizeof(format)); format.cbSize=sizeof(format);
    format.dwMask=CFM_COLOR|CFM_BACKCOLOR|CFM_BOLD|CFM_ITALIC|CFM_UNDERLINE|CFM_LINK;
    format.crTextColor=run.foreground; format.crBackColor=run.background;
    format.dwEffects=(run.bold?CFE_BOLD:0)|(run.italic?CFE_ITALIC:0)|(run.underline?CFE_UNDERLINE:0)|(!run.action.IsEmpty()?CFE_LINK:0);
    const long first=TextLength(control);
    control.SetSel(-1,-1);
    ::SendMessageW(control.m_hWnd,EM_REPLACESEL,FALSE,(LPARAM)run.text.c_str());
    const long last=TextLength(control);
    control.SetSel(first,last);
    ::SendMessageW(control.m_hWnd,EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    if (!run.action.IsEmpty())
      { LinkRange link; link.first=first; link.last=TextLength(control);
        link.type=run.actionType; link.action=run.action; transcript.links.push_back(link); }
    }
  if (record.image) RenderImage(transcript,record);
  transcript.imageStates.push_back(record.image?(int)record.image->GetState():-1);
  control.SetSel(-1,-1); ::SendMessageW(control.m_hWnd,EM_REPLACESEL,FALSE,(LPARAM)L"\r\n");
  control.SetReadOnly(TRUE);
}
bool CWorkspacePane::AppendRecord(const CString& name, const WorkspaceRecord& record)
{
  Transcript* transcript=Find(record.connectionId,name);
  if (!transcript) return false; // output never silently creates/focuses panes
  if (!transcript->records.empty() && record.sequence &&
      transcript->records.back().sequence>=record.sequence) return false;
  long start,end; transcript->text.GetSel(start,end);
  const int first=transcript->text.GetFirstVisibleLine();
  const ViewState view=SaveView(transcript->text);
  const bool follow=view.follow;
  size_t tabIndex=0;for(;tabIndex<m_transcripts.size();++tabIndex)if(m_transcripts[tabIndex]==transcript)break;
  const bool visible=IsWindowVisible()!=FALSE && m_tabs.GetCurSel()==(int)tabIndex && !transcript->paused && follow;
  if(!visible) { transcript->unread=min(9999U,transcript->unread+1);UpdateTabLabel(tabIndex); }
  transcript->text.SetRedraw(FALSE);
  size_t incoming=0; for(size_t i=0;i<record.runs.size();++i) incoming+=record.runs[i].text.size();
  if (incoming>65536) { transcript->text.SetRedraw(TRUE); return false; }
  transcript->records.push_back(record);
  size_t retainedBytes=0;
  for(size_t i=0;i<transcript->records.size();++i)
    { const WorkspaceRecord& item=transcript->records[i];
      for(size_t j=0;j<item.runs.size();++j) retainedBytes+=item.runs[j].text.size()*sizeof(wchar_t);
      if(item.image) retainedBytes+=512*384*4; }
  bool rebuilt=false;
  if (transcript->records.size()>2000 || TextLength(transcript->text)>3*1024*1024 || retainedBytes>8*1024*1024)
    {
    while (transcript->records.size()>1000 || (retainedBytes>4*1024*1024 && transcript->records.size()>1))
      { const WorkspaceRecord& item=transcript->records.front();
        for(size_t j=0;j<item.runs.size();++j) retainedBytes-=item.runs[j].text.size()*sizeof(wchar_t);
        if(item.image) retainedBytes-=512*384*4;
        transcript->records.pop_front(); }
    if(!transcript->paused)Rebuild(*transcript);
    if(name!="Output" && m_pruneHandler)m_pruneHandler(record.connectionId,name,transcript->records);
    rebuilt=true;
    }
  else if(!transcript->paused)RenderRecord(*transcript,record);
  if (follow) transcript->text.SendMessage(EM_SCROLLCARET);
  else if (!rebuilt)
    {
    RestoreView(transcript->text,view);
    }
  transcript->text.SetRedraw(TRUE); transcript->text.Invalidate();
  return true;
}
void CWorkspacePane::CloseConnection(const CString& id)
{
  SaveInputDraft();
  for (size_t i=m_transcripts.size(); i>0; --i)
    if (m_transcripts[i-1]->connectionId==id)
      {
      m_tabs.DeleteItem((int)i-1);
      m_transcripts[i-1]->text.DestroyWindow(); delete m_transcripts[i-1];
      m_transcripts.erase(m_transcripts.begin()+i-1);
      }
  m_selectedInput=-1;
  if (!m_transcripts.empty() && m_tabs.GetCurSel()<0) m_tabs.SetCurSel(0);
  LayoutChildren();
}
void CWorkspacePane::LayoutChildren()
{
  if (!m_tabs.GetSafeHwnd()) return;
  if (m_toolWindow && ::IsWindow(m_toolWindow))
    { CRect bounds; GetClientRect(bounds); bounds.DeflateRect(6,24,6,6);
      ::MoveWindow(m_toolWindow,bounds.left,bounds.top,max(1,bounds.Width()),max(1,bounds.Height()),TRUE); return; }
  CRect rect; GetClientRect(rect); rect.DeflateRect(6,24,6,6);
  CRect inputRect=rect; inputRect.top=max(inputRect.top,inputRect.bottom-25);
  m_reviewInput.MoveWindow(max(inputRect.left,inputRect.right-115),inputRect.top,115,25);
  m_input.MoveWindow(inputRect.left,inputRect.top,max(1,inputRect.Width()-120),25);
  rect.bottom=max(rect.top,rect.bottom-31);
  CRect searchRect=rect;searchRect.bottom=searchRect.top+24;
  m_search.MoveWindow(searchRect.left,searchRect.top,max(1,searchRect.Width()-155),24);
  m_findNext.MoveWindow(max(searchRect.left,searchRect.right-150),searchRect.top,85,24);
  m_pause.MoveWindow(max(searchRect.left,searchRect.right-60),searchRect.top,60,24);
  rect.top=min(rect.bottom,rect.top+28);
  const int selected=m_tabs.GetCurSel();
  m_pause.SetCheck(selected>=0 && selected<(int)m_transcripts.size() && m_transcripts[selected]->paused?BST_CHECKED:BST_UNCHECKED);
  m_pause.EnableWindow(selected>=0);m_findNext.EnableWindow(selected>=0);
  if(selected!=m_selectedInput)
    { m_selectedInput=selected;
      const wchar_t* draft=selected>=0 && selected<(int)m_transcripts.size()?m_transcripts[selected]->inputDraft.c_str():L"";
      ::SetWindowTextW(m_input.m_hWnd,draft); }
  m_input.EnableWindow(selected>=0);m_reviewInput.EnableWindow(selected>=0);
  m_tabs.MoveWindow(rect);
  CRect body=rect; m_tabs.AdjustRect(FALSE,body);
  for (size_t i=0; i<m_transcripts.size(); ++i)
    {
    Transcript& transcript=*m_transcripts[i];
    const ViewState view=SaveView(transcript.text);
    transcript.text.MoveWindow(body);
    CRect viewport;transcript.text.GetClientRect(viewport);
    bool images=false;for(size_t r=0;r<transcript.records.size();++r)if(transcript.records[r].image){images=true;break;}
    if(images && !transcript.paused && viewport.Width()!=transcript.renderedWidth)
      {transcript.text.SetRedraw(FALSE);Rebuild(transcript);RestoreView(transcript.text,view);transcript.text.SetRedraw(TRUE);transcript.text.Invalidate();}
    transcript.renderedWidth=viewport.Width();
    m_transcripts[i]->text.ShowWindow((int)i==m_tabs.GetCurSel()?SW_SHOW:SW_HIDE);
    }
}
void CWorkspacePane::OnSize(UINT type,int cx,int cy)
{
  CControlBar::OnSize(type,cx,cy); LayoutChildren();
}
void CWorkspacePane::OnPaint()
{
  CPaintDC dc(this); CRect rect; GetClientRect(rect);
  dc.FillSolidRect(rect,GetSysColor(COLOR_BTNFACE));
  rect.DeflateRect(8,3); rect.bottom=22;
  dc.SetBkMode(TRANSPARENT); dc.DrawText(m_toolTitle.IsEmpty()?"Transcripts - drag title to dock or float":m_toolTitle+" - drag title to dock or float",rect,DT_SINGLELINE|DT_END_ELLIPSIS);
}
void CWorkspacePane::OnLButtonDown(UINT flags,CPoint point)
{
  const bool horizontal=(m_dwStyle&CBRS_ORIENT_HORZ)!=0;
  CRect rect; GetClientRect(rect);
  const DWORD alignment=m_dwStyle&CBRS_ALIGN_ANY;
  bool edge=horizontal ? (alignment==CBRS_ALIGN_BOTTOM?point.y<6:point.y>=rect.bottom-6) :
    (alignment==CBRS_ALIGN_LEFT?point.x>=rect.right-6:point.x<6);
  if (!IsFloating() && edge)
    {
    m_resizing=true; m_resizeStart=point;
    ClientToScreen(&m_resizeStart);
    m_resizeOriginal=horizontal?m_dockHeight:m_dockWidth;
    SetCapture(); return;
    }
  if (point.y<24 && m_pDockContext) { ClientToScreen(&point); m_pDockContext->StartDrag(point); return; }
  CControlBar::OnLButtonDown(flags,point);
}
void CWorkspacePane::OnMouseMove(UINT flags,CPoint point)
{
  if (m_resizing)
    {
    ClientToScreen(&point);
    const CSize minimum=MinimumSize();
    if (m_dwStyle&CBRS_ORIENT_HORZ)
      { const int delta=point.y-m_resizeStart.y;
        m_dockHeight=max(minimum.cy,min(1000,m_resizeOriginal+((m_dwStyle&CBRS_ALIGN_BOTTOM)?-delta:delta))); }
    else
      { const int delta=point.x-m_resizeStart.x;
        m_dockWidth=max(minimum.cx,min(1500,m_resizeOriginal+((m_dwStyle&CBRS_ALIGN_LEFT)?delta:-delta))); }
    GetDockingFrame()->RecalcLayout(); return;
    }
  CControlBar::OnMouseMove(flags,point);
}
void CWorkspacePane::OnLButtonUp(UINT flags,CPoint point)
{
  if (m_resizing) { m_resizing=false; ReleaseCapture(); return; }
  CControlBar::OnLButtonUp(flags,point);
}
void CWorkspacePane::OnCaptureChanged(CWnd* window)
{
  m_resizing=false; CControlBar::OnCaptureChanged(window);
}
void CWorkspacePane::OnLButtonDblClk(UINT flags,CPoint point)
{
  if (point.y<24 && m_pDockContext) { m_pDockContext->ToggleDocking(); return; }
  CControlBar::OnLButtonDblClk(flags,point);
}
void CWorkspacePane::OnTabChanged(NMHDR*,LRESULT* result)
{
  SaveInputDraft();
  const int selected=m_tabs.GetCurSel();
  if(selected>=0 && selected<(int)m_transcripts.size() && !m_transcripts[selected]->paused)
    {m_transcripts[selected]->unread=0;UpdateTabLabel(selected);}
  LayoutChildren(); *result=0;
}


namespace {
struct WorkspaceRtfStream { const std::string* data; size_t offset; };
DWORD CALLBACK WorkspaceReadRtf(DWORD_PTR cookie, LPBYTE buffer, LONG count, LONG* read)
{
  WorkspaceRtfStream* stream=(WorkspaceRtfStream*)cookie;
  *read=(LONG)min((size_t)count,stream->data->size()-stream->offset);
  memcpy(buffer,stream->data->data()+stream->offset,*read);
  stream->offset+=*read; return 0;
}
}
void CWorkspacePane::RenderImage(Transcript& transcript, const WorkspaceRecord& record)
{
  CRichEditCtrl& control=transcript.text;
  if (record.image->GetState()!=CInlineImage::Ready)
    {
    const wchar_t* label=record.image->GetState()==CInlineImage::Loading ? L" [image loading]" : L" [image unavailable]";
    control.SetSel(-1,-1); ::SendMessageW(control.m_hWnd,EM_REPLACESEL,FALSE,(LPARAM)label);
    return;
    }
  int width=record.imageWidth>0?record.imageWidth:record.image->GetWidth();
  int height=record.imageHeight>0?record.imageHeight:record.image->GetHeight();
  if (record.imageWidth>0 && record.imageHeight==0)
    height=max(1,(int)((double)width*record.image->GetHeight()/max(1,record.image->GetWidth())));
  if (record.imageHeight>0 && record.imageWidth==0)
    width=max(1,(int)((double)height*record.image->GetWidth()/max(1,record.image->GetHeight())));
  CRect viewport;control.GetClientRect(viewport);
  const int maxWidth=max(1,min(512,viewport.Width()-12));
  double scale=min(1.0,min((double)maxWidth/max(1,width),384.0/max(1,height)));
  width=max(1,(int)(width*scale)); height=max(1,(int)(height*scale));
  BITMAPINFO info; memset(&info,0,sizeof(info));
  info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=height;
  info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
  info.bmiHeader.biCompression=BI_RGB; info.bmiHeader.biSizeImage=width*height*4;
  void* pixels=NULL;
  HBITMAP bitmap=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,&pixels,NULL,0);
  if (!bitmap || !pixels) return;
  CDC dc; dc.CreateCompatibleDC(NULL);
  HGDIOBJ old=::SelectObject(dc.m_hDC,bitmap);
  dc.FillSolidRect(0,0,width,height,RGB(25,25,28));
  record.image->Draw(&dc,CRect(0,0,width,height));
  ::SelectObject(dc.m_hDC,old);
  // RichEdit imports its own static image object. No document/view/GDI pointers
  // survive insertion, and image fetches remain in the existing bounded cache.
  RECT metricBounds={0,0,width*2540/96,height*2540/96};
  HDC metafileDc=CreateEnhMetaFileW(NULL,NULL,&metricBounds,L"MUSHclient\0Workspace image\0\0");
  if(!metafileDc) { DeleteObject(bitmap);return; }
  StretchDIBits(metafileDc,0,0,width,height,0,0,width,height,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
  HENHMETAFILE metafile=CloseEnhMetaFile(metafileDc);
  UINT byteCount=GetEnhMetaFileBits(metafile,0,NULL);
  std::vector<BYTE> picture(byteCount);
  if(byteCount) GetEnhMetaFileBits(metafile,byteCount,&picture[0]);
  DeleteEnhMetaFile(metafile);
  std::ostringstream prefix;
  prefix << "{\\rtf1\\ansi{\\pict\\emfblip\\picw" << metricBounds.right << "\\pich" << metricBounds.bottom
         << "\\picwgoal" << width*15 << "\\pichgoal" << height*15 << " ";
  std::string rtf=prefix.str();
  const char hex[]="0123456789abcdef";
  for(size_t i=0;i<picture.size();++i)
    { rtf+=hex[picture[i]>>4];rtf+=hex[picture[i]&15]; }
  rtf+="}}";
  DeleteObject(bitmap);
  control.SetSel(-1,-1);
  const long first=TextLength(control);
  WorkspaceRtfStream data={&rtf,0};
  EDITSTREAM stream={0}; stream.dwCookie=(DWORD_PTR)&data; stream.pfnCallback=WorkspaceReadRtf;
  control.StreamIn(SF_RTF|SFF_SELECTION,stream);
  if (!record.imageAction.IsEmpty())
    {
    const long last=TextLength(control);
    control.SetSel(first,last);
    CHARFORMAT2W format; memset(&format,0,sizeof(format)); format.cbSize=sizeof(format);
    format.dwMask=CFM_LINK; format.dwEffects=CFE_LINK;
    ::SendMessageW(control.m_hWnd,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);
    LinkRange link; link.first=first; link.last=last; link.type=0x0800;
    link.action=record.imageAction; transcript.links.push_back(link);
    }
}
void CWorkspacePane::Rebuild(Transcript& transcript)
{
  CRect viewport;transcript.text.GetClientRect(viewport);transcript.renderedWidth=viewport.Width();
  transcript.text.SetWindowText(""); transcript.links.clear(); transcript.imageStates.clear();
  for (size_t i=0;i<transcript.records.size();++i) RenderRecord(transcript,transcript.records[i]);
}
void CWorkspacePane::OnTimer(UINT_PTR timer)
{
  if (timer!=1) { CControlBar::OnTimer(timer); return; }
  for (size_t i=0;i<m_transcripts.size();++i)
    {
    Transcript& transcript=*m_transcripts[i];
    if(transcript.paused)continue;
    for (size_t j=0;j<transcript.records.size() && j<transcript.imageStates.size();++j)
      if (transcript.records[j].image && transcript.imageStates[j]!=(int)transcript.records[j].image->GetState())
        {
        long start,end; transcript.text.GetSel(start,end);
        const int first=transcript.text.GetFirstVisibleLine();
        const ViewState view=SaveView(transcript.text);
        const bool follow=view.follow;
        transcript.text.SetRedraw(FALSE); Rebuild(transcript);
        if (follow) transcript.text.SendMessage(EM_SCROLLCARET);
        else RestoreView(transcript.text,view);
        transcript.text.SetRedraw(TRUE); transcript.text.Invalidate(); break;
        }
    }
}
void CWorkspacePane::OnLink(UINT id,NMHDR* hdr,LRESULT* result)
{
  *result=0;
  ENLINK* link=(ENLINK*)hdr;
  if (link->msg!=WM_LBUTTONUP || !m_actionHandler) return;
  for (size_t i=0;i<m_transcripts.size();++i)
    if (m_transcripts[i]->text.GetDlgCtrlID()==id)
      {
      Transcript& transcript=*m_transcripts[i];
      for (size_t j=0;j<transcript.links.size();++j)
        if (link->chrg.cpMin>=transcript.links[j].first && link->chrg.cpMin<transcript.links[j].last)
          {
          const CString connectionId=transcript.connectionId;
          const LinkRange action=transcript.links[j];
          m_actionHandler(connectionId,action.type,action.action);
          *result=1; return;
          }
      }
}




bool CWorkspacePane::MoveSelectedTo(CWorkspacePane& target)
{
  const int selected=m_tabs.GetCurSel();
  if (selected<0 || selected>=(int)m_transcripts.size() || &target==this) return false;
  SaveInputDraft();
  Transcript* original=m_transcripts[selected];
  if (!target.OpenTranscript(original->connectionId,original->name)) return false;
  Transcript* copied=target.Find(original->connectionId,original->name);
  copied->records=original->records; copied->inputDraft=original->inputDraft;copied->inputPrefix=original->inputPrefix;copied->unread=original->unread;
  for(size_t i=0;i<target.m_transcripts.size();++i)if(target.m_transcripts[i]==copied)target.UpdateTabLabel(i);
  target.m_selectedInput=-1;target.LayoutChildren();target.Rebuild(*copied);
  m_tabs.DeleteItem(selected); original->text.DestroyWindow(); delete original;
  m_transcripts.erase(m_transcripts.begin()+selected);m_selectedInput=-1;
  if (!m_transcripts.empty()) m_tabs.SetCurSel(min(selected,(int)m_transcripts.size()-1));
  LayoutChildren(); return true;
}

bool CWorkspacePane::HostTool(HWND toolWindow,const CString& title)
{
  if (!::IsWindow(toolWindow) || !m_transcripts.empty()) return false;
  m_toolWindow=toolWindow; m_toolTitle=title; SetWindowText(title);
  LONG_PTR style=::GetWindowLongPtr(toolWindow,GWL_STYLE);
  style &= ~(WS_POPUP|WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
  style |= WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN;
  ::SetWindowLongPtr(toolWindow,GWL_STYLE,style);
  ::SetParent(toolWindow,m_hWnd);
  ::SetWindowPos(toolWindow,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
  const CSize minimum=MinimumSize();m_floatSize.cx=max(minimum.cx,m_floatSize.cx);m_floatSize.cy=max(minimum.cy,m_floatSize.cy);
  m_dockWidth=max(minimum.cx,m_dockWidth);m_dockHeight=max(minimum.cy,m_dockHeight);
  m_tabs.ShowWindow(SW_HIDE);m_input.ShowWindow(SW_HIDE);m_reviewInput.ShowWindow(SW_HIDE);m_search.ShowWindow(SW_HIDE);m_findNext.ShowWindow(SW_HIDE);m_pause.ShowWindow(SW_HIDE); LayoutChildren(); Invalidate(); return true;
}

namespace {
std::string WorkspaceUtf8(const std::wstring& text)
{
  if(text.empty()) return std::string();
  int bytes=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),NULL,0,NULL,NULL);
  std::string result(bytes,'\0');
  WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),&result[0],bytes,NULL,NULL);
  return result;
}
}
bool WorkspaceRoute::Compile(CString& error)
{
  error.Empty(); compiled.reset();
  if(transcriptName.CompareNoCase("Output")==0)
    {error="Output is reserved for the main transcript. Choose another name for this route.";return false;}
  if(connectionId.IsEmpty() || transcriptName.IsEmpty() || pattern.empty() || pattern.size()>1024)
    { error="Choose a connection, transcript name, and a pattern of 1 to 1024 characters."; return false; }
  if(!regex) return true;
  const char* message=NULL;int offset=0;
  std::string utf8=WorkspaceUtf8(pattern);
  pcre* expression=pcre_compile(utf8.c_str(),PCRE_UTF8,&message,&offset,NULL);
  if(!expression) { error=message?message:"Invalid regular expression.";return false; }
  compiled=std::shared_ptr<void>(expression,[](void* ptr){pcre_free(ptr);});return true;
}
bool WorkspaceRoute::Matches(const WorkspaceRecord& record) const
{
  if(record.connectionId!=connectionId) return false;
  std::wstring text;
  for(size_t i=0;i<record.runs.size();++i)
    { if(text.size()+record.runs[i].text.size()>65536) return false; text+=record.runs[i].text; }
  if(!regex) return text.find(pattern)!=std::wstring::npos;
  if(!compiled) return false;
  const std::string utf8=WorkspaceUtf8(text);
  pcre_extra limits;memset(&limits,0,sizeof(limits));
  limits.flags=PCRE_EXTRA_MATCH_LIMIT|PCRE_EXTRA_MATCH_LIMIT_RECURSION;
  limits.match_limit=100000;limits.match_limit_recursion=1000;
  int offsets[3]={0};
  return pcre_exec((const pcre*)compiled.get(),&limits,utf8.data(),(int)utf8.size(),0,0,offsets,3)>=0;
}

void CWorkspacePane::SaveInputDraft()
{
  if(m_selectedInput<0 || m_selectedInput>=(int)m_transcripts.size() || !m_input.GetSafeHwnd()) return;
  const int count=::GetWindowTextLengthW(m_input.m_hWnd);
  std::wstring text(min(count,16384)+1,L'\0');
  ::GetWindowTextW(m_input.m_hWnd,&text[0],(int)text.size());text.resize(wcslen(text.c_str()));
  m_transcripts[m_selectedInput]->inputDraft=text;
}
bool CWorkspacePane::SetInputPrefix(const CString& id,const CString& name,const std::wstring& prefix)
{
  Transcript* transcript=Find(id,name);
  if(!transcript || prefix.size()>1024) return false;
  transcript->inputPrefix=prefix;return true;
}
BOOL CWorkspacePane::PreTranslateMessage(MSG* message)
{
  if(message->message==WM_KEYDOWN && message->wParam=='F' && (GetKeyState(VK_CONTROL)&0x8000))
    { m_search.SetFocus();return TRUE; }
  if(message->hwnd==m_search.m_hWnd && message->message==WM_KEYDOWN && message->wParam==VK_RETURN)
    { OnFindNext();return TRUE; }
  if(message->hwnd==m_input.m_hWnd && message->message==WM_KEYDOWN && message->wParam==VK_RETURN)
    { OnReviewInput();return TRUE; }
  return CControlBar::PreTranslateMessage(message);
}
void CWorkspacePane::OnReviewInput()
{
  SaveInputDraft(); const int selected=m_tabs.GetCurSel();
  if(selected<0 || selected>=(int)m_transcripts.size() || !m_inputHandler) return;
  const Transcript& transcript=*m_transcripts[selected];
  if(transcript.inputDraft.empty()) return;
  const CString identity=transcript.connectionId;
  const std::wstring command=transcript.inputPrefix+transcript.inputDraft;
  m_inputHandler(identity,command);
}


void CWorkspacePane::GetLayouts(std::vector<WorkspacePaneLayout>& layouts,bool detached) const
{
  CRect bounds;
  if(IsFloating()) GetParentFrame()->GetWindowRect(bounds);else GetWindowRect(bounds);
  for(size_t i=0;i<m_transcripts.size();++i)
    {
    WorkspacePaneLayout layout;layout.connectionId=m_transcripts[i]->connectionId;layout.name=m_transcripts[i]->name;
    layout.detached=detached;layout.floating=IsFloating()!=FALSE;layout.alignment=m_dwStyle&CBRS_ALIGN_ANY;
    layout.x=bounds.left;layout.y=bounds.top;
    layout.width=layout.floating?m_floatSize.cx:m_dockWidth;
    layout.height=layout.floating?m_floatSize.cy:m_dockHeight;
    layouts.push_back(layout);
    }
}
void CWorkspacePane::ApplyLayout(const WorkspacePaneLayout& layout)
{
  const CSize minimum=MinimumSize();
  m_dockWidth=max(minimum.cx,min(1500,layout.width));m_dockHeight=max(minimum.cy,min(1000,layout.height));
  m_floatSize=CSize(m_dockWidth,m_dockHeight);
}

bool CWorkspacePane::HasTranscript(const CString& id,const CString& name) const
{
  for(size_t i=0;i<m_transcripts.size();++i)if(m_transcripts[i]->connectionId==id && m_transcripts[i]->name==name)return true;
  return false;
}

void CWorkspacePane::InstallDockContext(CDockContext* context)
{
  if(m_pDockContext)delete m_pDockContext;m_pDockContext=context;
}

bool CWorkspacePane::ClearTranscript(const CString& id,const CString& name)
{
  Transcript* transcript=Find(id,name);if(!transcript)return false;
  transcript->records.clear();transcript->links.clear();transcript->imageStates.clear();transcript->unread=0;
  for(size_t i=0;i<m_transcripts.size();++i)if(m_transcripts[i]==transcript)UpdateTabLabel(i);
  ::SetWindowTextW(transcript->text.m_hWnd,L"");
  if(name!="Output" && m_pruneHandler)m_pruneHandler(id,name,transcript->records);
  return true;
}

void CWorkspacePane::OnFindNext()
{
  const int selected=m_tabs.GetCurSel();if(selected<0 || selected>=(int)m_transcripts.size())return;
  wchar_t query[1025]={0};::GetWindowTextW(m_search.m_hWnd,query,1025);if(!query[0])return;
  CRichEditCtrl& control=m_transcripts[selected]->text;
  long first,last;control.GetSel(first,last);
  FINDTEXTEXW search;memset(&search,0,sizeof(search));search.chrg.cpMin=last;search.chrg.cpMax=-1;search.lpstrText=query;
  LRESULT found=::SendMessageW(control.m_hWnd,EM_FINDTEXTEXW,FR_DOWN,(LPARAM)&search);
  if(found==-1) {search.chrg.cpMin=0;found=::SendMessageW(control.m_hWnd,EM_FINDTEXTEXW,FR_DOWN,(LPARAM)&search);}
  if(found!=-1) {control.SetSel(search.chrgText);control.SendMessage(EM_SCROLLCARET);control.SetFocus();}
}
void CWorkspacePane::OnPause()
{
  const int selected=m_tabs.GetCurSel();if(selected<0 || selected>=(int)m_transcripts.size())return;
  Transcript& transcript=*m_transcripts[selected];transcript.paused=m_pause.GetCheck()==BST_CHECKED;
  if(!transcript.paused) {transcript.unread=0;UpdateTabLabel(selected);transcript.text.SetRedraw(FALSE);Rebuild(transcript);transcript.text.SetRedraw(TRUE);transcript.text.Invalidate();}
}

void CWorkspacePane::UpdateTabLabel(size_t index)
{
  if(index>=m_transcripts.size())return;
  const Transcript& transcript=*m_transcripts[index];
  CString label=transcript.name+" ["+transcript.connectionId.Left(8)+"]";
  if(transcript.unread) {CString count;count.Format(" (%u)",transcript.unread);label+=count;}
  TCITEM item;memset(&item,0,sizeof(item));item.mask=TCIF_TEXT;item.pszText=const_cast<char*>((LPCTSTR)label);
  m_tabs.SetItem((int)index,&item);
}
bool CWorkspacePane::FocusTranscript(size_t index)
{
  if(index>=m_transcripts.size())return false;SaveInputDraft();m_tabs.SetCurSel((int)index);
  Transcript& transcript=*m_transcripts[index];
  if(!transcript.paused){transcript.unread=0;UpdateTabLabel(index);}
  LayoutChildren();transcript.text.SetFocus();return true;
}
unsigned int CWorkspacePane::UnreadCount(const CString& id,const CString& name) const
{
  for(size_t i=0;i<m_transcripts.size();++i)if(m_transcripts[i]->connectionId==id && m_transcripts[i]->name==name)return m_transcripts[i]->unread;
  return 0;
}
std::wstring CWorkspacePane::InputPrefix(const CString& id,const CString& name) const
{
  for(size_t i=0;i<m_transcripts.size();++i)if(m_transcripts[i]->connectionId==id && m_transcripts[i]->name==name)return m_transcripts[i]->inputPrefix;
  return std::wstring();
}

CSize CWorkspacePane::MinimumSize() const
{
  CSize minimum(220,140);
  if(m_toolWindow && ::IsWindow(m_toolWindow))
    {MINMAXINFO requested;memset(&requested,0,sizeof(requested));
     ::SendMessageW(m_toolWindow,WM_GETMINMAXINFO,0,(LPARAM)&requested);
     minimum.cx=max(minimum.cx,min(4096,max(0,requested.ptMinTrackSize.x))+12);
     minimum.cy=max(minimum.cy,min(4096,max(0,requested.ptMinTrackSize.y))+30);}
  return minimum;
}

CSize CWorkspacePane::DockAvailableSize() const
{
  CRect bounds;
  if(m_pDockBar)
    {const CRect* current=(const CRect*)GetPropW(m_pDockBar->m_hWnd,WorkspaceDockLayoutProperty);
     if(current)bounds=*current;}
  if(bounds.IsRectEmpty() && m_pDockSite)m_pDockSite->GetClientRect(&bounds);
  // MFC dock bars overlap their edges. Leave a small inner allowance so child
  // controls remain inside the visible frame during both query and commit.
  return CSize(max(1,bounds.Width()-8),max(1,bounds.Height()-8));
}

long CWorkspacePane::TextLength(const CRichEditCtrl& control)
{
  GETTEXTLENGTHEX length={GTL_PRECISE|GTL_NUMCHARS,1200};
  return (long)::SendMessageW(control.m_hWnd,EM_GETTEXTLENGTHEX,(WPARAM)&length,0);
}
bool CWorkspacePane::ShouldFollow(const CRichEditCtrl& control)
{
  long start,end;control.GetSel(start,end);if(start!=end || end!=TextLength(control))return false;
  SCROLLINFO scroll;memset(&scroll,0,sizeof(scroll));scroll.cbSize=sizeof(scroll);scroll.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
  if(!::GetScrollInfo(control.m_hWnd,SB_VERT,&scroll))return true;
  const int bottom=max(scroll.nMin,scroll.nMax-(int)max(1U,scroll.nPage)+1);
  return scroll.nPos>=bottom-1;
}
CWorkspacePane::ViewState CWorkspacePane::SaveView(CRichEditCtrl& control)
{
  ViewState state;control.GetSel(state.start,state.end);state.follow=ShouldFollow(control);
  state.anchor=(long)::SendMessageW(control.m_hWnd,EM_LINEINDEX,control.GetFirstVisibleLine(),0);
  POINTL position={0,0};::SendMessageW(control.m_hWnd,EM_POSFROMCHAR,(WPARAM)&position,state.anchor);state.anchorY=position.y;
  return state;
}
void CWorkspacePane::RestoreView(CRichEditCtrl& control,const ViewState& state)
{
  if(state.follow){control.SetSel(-1,-1);control.SendMessage(EM_SCROLLCARET);return;}
  const long length=TextLength(control);control.SetSel(min(state.start,length),min(state.end,length));
  POINTL anchor={0,0};::SendMessageW(control.m_hWnd,EM_POSFROMCHAR,(WPARAM)&anchor,min(state.anchor,length));
  POINT scroll={0,0};::SendMessageW(control.m_hWnd,EM_GETSCROLLPOS,0,(LPARAM)&scroll);
  scroll.y=max(0L,scroll.y+anchor.y-state.anchorY);::SendMessageW(control.m_hWnd,EM_SETSCROLLPOS,0,(LPARAM)&scroll);
}
