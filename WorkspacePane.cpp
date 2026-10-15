#include "stdafx.h"
#include "workspace/WorkspaceNativeControl.h"
#include "WorkspacePane.h"
#include "pcre/pcre.h"
#include "WorkspaceResources.h"
#include "WorkspaceStyle.h"
#include "workspace/WorkspaceFocus.h"
#include "workspace/WorkspaceDockActions.h"
#include <set>
#include <atlconv.h>
#include <afxmultipaneframewnd.h>
#include <richole.h>
#include <afxpanedivider.h>
#include <afxpanecontainermanager.h>
#include <afxpanecontainer.h>

namespace {std::set<HWND> valyriaChannelWindows;}

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

// MFC performs resource loading while creating/painting tab and float hosts.
// Keep its standard behavior, with the EXE resources instead of locale DLLs.
class WorkspaceNativeFloat : public CMultiPaneFrameWnd
{
 DECLARE_DYNCREATE(WorkspaceNativeFloat)
 WorkspaceFocusState focus_;
protected:
 virtual LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp)
 {focus_.Before(m_hWnd,message,wp);LRESULT result;
  if(WorkspaceFrameworkResourceMessage(message,lp)){WorkspaceModernResources resources;result=CMultiPaneFrameWnd::WindowProc(message,wp,lp);}else result=CMultiPaneFrameWnd::WindowProc(message,wp,lp);
  focus_.After(message,wp);return result;}
};
IMPLEMENT_DYNCREATE(WorkspaceNativeFloat,CMultiPaneFrameWnd)
class WorkspaceNativeTabs : public CTabbedPane
{
 DECLARE_DYNCREATE(WorkspaceNativeTabs)
public:
 WorkspaceNativeTabs(){SetMiniFrameRTC(RUNTIME_CLASS(WorkspaceNativeFloat));}
protected:
 virtual LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp)
 {if(WorkspaceFrameworkResourceMessage(message,lp)){WorkspaceModernResources resources;return CTabbedPane::WindowProc(message,wp,lp);}return CTabbedPane::WindowProc(message,wp,lp);}
};
IMPLEMENT_DYNCREATE(WorkspaceNativeTabs,CTabbedPane)


IMPLEMENT_DYNAMIC(CWorkspacePane,CDockablePane)

BEGIN_MESSAGE_MAP(CWorkspacePane, CDockablePane)
  ON_WM_CREATE()
  ON_WM_TIMER()
  ON_BN_CLICKED(3, OnReviewInput)
  ON_BN_CLICKED(5, OnFindNext)
  ON_BN_CLICKED(6, OnPause)
  ON_BN_CLICKED(10, OnArrange)
  ON_BN_CLICKED(11, OnFindToggle)
  ON_NOTIFY_RANGE(EN_LINK, 100, 131, OnLink)
  ON_WM_SIZE()
  ON_WM_PAINT()
  ON_WM_SETFOCUS()
  ON_WM_SHOWWINDOW()
  ON_NOTIFY(TCN_SELCHANGE, 1, OnTabChanged)
END_MESSAGE_MAP()

CWorkspacePane::CWorkspacePane() : m_floatSize(500,350), m_dockWidth(360),
  m_dockHeight(250), m_resizing(false), m_resizeOriginal(0), m_toolWindow(NULL), m_selectedInput(-1), m_labelDirty(false), m_renderBatchDepth(0) {}
CWorkspacePane::~CWorkspacePane()
{
  valyriaChannelWindows.erase(m_hWnd);
  for (size_t i=0; i<m_transcripts.size(); ++i) delete m_transcripts[i];
}
BOOL CWorkspacePane::Create(CFrameWnd* frame,UINT barId)
{
  WorkspaceModernResources resources;
  // Native mouse docking rejects edge drops into floating groups unless the
  // individual pane advertises multi-pane floating support.
  const BOOL created=CDockablePane::Create("Transcripts",frame,CRect(0,0,360,350),TRUE,barId,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|CBRS_RIGHT|CBRS_FLOAT_MULTI);
  // CreateEx selects its default classes. Install our resource-aware hosts
  // afterwards so tab detachment and later floating keep using them.
  if(created){SetTabbedPaneRTC(RUNTIME_CLASS(WorkspaceNativeTabs));SetMiniFrameRTC(RUNTIME_CLASS(WorkspaceNativeFloat));valyriaChannelWindows.insert(m_hWnd);}
  return created;
}
CDockablePane* CWorkspacePane::AttachToTabWnd(CDockablePane* target,AFX_DOCK_METHOD method,BOOL active,CDockablePane** group)
{WorkspaceModernResources resources;return CDockablePane::AttachToTabWnd(target,method,active,group);}
BOOL CWorkspacePane::FloatPane(CRect bounds,AFX_DOCK_METHOD method,bool show)
{return CDockablePane::FloatPane(bounds,method,show);}
LRESULT CWorkspacePane::WindowProc(UINT message,WPARAM wp,LPARAM lp)
{
  if(message==WM_COMMAND && LOWORD(wp)==2 && (HIWORD(wp)==EN_SETFOCUS || HIWORD(wp)==EN_KILLFOCUS))WorkspaceFocusState::Trace("channel-input",m_hWnd,message,wp);
  if(message==WM_NCDESTROY)valyriaChannelWindows.erase(m_hWnd);
  if(message==WM_THEMECHANGED && m_tabs.GetSafeHwnd())LayoutChildren();
  if(WorkspaceFrameworkResourceMessage(message,lp))
    {WorkspaceModernResources resources;return CDockablePane::WindowProc(message,wp,lp);}
  return CDockablePane::WindowProc(message,wp,lp);
}
int CWorkspacePane::OnCreate(LPCREATESTRUCT cs)
{
  if (CDockablePane::OnCreate(cs)==-1) return -1;
  if (!m_tabs.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP, CRect(0,0,1,1), this, 1)) return -1;
  m_tabs.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
  HWND input=::CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
    0,0,1,1,m_hWnd,(HMENU)2,AfxGetInstanceHandle(),NULL);
  if(!input || !workspace_native_control::Attach(m_input,input)) return -1;
  ::SendMessageW(input,EM_SETLIMITTEXT,16384,0);
  ::SendMessageW(input,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
  if(!m_reviewInput.Create("Send",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,CRect(0,0,1,1),this,3)) return -1;
  m_reviewInput.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
  HWND search=::CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
    0,0,1,1,m_hWnd,(HMENU)4,AfxGetInstanceHandle(),NULL);
  if(!search || !workspace_native_control::Attach(m_search,search))return -1;
  ::SendMessageW(search,EM_SETLIMITTEXT,1024,0);::SendMessageW(search,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
  if(!m_findNext.Create("Find next",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,CRect(0,0,1,1),this,5) ||
     !m_pause.Create("Pause",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,CRect(0,0,1,1),this,6))return -1;
  m_findNext.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
  m_pause.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
  m_inputStatus.Create("",WS_CHILD|WS_VISIBLE,CRect(0,0,1,1),this,8);
  m_destination.Create("Destination",WS_CHILD|WS_VISIBLE|SS_NOPREFIX,CRect(0,0,1,1),this,9);
  if(!m_arrange.Create("Arrange",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,CRect(0,0,1,1),this,10) ||
     !m_findToggle.Create("Find",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,CRect(0,0,1,1),this,11))return -1;
  workspace_style::ApplySurface(m_hWnd);
  for(HWND control:{m_input.m_hWnd,m_search.m_hWnd,m_inputStatus.m_hWnd,m_destination.m_hWnd,m_pause.m_hWnd})workspace_style::StyleControl(control);
  workspace_style::StyleButton(m_reviewInput.m_hWnd,workspace_style::Icon::Send,true,L"Send to this channel's original character connection");
  workspace_style::StyleButton(m_arrange.m_hWnd,workspace_style::Icon::Channels,false,L"Join, split, detach or reorder this channel (Ctrl+Shift+M)");
  workspace_style::StyleButton(m_findToggle.m_hWnd,workspace_style::Icon::Find,false,L"Find in this channel (Ctrl+F)");
  workspace_style::StyleButton(m_findNext.m_hWnd,workspace_style::Icon::Find);
  workspace_style::StyleControl(m_tabs.m_hWnd);
  m_tabs.SetItemSize(CSize(workspace_style::Scale(m_hWnd,110),workspace_style::Scale(m_hWnd,40)));
  SetTimer(1,500,NULL);
  return 0;
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
  if(!m_transcripts.empty() && !Find(id,name))return false;
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
    if (!textWindow || !workspace_native_control::Attach(transcript->text,textWindow))
      { if (textWindow) ::DestroyWindow(textWindow); delete transcript; return false; }
    WorkspaceOleCallback* oleCallback=new WorkspaceOleCallback;
    ::SendMessageW(textWindow,EM_SETOLECALLBACK,0,(LPARAM)oleCallback);oleCallback->Release();
    transcript->text.SetBackgroundColor(FALSE,RGB(25,25,28));
    transcript->text.LimitText(4*1024*1024);
    transcript->text.SendMessage(EM_SETUNDOLIMIT,0,0); // Read-only output has no editable undo history.
    transcript->text.SetEventMask(transcript->text.GetEventMask()|ENM_LINK);
    CHARFORMAT2W format; memset(&format,0,sizeof(format)); format.cbSize=sizeof(format);
    format.dwMask=CFM_FACE|CFM_SIZE; format.yHeight=200;
    wcscpy_s(format.szFaceName, L"Consolas");
    ::SendMessageW(textWindow,EM_SETCHARFORMAT,0,(LPARAM)&format);
    CString label=name+" ["+id.Left(8)+"]";
    m_tabs.InsertItem((int)m_transcripts.size(),label);
    m_transcripts.push_back(transcript);SetWindowText(label);
    }
  for (size_t i=0; i<m_transcripts.size(); ++i)
    if (m_transcripts[i]==transcript) m_tabs.SetCurSel((int)i);
  if(!transcript->paused){transcript->unread=0;transcript->mentions=0;for(size_t i=0;i<m_transcripts.size();++i)if(m_transcripts[i]==transcript)UpdateTabLabel(i);}
  LayoutChildren();
  return true;
}
namespace {
void WorkspaceSelect(CRichEditCtrl& control,long first,long last)
{
  long currentFirst,currentLast;control.GetSel(currentFirst,currentLast);
  // RichEdit can reflow retained text even when EM_EXSETSEL changes nothing.
  if(currentFirst!=first || currentLast!=last)control.SetSel(first,last);
}
struct WorkspaceRtfStream { const std::string* data; size_t offset; };
DWORD CALLBACK WorkspaceReadRtf(DWORD_PTR cookie, LPBYTE buffer, LONG count, LONG* read)
{
  WorkspaceRtfStream* stream=(WorkspaceRtfStream*)cookie;
  *read=(LONG)min((size_t)count,stream->data->size()-stream->offset);
  memcpy(buffer,stream->data->data()+stream->offset,*read);
  stream->offset+=*read; return 0;
}
long WorkspaceRtfText(std::ostringstream& out,const std::wstring& text)
{
  long characters=0;
  for(size_t i=0;i<text.size() && text[i];++i){
    wchar_t c=text[i];++characters;
    if(c=='\r' || c=='\n'){if(c=='\r' && i+1<text.size() && text[i+1]=='\n')++i;out<<"\\par ";}
    else if(c=='\t')out<<"\\tab ";
    else if(c=='{' || c=='}' || c=='\\')out<<'\\'<<(char)c;
    else if(c>=32 && c<127)out<<(char)c;
    else out<<"\\u"<<(int)(short)c<<' ';
  }
  return characters;
}
}
long CWorkspacePane::RenderRecord(Transcript& transcript, const WorkspaceRecord& record,long insertAt,bool track)
{
  CRichEditCtrl& control=transcript.text;
  if(record.hasSurfaceColors && track){
    if(transcript.surfaceBackground!=record.surfaceBackground)
      control.SetBackgroundColor(FALSE,record.surfaceBackground);
    transcript.surfaceForeground=record.surfaceForeground;
    transcript.surfaceBackground=record.surfaceBackground;
  }
  const long origin=insertAt<0?(transcript.renderedEnds.empty()?0:transcript.renderedEnds.back()):insertAt;
  long end=origin;
  const bool imageReady=record.image && record.image->GetState()==CInlineImage::Ready;
  // Inline-image source runs are the generated [URL] fallback. Retain them
  // in the record, but render the decoded picture at the paragraph origin.
  if(!imageReady && !record.runs.empty())
    {
    // Stream one formatted paragraph rather than forcing RichEdit layout and
    // precise-length queries separately for every style run.
    std::ostringstream out;out<<"{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0 Consolas;}}{\\colortbl;";
    for(const auto& run:record.runs)for(COLORREF color:{run.foreground,run.background})
      out<<"\\red"<<(int)GetRValue(color)<<"\\green"<<(int)GetGValue(color)<<"\\blue"<<(int)GetBValue(color)<<';';
    out<<"}\\plain\\f0\\fs20\\uc0 ";
    long position=origin;std::vector<LinkRange> links;
    for(size_t i=0;i<record.runs.size();++i){
      const WorkspaceRun& run=record.runs[i];out<<"\\cf"<<i*2+1<<"\\highlight"<<i*2+2<<(run.bold?"\\b ":"\\b0 ")<<(run.italic?"\\i ":"\\i0 ")<<(run.underline?"\\ul ":"\\ulnone ");
      const long first=position;position+=WorkspaceRtfText(out,run.text);
      if(!run.action.IsEmpty()){LinkRange link;link.first=first;link.last=position;link.type=run.actionType;link.action=run.action;links.push_back(link);}
    }
    out<<'}';const std::string rtf=out.str();WorkspaceRtfStream data={&rtf,0};EDITSTREAM stream={0};stream.dwCookie=(DWORD_PTR)&data;stream.pfnCallback=WorkspaceReadRtf;
    WorkspaceSelect(control,origin,origin);
    control.StreamIn(SF_RTF|SFF_SELECTION,stream);
    long selectionStart;control.GetSel(selectionStart,end);
    for(const auto& link:links){control.SetSel(link.first,link.last);CHARFORMAT2W format;memset(&format,0,sizeof(format));format.cbSize=sizeof(format);format.dwMask=CFM_LINK;format.dwEffects=CFE_LINK;::SendMessageW(control.m_hWnd,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);transcript.links.push_back(link);}
    }
  if (record.image) end=RenderImage(transcript,record,end);
  WorkspaceSelect(control,end,end);
  CHARFORMAT2W paragraph;memset(&paragraph,0,sizeof(paragraph));paragraph.cbSize=sizeof(paragraph);
  paragraph.dwMask=CFM_COLOR|CFM_BACKCOLOR|CFM_BOLD|CFM_ITALIC|CFM_UNDERLINE|CFM_LINK;
  paragraph.crTextColor=record.hasSurfaceColors?record.surfaceForeground:transcript.surfaceForeground;
  paragraph.crBackColor=record.hasSurfaceColors?record.surfaceBackground:transcript.surfaceBackground;
  if(record.bleedBackground){
    if(!record.runs.empty())paragraph.crBackColor=record.runs.back().background;
    else {CHARFORMAT2W inherited;memset(&inherited,0,sizeof(inherited));inherited.cbSize=sizeof(inherited);
      // RichEdit's insertion style can reset at the document end. Read the
      // preceding paragraph mark when intentionally bleeding an empty line.
      if(end>0)WorkspaceSelect(control,end-1,end);
      ::SendMessageW(control.m_hWnd,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&inherited);
      WorkspaceSelect(control,end,end);
      if(!(inherited.dwEffects&CFE_AUTOBACKCOLOR))paragraph.crBackColor=inherited.crBackColor;}
  }
  ::SendMessageW(control.m_hWnd,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&paragraph);
  ::SendMessageW(control.m_hWnd,EM_REPLACESEL,FALSE,(LPARAM)L"\r\n");
  long selectionStart,renderedEnd;control.GetSel(selectionStart,renderedEnd);
  if(track){transcript.imageStates.push_back(record.image?(int)record.image->GetState():-1);transcript.renderedEnds.push_back(renderedEnd);}
  return renderedEnd;
}
void CWorkspacePane::EndRenderBatch()
{
  if(!m_renderBatchDepth || --m_renderBatchDepth)return;
  for(auto* transcript:m_transcripts)if(transcript->batchPending){
    transcript->batchPending=false;
    transcript->text.SetRedraw(TRUE);
    RestoreView(transcript->text,transcript->batchView);
    transcript->text.Invalidate();
  }
}
namespace {
std::wstring MentionFold(const std::wstring& text)
{
  if(text.empty())return {};
  int length=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,text.data(),(int)text.size(),NULL,0,NULL,NULL,0);
  if(!length)return {};
  std::wstring folded(length,L'\0');
  if(!LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,text.data(),(int)text.size(),&folded[0],length,NULL,NULL,0))return {};
  return folded;
}
bool RecordMentions(const WorkspaceRecord& record,const std::wstring& mention)
{
  if(mention.empty() || !record.serverOutput || record.imageOnly)return false;
  std::wstring text;for(const auto& run:record.runs)text+=run.text;
  return MentionFold(text).find(mention)!=std::wstring::npos;
}
}
bool CWorkspacePane::AppendRecord(const CString& name, const WorkspaceRecord& record)
{
  Transcript* transcript=Find(record.connectionId,name);
  if (!transcript) return false; // output never silently creates/focuses panes
  if (!transcript->records.empty() && record.sequence &&
      transcript->records.back().sequence>=record.sequence) return false;
  size_t incoming=0;for(const auto& run:record.runs)incoming+=run.text.size();
  if(incoming>65536)return false;
  CBaseTabbedPane* group=IsTabbed()?GetParentTabbedPane():NULL;
  const bool inactive=group && group->GetUnderlyingWindow() && group->GetUnderlyingWindow()->GetActiveWnd()!=this;
  if(transcript->paused || inactive){
    if(RecordMentions(record,transcript->foldedMention))transcript->mentions=min(9999U,transcript->mentions+1);
    transcript->deferred=true;
    transcript->records.push_back(record);
    transcript->retainedBytes+=incoming*sizeof(wchar_t)+(record.image?512*384*4:0);
    if(transcript->records.size()>2000 || transcript->retainedBytes>8*1024*1024){
      while(transcript->records.size()>1000 || (transcript->retainedBytes>4*1024*1024 && transcript->records.size()>1)){
        const auto& oldest=transcript->records.front();for(const auto& run:oldest.runs)transcript->retainedBytes-=run.text.size()*sizeof(wchar_t);
        if(oldest.image)transcript->retainedBytes-=512*384*4;transcript->records.pop_front();
        if(transcript->deferredRemoved<transcript->renderedEnds.size())++transcript->deferredRemoved;
      }
      if(name!="Output" && m_pruneHandler)m_pruneHandler(record.connectionId,name,transcript->records);
    }
    transcript->unread=min(9999U,transcript->unread+1);m_labelDirty=true;
    return true; // Frozen/inactive output retains data without native layout work.
  }
  RefreshDeferred();
  const bool batching=m_renderBatchDepth!=0;
  if(batching && !transcript->batchPending){transcript->batchView=SaveView(transcript->text);transcript->batchPending=true;}
  const ViewState view=batching?transcript->batchView:SaveView(transcript->text);
  const bool follow=view.follow;
  size_t tabIndex=0;for(;tabIndex<m_transcripts.size();++tabIndex)if(m_transcripts[tabIndex]==transcript)break;
  const bool visible=IsWindowVisible()!=FALSE && m_tabs.GetCurSel()==(int)tabIndex && !transcript->paused && follow;
  if(!visible) { transcript->unread=min(9999U,transcript->unread+1);if(RecordMentions(record,transcript->foldedMention))transcript->mentions=min(9999U,transcript->mentions+1);m_labelDirty=true; }
  transcript->text.SetRedraw(FALSE);
  transcript->records.push_back(record);
  size_t& retainedBytes=transcript->retainedBytes;
  retainedBytes+=incoming*sizeof(wchar_t)+(record.image?512*384*4:0);
  bool rebuilt=false;
  if (transcript->records.size()>2000 || (!transcript->renderedEnds.empty() && transcript->renderedEnds.back()>3*1024*1024) || retainedBytes>8*1024*1024)
    {
    size_t removed=0;
    while (transcript->records.size()>1000 || (retainedBytes>4*1024*1024 && transcript->records.size()>1))
      { const WorkspaceRecord& item=transcript->records.front();
        for(size_t j=0;j<item.runs.size();++j) retainedBytes-=item.runs[j].text.size()*sizeof(wchar_t);
        if(item.image) retainedBytes-=512*384*4;
        transcript->records.pop_front();++removed; }
    if(!transcript->paused){
      if(transcript->renderedEnds.size()+1==transcript->records.size()+removed){
        const long cut=TrimRenderedPrefix(*transcript,removed);
        ViewState adjusted=view;adjusted.start=max(0L,adjusted.start-cut);adjusted.end=max(0L,adjusted.end-cut);adjusted.anchor=max(0L,adjusted.anchor-cut);
        RenderRecord(*transcript,record);
        if(batching)transcript->batchView=adjusted;
        else if(!follow)RestoreView(transcript->text,adjusted);
      }else {Rebuild(*transcript);if(!batching && !follow)RestoreView(transcript->text,view);}
    }
    if(name!="Output" && m_pruneHandler)m_pruneHandler(record.connectionId,name,transcript->records);
    rebuilt=true;
    }
  else if(!transcript->paused)RenderRecord(*transcript,record);
  if (!batching && !follow && !rebuilt)
    {
    RestoreView(transcript->text,view);
    }
  if(!batching){
    transcript->text.SetRedraw(TRUE); transcript->text.Invalidate();
    if (follow) transcript->text.SendMessage(EM_SCROLLCARET);
  }
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
    { CRect bounds; GetClientRect(bounds); bounds.DeflateRect(6,6,6,6);
      ::MoveWindow(m_toolWindow,bounds.left,bounds.top,max(1,bounds.Width()),max(1,bounds.Height()),TRUE); return; }
  const auto S=[&](int value){return workspace_style::Scale(m_hWnd,value);};
  CRect rect; GetClientRect(rect); rect.DeflateRect(S(10),S(8),S(10),S(8));
  const int inputHeight=S(34),sendWidth=S(84),gap=S(6);
  CRect inputRect=rect;inputRect.top=max(inputRect.top,inputRect.bottom-inputHeight);
  m_reviewInput.MoveWindow(max(inputRect.left,inputRect.right-sendWidth),inputRect.top,sendWidth,inputHeight);
  m_input.MoveWindow(inputRect.left,inputRect.top,max(1,inputRect.Width()-sendWidth-gap),inputHeight);
  m_destination.MoveWindow(rect.left,max(rect.top,inputRect.top-S(23)),max(1,rect.Width()),S(20));
  // Submission feedback has a separate accessible label; only reserves a row
  // when it contains a status. The stable destination is always visible.
  const bool hasStatus=m_inputStatus.GetWindowTextLength()>0;
  m_inputStatus.ShowWindow(hasStatus?SW_SHOW:SW_HIDE);
  if(hasStatus)m_inputStatus.MoveWindow(rect.left,max(rect.top,inputRect.top-S(43)),max(1,rect.Width()),S(18));
  rect.bottom=max(rect.top,inputRect.top-S(hasStatus?49:29));
  const bool compact=rect.Width()<S(244);
  m_findToggle.MoveWindow(rect.left,rect.top,S(72),S(30));
  m_arrange.MoveWindow(rect.left+S(78),rect.top,max(S(72),min(S(96),rect.Width()-S(78))),S(30));
  m_pause.MoveWindow(compact?rect.left:rect.right-S(64),rect.top+(compact?S(36):0),S(64),S(30));
  rect.top=min(rect.bottom,rect.top+S(compact?72:36));
  m_search.ShowWindow(m_searchVisible?SW_SHOW:SW_HIDE);m_findNext.ShowWindow(m_searchVisible?SW_SHOW:SW_HIDE);
  if(m_searchVisible){m_search.MoveWindow(rect.left,rect.top,max(1,rect.Width()-S(100)),S(30));m_findNext.MoveWindow(max(rect.left,rect.right-S(94)),rect.top,S(94),S(30));rect.top=min(rect.bottom,rect.top+S(36));}
  const int selected=m_tabs.GetCurSel();
  m_pause.SetCheck(selected>=0 && selected<(int)m_transcripts.size() && m_transcripts[selected]->paused?BST_CHECKED:BST_UNCHECKED);
  m_pause.EnableWindow(selected>=0);m_findNext.EnableWindow(selected>=0);
  if(selected!=m_selectedInput)
    { m_selectedInput=selected;
      const wchar_t* draft=selected>=0 && selected<(int)m_transcripts.size()?m_transcripts[selected]->inputDraft.c_str():L"";
      ::SetWindowTextW(m_input.m_hWnd,draft); }
  m_input.EnableWindow(selected>=0);m_reviewInput.EnableWindow(selected>=0);
  if(selected>=0 && selected<(int)m_transcripts.size()){
    const auto& transcript=*m_transcripts[selected];
    const std::wstring channel=transcript.name=="Output"?L"Main":std::wstring(CA2W(transcript.name,CP_UTF8));
    const std::wstring owner=m_characterLabel.empty()?std::wstring(CA2W(transcript.connectionId,CP_UTF8)):m_characterLabel;
    const std::wstring destination=L"To "+owner+(m_gameLabel.empty()?L"":L" / "+m_gameLabel)+L" / "+channel;
    wchar_t old[1024]={};::GetWindowTextW(m_destination.m_hWnd,old,1024);if(destination!=old)::SetWindowTextW(m_destination.m_hWnd,destination.c_str());
  }
  m_tabs.MoveWindow(rect);
  CRect body=rect; if(m_transcripts.size()>1)m_tabs.AdjustRect(FALSE,body);
  m_tabs.ShowWindow(m_transcripts.size()>1?SW_SHOW:SW_HIDE);
  for (size_t i=0; i<m_transcripts.size(); ++i)
    {
    Transcript& transcript=*m_transcripts[i];
    const ViewState view=SaveView(transcript.text);
    transcript.text.MoveWindow(body);
    CRect viewport;transcript.text.GetClientRect(viewport);
    if(viewport.Width()!=transcript.renderedWidth)ReflowImages(transcript,view);
    m_transcripts[i]->text.ShowWindow((int)i==m_tabs.GetCurSel()?SW_SHOW:SW_HIDE);
    }
}
void CWorkspacePane::OnSize(UINT type,int cx,int cy)
{
  CDockablePane::OnSize(type,cx,cy); LayoutChildren();
}
void CWorkspacePane::OnPaint()
{
  CPaintDC dc(this); CRect rect; GetClientRect(rect);
  dc.FillSolidRect(rect,workspace_style::Colors(m_hWnd).surface);

}
void CWorkspacePane::OnSetFocus(CWnd* oldWindow)
{
  RefreshDeferred();
  CDockablePane::OnSetFocus(oldWindow);
  if(!m_transcripts.empty() && !m_transcripts[0]->paused){m_transcripts[0]->unread=0;m_transcripts[0]->mentions=0;UpdateTabLabel(0);}
}
void CWorkspacePane::OnTabChanged(NMHDR*,LRESULT* result)
{
  SaveInputDraft();
  const int selected=m_tabs.GetCurSel();
  if(selected>=0 && selected<(int)m_transcripts.size() && !m_transcripts[selected]->paused)
    {m_transcripts[selected]->unread=0;m_transcripts[selected]->mentions=0;UpdateTabLabel(selected);}
  LayoutChildren(); *result=0;
}


long CWorkspacePane::RenderImage(Transcript& transcript, const WorkspaceRecord& record,long insertAt)
{
  CRichEditCtrl& control=transcript.text;
  if (record.image->GetState()!=CInlineImage::Ready)
    {
    const wchar_t* label=record.image->GetState()==CInlineImage::Loading ? L" [image loading]" : L" [image unavailable]";
    WorkspaceSelect(control,insertAt,insertAt); ::SendMessageW(control.m_hWnd,EM_REPLACESEL,FALSE,(LPARAM)label);
    long first,last;control.GetSel(first,last);return last;
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
  if (!bitmap || !pixels) {if(bitmap)DeleteObject(bitmap);return insertAt;}
  CDC dc; dc.CreateCompatibleDC(NULL);
  HGDIOBJ old=::SelectObject(dc.m_hDC,bitmap);
  dc.FillSolidRect(0,0,width,height,record.hasSurfaceColors?record.surfaceBackground:transcript.surfaceBackground);
  record.image->Draw(&dc,CRect(0,0,width,height));
  GdiFlush(); // synchronize DIB bits before they are consumed by the metafile
  ::SelectObject(dc.m_hDC,old);
  // RichEdit imports its own static image object. No document/view/GDI pointers
  // survive insertion, and image fetches remain in the existing bounded cache.
  RECT metricBounds={0,0,width*2540/96,height*2540/96};
  HDC metafileDc=CreateEnhMetaFileW(NULL,NULL,&metricBounds,L"MUSHclient\0Workspace image\0\0");
  if(!metafileDc) { DeleteObject(bitmap);return insertAt; }
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
  WorkspaceSelect(control,insertAt,insertAt);
  const long first=insertAt;
  WorkspaceRtfStream data={&rtf,0};
  EDITSTREAM stream={0}; stream.dwCookie=(DWORD_PTR)&data; stream.pfnCallback=WorkspaceReadRtf;
  control.StreamIn(SF_RTF|SFF_SELECTION,stream);
  long selectionStart,last;control.GetSel(selectionStart,last);
  if (!record.imageAction.IsEmpty())
    {
    control.SetSel(first,last);
    CHARFORMAT2W format; memset(&format,0,sizeof(format)); format.cbSize=sizeof(format);
    format.dwMask=CFM_LINK; format.dwEffects=CFE_LINK;
    ::SendMessageW(control.m_hWnd,EM_SETCHARFORMAT,SCF_SELECTION,(LPARAM)&format);
    LinkRange link; link.first=first; link.last=last; link.type=0x0800;
    link.action=record.imageAction; transcript.links.push_back(link);
    }
  return last;
}
long CWorkspacePane::ReplaceRenderedRecord(Transcript& transcript,size_t index)
{
  const long first=index?transcript.renderedEnds[index-1]:0,last=transcript.renderedEnds[index];
  transcript.text.SetSel(first,last);
  ::SendMessageW(transcript.text.m_hWnd,EM_REPLACESEL,FALSE,(LPARAM)L"");
  for(auto link=transcript.links.begin();link!=transcript.links.end();){
    if(link->first>=first && link->first<last)link=transcript.links.erase(link);
    else {if(link->first>=last){link->first-=last-first;link->last-=last-first;}++link;}
  }
  const size_t oldLinks=transcript.links.size();
  const long end=RenderRecord(transcript,transcript.records[index],first,false),inserted=end-first;
  for(size_t i=0;i<oldLinks;++i)if(transcript.links[i].first>=first){transcript.links[i].first+=inserted;transcript.links[i].last+=inserted;}
  const long delta=end-last;
  for(size_t i=index;i<transcript.renderedEnds.size();++i)transcript.renderedEnds[i]+=delta;
  transcript.imageStates[index]=(int)transcript.records[index].image->GetState();
  return delta;
}
bool CWorkspacePane::RetainedSourceAfter(const CString& id,const CString& name,unsigned __int64 boundary) const
{
  for(const auto* transcript:m_transcripts)if(transcript->connectionId==id && transcript->name==name)
    return transcript->records.empty() || (transcript->records.front().firstSourceCookie && transcript->records.front().firstSourceCookie>=boundary);
  return true; // There is no view to reconstruct.
}
void CWorkspacePane::Rebuild(Transcript& transcript)
{
  transcript.deferred=false;
  transcript.deferredRemoved=0;
  CRect viewport;transcript.text.GetClientRect(viewport);transcript.renderedWidth=viewport.Width();
  transcript.text.SetWindowText(""); transcript.links.clear(); transcript.imageStates.clear();transcript.renderedEnds.clear();
  for (size_t i=0;i<transcript.records.size();++i) RenderRecord(transcript,transcript.records[i]);
}
void CWorkspacePane::ReflowImages(Transcript& transcript,ViewState view)
{
  if(transcript.paused || transcript.deferred || m_renderBatchDepth ||
     transcript.renderedEnds.size()!=transcript.records.size())return;
  CRect viewport;transcript.text.GetClientRect(viewport);
  const int previous=max(1,min(512,transcript.renderedWidth-12));
  const int available=max(1,min(512,viewport.Width()-12));
  transcript.renderedWidth=viewport.Width();
  if(previous==available)return;
  bool changed=false;
  for(size_t index=0;index<transcript.records.size();++index){
    const WorkspaceRecord& record=transcript.records[index];
    if(!record.image || record.image->GetState()!=CInlineImage::Ready)continue;
    if(!changed){transcript.text.SetRedraw(FALSE);changed=true;}
    const long first=index?transcript.renderedEnds[index-1]:0,last=transcript.renderedEnds[index];
    const long delta=ReplaceRenderedRecord(transcript,index);
    const auto adjust=[&](long position){return position>=last?position+delta:(position>first?min(position,transcript.renderedEnds[index]):position);};
    view.start=adjust(view.start);view.end=adjust(view.end);view.anchor=adjust(view.anchor);
  }
  if(changed){transcript.text.SetRedraw(TRUE);RestoreView(transcript.text,view);transcript.text.Invalidate();}
}
void CWorkspacePane::RefreshDeferred()
{
  CBaseTabbedPane* group=IsTabbed()?GetParentTabbedPane():NULL;
  if(group && group->GetUnderlyingWindow() && group->GetUnderlyingWindow()->GetActiveWnd()!=this)return;
  for(auto* transcript:m_transcripts)if(transcript->deferred && !transcript->paused){
    ViewState view=transcript->batchPending?transcript->batchView:SaveView(transcript->text);
    // Capture positions in the still-frozen rendered records, rather than in
    // the already-pruned model. The prefix count also handles repeated prunes
    // that remove records which were never rendered. Sequence may be zero, so
    // record order is the identity here; every model mutation removes a prefix.
    struct Position { size_t record;long offset;bool atEnd; } positions[3];
    const long offsets[]={view.start,view.end,view.anchor};
    const size_t removed=transcript->deferredRemoved;
    for(size_t i=0;i<3;++i){
      const auto& ends=transcript->renderedEnds;
      size_t index=std::upper_bound(ends.begin(),ends.end(),offsets[i])-ends.begin();
      if(index==ends.size() && index)--index;
      const long first=index?ends[index-1]:0;
      positions[i].record=index;positions[i].offset=max(0L,offsets[i]-first);
      positions[i].atEnd=index<ends.size() && offsets[i]==ends[index];
    }
    transcript->text.SetRedraw(FALSE);Rebuild(*transcript);
    const auto restorePosition=[&](const Position& position){
      const auto& ends=transcript->renderedEnds;
      if(position.record<removed || position.record-removed>=ends.size())return 0L;
      const size_t index=position.record-removed;
      const long first=index?ends[index-1]:0;
      // An image loading label can become a one-character OLE object during
      // deferral. Keep record-end endpoints at the new paragraph boundary.
      return position.atEnd?ends[index]:min(first+position.offset,ends[index]);
    };
    view.start=restorePosition(positions[0]);view.end=restorePosition(positions[1]);
    view.anchor=restorePosition(positions[2]);
    if(positions[2].record<removed)view.anchorY=0;
    if(transcript->batchPending)transcript->batchView=view;
    RestoreView(transcript->text,view);
    transcript->text.SetRedraw(TRUE);transcript->text.Invalidate();
  }
}
void CWorkspacePane::OnShowWindow(BOOL show,UINT status)
{
  CDockablePane::OnShowWindow(show,status);if(show)RefreshDeferred();
}
long CWorkspacePane::TrimRenderedPrefix(Transcript& transcript,size_t records)
{
  if(!records)return 0;
  const long cut=transcript.renderedEnds[records-1];
  transcript.text.SetSel(0,cut);
  ::SendMessageW(transcript.text.m_hWnd,EM_REPLACESEL,FALSE,(LPARAM)L"");
  for(size_t i=0;i<records;++i)transcript.renderedEnds.pop_front();
  for(auto& end:transcript.renderedEnds)end-=cut;
  transcript.imageStates.erase(transcript.imageStates.begin(),transcript.imageStates.begin()+records);
  for(auto link=transcript.links.begin();link!=transcript.links.end();){
    if(link->last<=cut)link=transcript.links.erase(link);
    else {link->first=max(0L,link->first-cut);link->last-=cut;++link;}
  }
  return cut;
}
void CWorkspacePane::OnTimer(UINT_PTR timer)
{
  if (timer!=1) { CDockablePane::OnTimer(timer); return; }
  if(m_labelDirty){m_labelDirty=false;for(size_t i=0;i<m_transcripts.size();++i)UpdateTabLabel(i);}
  for (size_t i=0;i<m_transcripts.size();++i)
    {
    Transcript& transcript=*m_transcripts[i];
    if(m_connectionHandler){const bool live=m_connectionHandler(transcript.connectionId);m_reviewInput.EnableWindow(live);if(transcript.connectionState!=(live?1:0)){m_inputStatus.SetWindowText(live?"Ready":"Disconnected - draft retained");transcript.connectionState=live?1:0;LayoutChildren();m_reviewInput.EnableWindow(live);}}
    if(transcript.paused || transcript.deferred || m_renderBatchDepth)continue;
    CRect viewport;transcript.text.GetClientRect(viewport);
    if(viewport.Width()!=transcript.renderedWidth)ReflowImages(transcript,SaveView(transcript.text));
    for (size_t j=0;j<transcript.records.size() && j<transcript.imageStates.size();++j)
      if (transcript.records[j].image && transcript.imageStates[j]!=(int)transcript.records[j].image->GetState())
        {
        long start,end; transcript.text.GetSel(start,end);
        const int first=transcript.text.GetFirstVisibleLine();
        ViewState view=SaveView(transcript.text);
        transcript.text.SetRedraw(FALSE);
        if(transcript.renderedEnds.size()==transcript.records.size()){
          for(size_t k=j;k<transcript.records.size();++k)if(transcript.records[k].image && transcript.imageStates[k]!=(int)transcript.records[k].image->GetState()){
            const long first=k?transcript.renderedEnds[k-1]:0,last=transcript.renderedEnds[k];
            const long delta=ReplaceRenderedRecord(transcript,k);
            const auto adjust=[&](long position){return position>=last?position+delta:(position>first?min(position,transcript.renderedEnds[k]):position);};
            view.start=adjust(view.start);view.end=adjust(view.end);view.anchor=adjust(view.anchor);
          }
        }else Rebuild(transcript);
        transcript.text.SetRedraw(TRUE);RestoreView(transcript.text,view);
        transcript.text.Invalidate(); break;
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
          {WorkspaceApplicationResources application;m_actionHandler(connectionId,action.type,action.action);}
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
  copied->retainedBytes=original->retainedBytes;
  for(size_t i=0;i<target.m_transcripts.size();++i)if(target.m_transcripts[i]==copied)target.UpdateTabLabel(i);
  target.m_selectedInput=-1;target.LayoutChildren();target.Rebuild(*copied);
  m_tabs.DeleteItem(selected); original->text.DestroyWindow(); delete original;
  m_transcripts.erase(m_transcripts.begin()+selected);m_selectedInput=-1;
  if (!m_transcripts.empty()) m_tabs.SetCurSel(min(selected,(int)m_transcripts.size()-1));
  LayoutChildren(); return true;
}

int CWorkspacePane::CloseHostedTool()
{
  if(!IsToolHost())return 0;
  const HWND tool=m_toolWindow;const HWND pane=GetSafeHwnd();
  {WorkspaceApplicationResources application;::SendMessageW(tool,WM_CLOSE,0,0);}
  if(!::IsWindow(pane))return 1;
  if(::IsWindow(tool))return -1;
  m_toolWindow=NULL;ShowPane(FALSE,FALSE,FALSE);return 1;
}

bool CWorkspacePane::HostTool(HWND toolWindow,const CString& title)
{
  if (!::IsWindow(toolWindow) || !m_transcripts.empty()) return false;
  m_toolWindow=toolWindow; m_toolTitle=title; SetWindowText(title);
  CBaseTabbedPane* group=GetParentTabbedPane();if(group && group->GetUnderlyingWindow()){CMFCBaseTabCtrl* tabs=group->GetUnderlyingWindow();const int tab=tabs->GetTabFromHwnd(m_hWnd);if(tab>=0)tabs->SetTabLabel(tab,title);}
  LONG_PTR style=::GetWindowLongPtr(toolWindow,GWL_STYLE);
  style &= ~(WS_POPUP|WS_CAPTION|WS_THICKFRAME|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
  style |= WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN;
  ::SetWindowLongPtr(toolWindow,GWL_STYLE,style);
  ::SetParent(toolWindow,m_hWnd);
  ::SetWindowPos(toolWindow,NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
  const CSize minimum=MinimumSize();m_floatSize.cx=max(minimum.cx,m_floatSize.cx);m_floatSize.cy=max(minimum.cy,m_floatSize.cy);
  m_dockWidth=max(minimum.cx,m_dockWidth);m_dockHeight=max(minimum.cy,m_dockHeight);
  m_tabs.ShowWindow(SW_HIDE);m_input.ShowWindow(SW_HIDE);m_reviewInput.ShowWindow(SW_HIDE);m_search.ShowWindow(SW_HIDE);m_findNext.ShowWindow(SW_HIDE);m_pause.ShowWindow(SW_HIDE);m_inputStatus.ShowWindow(SW_HIDE);m_destination.ShowWindow(SW_HIDE);m_arrange.ShowWindow(SW_HIDE);m_findToggle.ShowWindow(SW_HIDE); LayoutChildren(); Invalidate(); return true;
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
  if(message->message==WM_KEYDOWN && message->wParam=='M' && (GetKeyState(VK_CONTROL)&0x8000) && (GetKeyState(VK_SHIFT)&0x8000))
    {ShowPlacementMenu();return TRUE;}
  if(message->message==WM_KEYDOWN && message->wParam=='F' && (GetKeyState(VK_CONTROL)&0x8000))
    { FocusSearch();return TRUE; }
  if(message->message==WM_KEYDOWN && message->wParam==VK_ESCAPE && m_searchVisible)
    {m_searchVisible=false;LayoutChildren();m_input.SetFocus();return TRUE;}
  if(message->hwnd==m_search.m_hWnd && message->message==WM_KEYDOWN && message->wParam==VK_RETURN)
    { OnFindNext();return TRUE; }
  if(message->hwnd==m_input.m_hWnd && message->message==WM_KEYDOWN && (message->wParam==VK_UP || message->wParam==VK_DOWN))
    {
    SaveInputDraft();const int selected=m_tabs.GetCurSel();if(selected<0 || selected>=(int)m_transcripts.size())return TRUE;
    Transcript& transcript=*m_transcripts[selected];const std::string raw=WorkspaceUtf8(transcript.inputDraft);
    const std::string text=message->wParam==VK_UP?transcript.history.Up(raw):transcript.history.Down(raw);
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),NULL,0);
    std::wstring wide(count,L'\0');if(count)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),&wide[0],count);
    transcript.inputDraft=wide;::SetWindowTextW(m_input.m_hWnd,wide.c_str());m_input.SetSel(-1,-1);return TRUE;
    }
  if(message->hwnd==m_input.m_hWnd && message->message==WM_KEYDOWN && message->wParam==VK_RETURN)
    { OnReviewInput();return TRUE; }
  return CDockablePane::PreTranslateMessage(message);
}
void CWorkspacePane::OnReviewInput()
{
  SaveInputDraft(); const int selected=m_tabs.GetCurSel();
  if(selected<0 || selected>=(int)m_transcripts.size() || !m_inputHandler) return;
  const Transcript& transcript=*m_transcripts[selected];
  if(transcript.inputDraft.empty()) return;
  const CString identity=transcript.connectionId;
  const std::wstring command=transcript.inputPrefix+transcript.inputDraft;
  const HWND window=GetSafeHwnd();
  const CString name=transcript.name;
  const std::wstring draft=transcript.inputDraft;
  const std::string id=(LPCTSTR)identity;
  const workspace_input::PreparedInput prepared=workspace_input::PrepareHumanInput(id,id,!m_connectionHandler || m_connectionHandler(identity),WorkspaceUtf8(transcript.inputPrefix),WorkspaceUtf8(draft));
  if(!prepared.ok) { AfxMessageBox(prepared.error.c_str());return; }
  const InputHandler callback=m_inputHandler;
  bool submitted;{WorkspaceApplicationResources application;submitted=callback(identity,command);}
  // Alias/script callbacks can close the original world and destroy this pane.
  if(!::IsWindow(window) || CWnd::FromHandlePermanent(window)!=this)return;
  Transcript* current=Find(identity,name);if(!current)return;
  current->history.Sent(WorkspaceUtf8(draft),submitted);
  std::wstring status=submitted?L"Submitted":L"Not submitted";
  if(!submitted && m_inputErrorHandler){const std::wstring error=m_inputErrorHandler(identity);if(!error.empty())status+=L": "+error;}
  ::SetWindowTextW(m_inputStatus.m_hWnd,status.c_str());
  LayoutChildren();
  if(submitted && current->inputDraft==draft)
    {current->inputDraft.clear();::SetWindowTextW(m_input.m_hWnd,L"");}

}


namespace {
const CDockablePane* WorkspaceFirstSplitPane(const CPaneContainer* node)
{
  if(!node)return NULL;
  if(node->GetLeftPane())return node->GetLeftPane();
  const CDockablePane* pane=WorkspaceFirstSplitPane(node->GetLeftPaneContainer());if(pane)return pane;
  if(node->GetRightPane())return node->GetRightPane();return WorkspaceFirstSplitPane(node->GetRightPaneContainer());
}
CWorkspacePane* WorkspaceFirstLeaf(const CDockablePane* pane)
{
  CWorkspacePane* leaf=DYNAMIC_DOWNCAST(CWorkspacePane,const_cast<CDockablePane*>(pane));if(leaf)return leaf;
  CBaseTabbedPane* group=DYNAMIC_DOWNCAST(CBaseTabbedPane,const_cast<CDockablePane*>(pane));
  return group && group->GetTabsNum()?DYNAMIC_DOWNCAST(CWorkspacePane,group->GetUnderlyingWindow()->GetTabWnd(0)):NULL;
}
}

void CWorkspacePane::GetLayouts(std::vector<WorkspacePaneLayout>& layouts,bool detached) const
{
  CBaseTabbedPane* group=GetParentTabbedPane();
  const CDockablePane* owner=group?static_cast<CDockablePane*>(group):this;
  CRect bounds;if(owner->IsFloating() && owner->GetParentMiniFrame())owner->GetParentMiniFrame()->GetWindowRect(bounds);else owner->GetWindowRect(bounds);
  WorkspacePaneLayout layout;
  if(!m_transcripts.empty()){layout.connectionId=m_transcripts[0]->connectionId;layout.name=m_transcripts[0]->name;}
  else if(IsToolHost()){layout.connectionId="@tools";layout.name=m_toolTitle;}
  else return;
  layout.groupConnectionId=layout.connectionId;layout.groupName=layout.name;
  if(group && group->GetUnderlyingWindow())
    {
    CMFCBaseTabCtrl* tabs=group->GetUnderlyingWindow();layout.tabOrder=tabs->GetTabFromHwnd(m_hWnd);layout.active=tabs->GetActiveTab()==layout.tabOrder;
    CWorkspacePane* leader=DYNAMIC_DOWNCAST(CWorkspacePane,tabs->GetTabWnd(0));
    if(leader){if(!leader->m_transcripts.empty()){layout.groupConnectionId=leader->m_transcripts[0]->connectionId;layout.groupName=leader->m_transcripts[0]->name;}else{layout.groupConnectionId="@tools";layout.groupName=leader->m_toolTitle;}}
    }
  layout.detached=detached;layout.floating=owner->IsFloating()!=FALSE;layout.alignment=owner->GetCurrentAlignment();
  layout.x=bounds.left;layout.y=bounds.top;layout.width=bounds.Width();layout.height=bounds.Height();
  CPaneDivider* divider=owner->GetDefaultPaneDivider();
  if(divider && !layout.floating)
    {
    BOOL left=FALSE;CPaneContainer* node=divider->FindPaneContainer(const_cast<CDockablePane*>(owner),left);
    const CDockablePane* neighbor=NULL;CPaneContainer* split=node;
    if(node && !left)neighbor=node->GetLeftPane()?node->GetLeftPane():WorkspaceFirstSplitPane(node->GetLeftPaneContainer());
    while(node && !neighbor && node->GetParentPaneContainer())
      {CPaneContainer* parent=node->GetParentPaneContainer();if(!node->IsLeftPaneContainer()){neighbor=parent->GetLeftPane()?parent->GetLeftPane():WorkspaceFirstSplitPane(parent->GetLeftPaneContainer());split=parent;}node=parent;}
    CWorkspacePane* target=WorkspaceFirstLeaf(neighbor);
    if(target && target!=this && split && split->GetPaneDivider())
      {if(!target->m_transcripts.empty()){layout.splitConnectionId=target->m_transcripts[0]->connectionId;layout.splitName=target->m_transcripts[0]->name;}else{layout.splitConnectionId="@tools";layout.splitName=target->m_toolTitle;}
       const bool horizontal=split->GetPaneDivider()->IsHorizontal()!=FALSE;layout.splitAlignment=horizontal?CBRS_ALIGN_BOTTOM:CBRS_ALIGN_RIGHT;
       CRect total;split->GetWindowRect(total);const int all=horizontal?total.Height():total.Width();const int part=horizontal?bounds.Height():bounds.Width();layout.splitPercent=max(10,min(90,all?part*100/all:50));
       for(CPaneContainer* parent=split->GetParentPaneContainer();parent;parent=parent->GetParentPaneContainer())++layout.splitDepth;}
    }
  layouts.push_back(layout);
}
void CWorkspacePane::ApplyLayout(const WorkspacePaneLayout& layout)
{
  const CSize minimum=MinimumSize();
  m_dockWidth=max(minimum.cx,min(1500,layout.width));m_dockHeight=max(minimum.cy,min(1000,layout.height));
  m_floatSize=CSize(m_dockWidth,m_dockHeight);
  if(GetSafeHwnd())SetWindowPos(NULL,0,0,m_floatSize.cx,m_floatSize.cy,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
}

bool CWorkspacePane::HasTranscript(const CString& id,const CString& name) const
{
  for(size_t i=0;i<m_transcripts.size();++i)if(m_transcripts[i]->connectionId==id && m_transcripts[i]->name==name)return true;
  return false;
}

bool CWorkspacePane::ClearTranscript(const CString& id,const CString& name)
{
  Transcript* transcript=Find(id,name);if(!transcript)return false;
  transcript->records.clear();transcript->links.clear();transcript->imageStates.clear();transcript->renderedEnds.clear();transcript->deferredRemoved=0;transcript->retainedBytes=0;transcript->deferred=false;transcript->unread=0;transcript->mentions=0;
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
  if(!transcript.paused) {transcript.unread=0;transcript.mentions=0;UpdateTabLabel(selected);RefreshDeferred();}
}

void CWorkspacePane::UpdateTabLabel(size_t index)
{
  WorkspaceModernResources resources;
  if(index>=m_transcripts.size())return;
  const Transcript& transcript=*m_transcripts[index];
  CString label=transcript.name+" ["+transcript.connectionId.Left(8)+"]";
  if(!m_characterLabel.empty()){label=(transcript.name=="Output"?CString("Main"):transcript.name);label+=" - ";label+=CW2A(m_characterLabel.c_str(),CP_UTF8);}
  if(transcript.unread) {CString count;count.Format(" (%u)",transcript.unread);label+=count;}
  if(transcript.mentions){CString count;count.Format(" [mentions: %u]",transcript.mentions);label+=count;}
  TCITEM item;memset(&item,0,sizeof(item));item.mask=TCIF_TEXT;item.pszText=const_cast<char*>((LPCTSTR)label);
  m_tabs.SetItem((int)index,&item);SetWindowText(label);
  if(IsTabbed()){CBaseTabbedPane* group=GetParentTabbedPane();if(group && group->GetUnderlyingWindow()){CMFCBaseTabCtrl* tabs=group->GetUnderlyingWindow();int tab=tabs->GetTabFromHwnd(m_hWnd);if(tab>=0)tabs->SetTabLabel(tab,label);}}
}
bool CWorkspacePane::FocusTranscript(size_t index)
{
  if(index>=m_transcripts.size())return false;SaveInputDraft();m_tabs.SetCurSel((int)index);
  Transcript& transcript=*m_transcripts[index];
  if(!transcript.paused){transcript.unread=0;transcript.mentions=0;UpdateTabLabel(index);}
  LayoutChildren();transcript.text.SetFocus();return true;
}
void CWorkspacePane::SetPresentationIdentity(const std::wstring& game,const std::wstring& character)
{
  if(m_gameLabel==game && m_characterLabel==character)return;
  m_gameLabel=game;m_characterLabel=character;
  for(size_t i=0;i<m_transcripts.size();++i)UpdateTabLabel(i);
  LayoutChildren();
}
bool CWorkspacePane::ArrangeBeside(CWorkspacePane& target,Placement placement)
{
  if(&target==this || !::IsWindow(target.m_hWnd) || !::IsWindow(m_hWnd))return false;
  const HWND sourceWindow=m_hWnd,targetWindow=target.m_hWnd;
  SaveInputDraft();
  WorkspaceModernResources resources;
  if(placement==Placement::Join && GetParentTabbedPane() && GetParentTabbedPane()==target.GetParentTabbedPane())return true;
  // Native detach owns recent-dock metadata and preserves the pane, input and
  // RichEdit HWNDs. Never copy/replay transcript records during arrangement.
  if(IsAutoHideMode())SetAutoHideMode(FALSE,CBRS_ALIGN_ANY);
  if(auto* group=GetParentTabbedPane())if(!group->DetachPane(this,TRUE))return false;
  if(!::IsWindow(sourceWindow) || CWnd::FromHandlePermanent(sourceWindow)!=this || !::IsWindow(targetWindow) || CWnd::FromHandlePermanent(targetWindow)!=&target)return false;
  BOOL moved=FALSE;
  if(placement==Placement::Join)moved=AttachToTabWnd(&target,DM_SHOW,TRUE)!=NULL;
  else {
    CDockablePane* anchor=target.GetParentTabbedPane()?static_cast<CDockablePane*>(target.GetParentTabbedPane()):&target;
    const DWORD align=placement==Placement::Left?CBRS_ALIGN_LEFT:placement==Placement::Right?CBRS_ALIGN_RIGHT:placement==Placement::Above?CBRS_ALIGN_TOP:CBRS_ALIGN_BOTTOM;
    CRect desired;anchor->GetWindowRect(desired);if(align==CBRS_ALIGN_LEFT)desired.right=desired.CenterPoint().x;else if(align==CBRS_ALIGN_RIGHT)desired.left=desired.CenterPoint().x;else if(align==CBRS_ALIGN_TOP)desired.bottom=desired.CenterPoint().y;else desired.top=desired.CenterPoint().y;
    moved=DockToWindow(anchor,align,desired);
  }
  if(moved){ShowPane(TRUE,FALSE,TRUE);if(auto* frame=DYNAMIC_DOWNCAST(CFrameWnd,GetDockSiteFrameWnd()))frame->RecalcLayout();}
  return moved!=FALSE;
}
bool CWorkspacePane::DetachToWindow()
{
  SaveInputDraft();
  CRect floating;GetWindowRect(floating);floating.right=floating.left+max(workspace_style::Scale(m_hWnd,440),floating.Width());floating.bottom=floating.top+max(workspace_style::Scale(m_hWnd,320),floating.Height());
  return FloatWorkspacePaneVisible(*this,floating)!=FALSE;
}
void CWorkspacePane::ShowPlacementMenu()
{
  const HWND original=m_hWnd;CString identity,name;if(!m_transcripts.empty()){identity=m_transcripts[0]->connectionId;name=m_transcripts[0]->name;}
  struct Move {HWND target;Placement placement;};std::vector<Move> choices;
  CMenu menu;menu.CreatePopupMenu();
  menu.AppendMenu(MF_STRING,1,"Detach into native window");
  if(auto* group=GetParentTabbedPane()){
    const auto* tabs=group->GetUnderlyingWindow();const int index=tabs->GetTabFromHwnd(m_hWnd);
    menu.AppendMenu(MF_STRING|(index>0?0:MF_GRAYED),2,"Move tab left");
    menu.AppendMenu(MF_STRING|(index>=0 && index<tabs->GetTabsNum()-1?0:MF_GRAYED),3,"Move tab right");
  }
  menu.AppendMenu(MF_SEPARATOR);
  for(HWND window:valyriaChannelWindows){auto* target=DYNAMIC_DOWNCAST(CWorkspacePane,CWnd::FromHandlePermanent(window));if(!target || target==this || !::IsWindow(window) || target->IsEmpty())continue;
    CString title;target->GetWindowText(title);CMenu child;child.CreatePopupMenu();
    const Placement placements[]={Placement::Join,Placement::Left,Placement::Right,Placement::Above,Placement::Below};
    const char* labels[]={"Join tabs","Place left","Place right","Place above","Place below"};
    for(int i=0;i<5;++i){choices.push_back({window,placements[i]});child.AppendMenu(MF_STRING,(UINT)(100+choices.size()-1),labels[i]);}
    menu.AppendMenu(MF_POPUP,(UINT_PTR)child.Detach(),title);
  }
  CRect bounds;m_arrange.GetWindowRect(bounds);if(bounds.IsRectEmpty())GetWindowRect(bounds);
  const UINT command=menu.TrackPopupMenu(TPM_RETURNCMD|TPM_NONOTIFY|TPM_LEFTALIGN,bounds.left,bounds.bottom,this);
  if(!command || !::IsWindow(original) || CWnd::FromHandlePermanent(original)!=this || (!identity.IsEmpty()&&!HasTranscript(identity,name)))return;
  WorkspaceModernResources resources;
  if(command==1){DetachToWindow();return;}
  if(command==2 || command==3){if(auto* group=GetParentTabbedPane()){auto* tabs=group->GetUnderlyingWindow();const int index=tabs->GetTabFromHwnd(m_hWnd),next=index+(command==2?-1:1);if(index>=0 && next>=0 && next<tabs->GetTabsNum())tabs->SwapTabs(index,next);}return;}
  if(command>=100 && command-100<choices.size()){const Move move=choices[command-100];auto* target=DYNAMIC_DOWNCAST(CWorkspacePane,CWnd::FromHandlePermanent(move.target));if(target && ::IsWindow(move.target))ArrangeBeside(*target,move.placement);}
}
unsigned int CWorkspacePane::UnreadCount(const CString& id,const CString& name) const
{
  for(size_t i=0;i<m_transcripts.size();++i)if(m_transcripts[i]->connectionId==id && m_transcripts[i]->name==name)return m_transcripts[i]->unread;
  return 0;
}
unsigned int CWorkspacePane::MentionCount(const CString& id,const CString& name) const
{
  for(const auto* transcript:m_transcripts)if(transcript->connectionId==id && transcript->name==name)return transcript->mentions;
  return 0;
}
bool CWorkspacePane::SetPreferences(const CString& id,const CString& name,const workspace_spawn::Preferences& preferences)
{
  auto* transcript=Find(id,name);if(!transcript || !workspace_spawn::Valid(preferences))return false;
  if(transcript->preferences.mention!=preferences.mention)transcript->mentions=0;
  transcript->preferences=preferences;transcript->foldedMention=MentionFold(preferences.mention);m_labelDirty=true;return true;
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

long CWorkspacePane::TextLength(const CRichEditCtrl& control)
{
  GETTEXTLENGTHEX length={GTL_PRECISE|GTL_NUMCHARS,1200};
  return (long)::SendMessageW(control.m_hWnd,EM_GETTEXTLENGTHEX,(WPARAM)&length,0);
}
bool CWorkspacePane::ShouldFollow(const CRichEditCtrl& control)
{
  long start,end;control.GetSel(start,end);const long length=TextLength(control);
  if(start!=end || end!=length)return false;
  if(!length)return true; // A new control can still have the default scrollbar range.
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
  const long length=TextLength(control);
  if(state.follow){WorkspaceSelect(control,length,length);control.SendMessage(EM_SCROLLCARET);return;}
  WorkspaceSelect(control,min(state.start,length),min(state.end,length));
  POINTL anchor={0,0};::SendMessageW(control.m_hWnd,EM_POSFROMCHAR,(WPARAM)&anchor,min(state.anchor,length));
  POINT scroll={0,0};::SendMessageW(control.m_hWnd,EM_GETSCROLLPOS,0,(LPARAM)&scroll);
  scroll.y=max(0L,scroll.y+anchor.y-state.anchorY);::SendMessageW(control.m_hWnd,EM_SETSCROLLPOS,0,(LPARAM)&scroll);
}
