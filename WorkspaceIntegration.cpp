#include "stdafx.h"
#include "MUSHclient.h"
#include "doc.h"
#include "mainfrm.h"
#include "WorkspaceIntegration.h"
#include "workspace/PennEditorWindow.h"
#include "workspace/WorkspaceInputEncoding.h"
#include "scripting/errors.h"
#include <map>
#include <regex>
#include <set>

WorkspaceRenderBatch::WorkspaceRenderBatch():frame_(NULL)
{
  CMainFrame* frame=DYNAMIC_DOWNCAST(CMainFrame,App.m_pMainWnd);
  if(frame && frame->GetSafeHwnd()){frame_=frame->GetSafeHwnd();frame->BeginWorkspaceRenderBatch();}
}
WorkspaceRenderBatch::~WorkspaceRenderBatch()
{
  CMainFrame* frame=frame_?DYNAMIC_DOWNCAST(CMainFrame,CWnd::FromHandlePermanent(frame_)):NULL;
  if(frame)frame->EndWorkspaceRenderBatch();
}

namespace {
struct Identity { CString id; unsigned __int64 sequence; Identity():sequence(0){} };
std::map<CMUSHclientDoc*, Identity> identities;
unsigned __int64 nextIdentity=0;
agent_workspace::Store agentStore;
std::map<std::string,unsigned __int64> paneLastSequence;
std::map<unsigned __int64,std::string> sceneDraftBindings;
std::map<CString,std::wstring> humanInputErrors;
std::map<CMUSHclientDoc*,std::set<unsigned __int64> > pendingSourceLines;
std::map<CMUSHclientDoc*,std::set<unsigned __int64> > activeSourceLines,deferredLocalLines;
std::wstring wide(const char* text,int length,bool utf8)
{
  if (!length) return std::wstring();
  UINT cp=utf8?CP_UTF8:CP_ACP;
  int count=MultiByteToWideChar(cp,0,text,length,NULL,0);
  std::wstring result(count,L'\0');
  if(count) MultiByteToWideChar(cp,0,text,length,&result[0],count);
  return result;
}
std::string utf8(const std::wstring& text)
{
  if(text.empty()) return std::string();
  int count=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),NULL,0,NULL,NULL);
  std::string result(count,'\0');
  if(count) WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),&result[0],count,NULL,NULL);
  return result;
}
}
agent_workspace::Store& WorkspaceAgentStore() { return agentStore; }
CString WorkspaceConnectionId(CMUSHclientDoc* doc)
{
  auto found=identities.find(doc);if(found!=identities.end())return found->second.id;
  try {
    Identity item;item.id.Format("%s:runtime:%I64u",(LPCTSTR)doc->m_strWorldID,++nextIdentity);
    agentStore.addWindow((LPCTSTR)item.id,utf8(wide(doc->m_mush_name,doc->m_mush_name.GetLength(),false)),"Main");
    try {identities.emplace(doc,item);}catch(...){agentStore.expireWindow((LPCTSTR)item.id);throw;}
    return item.id;
  }catch(...) {return CString();} // Optional workspace limits never stop legacy input.
}
void WorkspaceExpireConnection(CMUSHclientDoc* doc)
{
  std::map<CMUSHclientDoc*,Identity>::iterator it=identities.find(doc);
  if(it==identities.end()) return;
  PennEditorInvalidateEndpoint((LPCTSTR)it->second.id);
  humanInputErrors[it->second.id]=L"Original world disconnected; draft preserved.";
  const std::string panePrefix=std::string((LPCTSTR)it->second.id)+"/pane/";
  for(const auto& w:agentStore.humanWindows())
    if(w.id.compare(0,panePrefix.size(),panePrefix)==0)agentStore.expireWindow(w.id);
  agentStore.expireWindow((LPCTSTR)it->second.id);
  // Preserve the document's docked views, but grants never survive disconnect.
  agentStore.addWindow((LPCTSTR)it->second.id,utf8(wide(doc->m_mush_name,doc->m_mush_name.GetLength(),false)),"Main");
}
void WorkspaceCloseDocument(CMUSHclientDoc* doc)
{
  std::map<CMUSHclientDoc*,Identity>::iterator it=identities.find(doc);
  if(it==identities.end()) return;
  PennEditorInvalidateEndpoint((LPCTSTR)it->second.id);
  humanInputErrors.erase(it->second.id);
  const std::string panePrefix=std::string((LPCTSTR)it->second.id)+"/pane/";
  for(const auto& w:agentStore.humanWindows())
    if(w.id.compare(0,panePrefix.size(),panePrefix)==0)agentStore.expireWindow(w.id);
  agentStore.expireWindow((LPCTSTR)it->second.id);
  if(App.m_pMainWnd && Frame.GetSafeHwnd()) Frame.CloseWorkspaceConnection(it->second.id);
  for(auto pane=paneLastSequence.begin();pane!=paneLastSequence.end();) {if(pane->first.compare(0,panePrefix.size(),panePrefix)==0)pane=paneLastSequence.erase(pane);else ++pane;}
  pendingSourceLines.erase(doc);activeSourceLines.erase(doc);deferredLocalLines.erase(doc);
  identities.erase(it);
}
void WorkspaceClearOutput(CMUSHclientDoc* doc)
{
  auto found=identities.find(doc);
  if(found!=identities.end()) {
    agentStore.resetWindow((LPCTSTR)found->second.id);
    if(App.m_pMainWnd && Frame.GetSafeHwnd())Frame.ClearWorkspaceTranscript(found->second.id,"Output");
  }
}
namespace {
WorkspaceRecord captureLines(CMUSHclientDoc* doc,const std::vector<CLine*>& lines)
{
  WorkspaceRecord record;record.connectionId=WorkspaceConnectionId(doc);record.sequence=0;
  // Use the same surface fill as the original world's output view, including
  // blank records whose style list has no characters to copy.
  record.hasSurfaceColors=true;record.bleedBackground=App.m_bBleedBackground!=FALSE;
  record.surfaceBackground=doc->m_iBackgroundColour!=NO_COLOUR?doc->m_iBackgroundColour:
    doc->TranslateColour(doc->m_bCustom16isDefaultColour?doc->m_customback[15]:doc->m_normalcolour[BLACK],1.0);
  record.surfaceForeground=doc->TranslateColour(doc->m_bCustom16isDefaultColour?doc->m_customtext[15]:doc->m_normalcolour[WHITE],1.0);
  if(lines.empty())return record;
  record.firstSourceCookie=lines.front()->m_workspaceCookie;
  CLine* final=lines.back();
  record.receivedAtMillis=static_cast<unsigned __int64>(final->m_theTime.GetTime())*1000;
  if(final->IsInlineImage()) {record.image=final->m_inlineImage;record.imageWidth=final->m_imageWidth;record.imageHeight=final->m_imageHeight;record.imageAction=final->m_imageSource;}
  size_t remaining=131072;
  for(CLine* line:lines) {
    if(!remaining)break;
    int offset=0;
    for(POSITION pos=line->styleList.GetHeadPosition();pos;) {
      CStyle* style=line->styleList.GetNext(pos);int length=min((int)style->iLength,line->len-offset);length=min(length,(int)remaining);
      if(length<=0)continue;
      WorkspaceRun run;run.text=wide(line->text+offset,length,doc->m_bUTF_8);
      doc->GetStyleRGB(style,run.foreground,run.background);
      run.bold=(style->iFlags&HILITE)!=0;run.italic=(style->iFlags&ITALIC)!=0;run.underline=(style->iFlags&UNDERLINE)!=0;
      if(style->pAction)run.action=style->pAction->m_strAction;
      run.actionType=style->iFlags&ACTIONTYPE;record.runs.push_back(run);offset+=length;remaining-=length;
      if(!remaining)break;
    }
  }
  return record;
}
std::string recordText(const WorkspaceRecord& record) {std::wstring text;for(const auto& run:record.runs)text+=run.text;return utf8(text);}
bool encodeCommand(CMUSHclientDoc* doc,const std::string& source,std::string& encoded) {
  return workspace_input::EncodeForWorld(doc->m_bUTF_8!=0,GetACP(),source,encoded);
}
CMUSHclientDoc* liveDocument(const CString& id) {for(const auto& pair:identities)if(pair.second.id==id)return pair.first;return NULL;}
void ensurePane(const CString& connectionId,const CString& name,const std::string& id) {
  bool found=false;for(const auto& w:agentStore.humanWindows())if(w.id==id)found=true;
  CMUSHclientDoc* doc=liveDocument(connectionId);
  if(!found && doc)agentStore.addWindow(id,utf8(wide(doc->m_mush_name,doc->m_mush_name.GetLength(),false)),utf8(wide(name,name.GetLength(),false)));
}
}
namespace {
POSITION sourcePosition(CMUSHclientDoc* doc,unsigned __int64 cookie) {
  POSITION pos=doc->m_LineList.GetTailPosition();
  while(pos){POSITION source=pos;CLine* line=doc->m_LineList.GetPrev(pos);if(line->m_workspaceCookie==cookie)return source;}
  return NULL;
}
WorkspaceRecord captureAt(CMUSHclientDoc* doc,POSITION pos) {
  std::vector<CLine*> lines;bool tail=true;int flags=0;
  // Retain the bounded prefix, including routing/private-reply markers.
  while(pos) {
    CLine* line=doc->m_LineList.GetPrev(pos);
    if(!tail && (line->hard_return || line->IsInlineImage() || (line->flags&NOTE_OR_COMMAND)!=flags))break;
    if(tail)flags=line->flags&NOTE_OR_COMMAND;
    tail=false;lines.push_back(line);if(line->IsInlineImage())break;
  }
  std::reverse(lines.begin(),lines.end());return captureLines(doc,lines);
}
}
WorkspaceRecord WorkspaceCaptureRecord(CMUSHclientDoc* doc)
{
  return captureAt(doc,doc->m_pCurrentLine?sourcePosition(doc,doc->m_pCurrentLine->m_workspaceCookie):NULL);
}
static void WorkspaceRefreshOutput(CMUSHclientDoc* doc,bool redraw)
{
  auto identity=identities.find(doc);if(identity==identities.end())return;
  try {
    if(redraw && App.m_pMainWnd && Frame.GetSafeHwnd())Frame.ClearWorkspaceTranscript(identity->second.id,"Output");
    // Snapshot only completed retained logical paragraphs, capped to the agent history limit.
    std::deque<std::vector<CLine*> > completed;std::vector<CLine*> paragraph;
    POSITION pos=doc->m_LineList.GetTailPosition();size_t bytes=0;
    while(pos && completed.size()<10000 && bytes<8*1024*1024) {
      CLine* line=doc->m_LineList.GetPrev(pos);
      if(line->hard_return || line->IsInlineImage()) {
        if(!paragraph.empty()){std::reverse(paragraph.begin(),paragraph.end());completed.push_front(paragraph);paragraph.clear();}
        paragraph.push_back(line);
      } else if(!paragraph.empty() && (paragraph.back()->flags&NOTE_OR_COMMAND)==(line->flags&NOTE_OR_COMMAND))paragraph.push_back(line);
      bytes+=line->len;
    }
    if(!paragraph.empty() && completed.size()<10000){std::reverse(paragraph.begin(),paragraph.end());completed.push_front(paragraph);}
    std::vector<agent_workspace::Line> history;
    for(const auto& lines:completed) {
      bool pending=false;auto active=pendingSourceLines.find(doc);
      if(active!=pendingSourceLines.end())for(CLine* line:lines)if(active->second.count(line->m_workspaceCookie))pending=true;
      auto deferred=deferredLocalLines.find(doc);
      if(deferred!=deferredLocalLines.end())for(CLine* line:lines)if(deferred->second.count(line->m_workspaceCookie))pending=true;
      if(pending)continue; // Trigger decisions or local-note delivery are not final yet.
      WorkspaceRecord record=captureLines(doc,lines);record.sequence=++identity->second.sequence;
      agent_workspace::Line row;row.text=recordText(record);row.receivedAt=record.receivedAtMillis;history.push_back(row);
      if(redraw && App.m_pMainWnd && Frame.GetSafeHwnd())Frame.AppendWorkspaceRecord("Output",record);
    }
    agentStore.replaceWindowHistory((LPCTSTR)identity->second.id,history);
  }catch(...) { /* A history mirror cannot interrupt legacy buffer maintenance. */ }
}
void WorkspaceRebuildOutput(CMUSHclientDoc* doc){WorkspaceRefreshOutput(doc,true);}
void WorkspacePruneOutput(CMUSHclientDoc* doc)
{
  auto identity=identities.find(doc);if(identity==identities.end())return;
  // Chunk pruning often discards lines older than the smaller native view.
  // Preserve that view's selection, images and layout when all of it survives.
  const bool preserved=!doc->m_LineList.IsEmpty() && (App.m_pMainWnd && Frame.GetSafeHwnd() &&
    Frame.WorkspaceOutputRetainedAfter(identity->second.id,doc->m_LineList.GetHead()->m_workspaceCookie));
  WorkspaceRefreshOutput(doc,!preserved);
}
bool WorkspacePublishRecord(CMUSHclientDoc* doc,const WorkspaceRecord& prepared,bool omitted)
{
  auto live=identities.find(doc);
  if(prepared.connectionId.IsEmpty() || live==identities.end() || live->second.id!=prepared.connectionId)return false;
  WorkspaceRecord record=prepared;record.sequence=++live->second.sequence;
  std::wstring text;
  for(size_t i=0;i<record.runs.size();++i) text+=record.runs[i].text;
  // Correlated private IDE replies never enter shared transcripts or routes.
  if(PennEditorHandleOutput((LPCTSTR)record.connectionId,utf8(text)))return true;
  if(omitted)return false;
  // A completed logical record is copied once; neither sink invokes triggers.
  bool routedOmit=App.m_pMainWnd && Frame.GetSafeHwnd() && Frame.RouteWorkspaceRecord(record);
  if(!routedOmit)
    {
    agentStore.append((LPCTSTR)record.connectionId,utf8(text),record.receivedAtMillis);
    if(App.m_pMainWnd && Frame.GetSafeHwnd()) Frame.AppendWorkspaceRecord("Output",record);
    }
  return routedOmit;
}
WorkspacePublishGuard::WorkspacePublishGuard(CMUSHclientDoc* doc,bool received)
  :doc_(doc),received_(received),published_(false),
   sourceCookie_(doc->m_pCurrentLine?doc->m_pCurrentLine->m_workspaceCookie:0)
{
  try {
    if(WorkspaceConnectionId(doc_).IsEmpty()){published_=true;return;}
    pendingSourceLines[doc_].insert(sourceCookie_);activeSourceLines[doc_].insert(sourceCookie_);
  }catch(...) {
    auto pending=pendingSourceLines.find(doc_);if(pending!=pendingSourceLines.end()){pending->second.erase(sourceCookie_);if(pending->second.empty())pendingSourceLines.erase(pending);}
    auto active=activeSourceLines.find(doc_);if(active!=activeSourceLines.end()){active->second.erase(sourceCookie_);if(active->second.empty())activeSourceLines.erase(active);}
    sourceCookie_=0;published_=true;
  }
}
WorkspacePublishGuard::~WorkspacePublishGuard()
{
  try {
    if(!published_ && identities.count(doc_)) {
      POSITION source=sourcePosition(doc_,sourceCookie_);
      if(source) {
        CLine* line=doc_->m_LineList.GetAt(source);
        auto active=activeSourceLines.find(doc_);
        if((line->flags&NOTE_OR_COMMAND) && active!=activeSourceLines.end() && !active->second.empty() && *active->second.begin()<sourceCookie_) {
          auto& deferred=deferredLocalLines[doc_];deferred.insert(sourceCookie_);
          while(deferred.size()>10000)deferred.erase(deferred.begin());
        } else WorkspacePublishRecord(doc_,captureAt(doc_,source),received_ && doc_->m_bLineOmittedFromOutput);
      }
    }
  }catch(...) { /* Optional workspace must not interrupt the existing receive path. */ }
  auto pending=pendingSourceLines.find(doc_);if(pending!=pendingSourceLines.end()){pending->second.erase(sourceCookie_);if(pending->second.empty())pendingSourceLines.erase(pending);}
  auto active=activeSourceLines.find(doc_);if(active!=activeSourceLines.end()){active->second.erase(sourceCookie_);if(active->second.empty())activeSourceLines.erase(active);}
  if(!activeSourceLines.count(doc_) && identities.count(doc_)) {
    auto deferred=deferredLocalLines.find(doc_);
    if(deferred!=deferredLocalLines.end()) {
      auto cookies=std::move(deferred->second);deferredLocalLines.erase(deferred);
      for(auto cookie:cookies)try {POSITION source=sourcePosition(doc_,cookie);if(source)WorkspacePublishRecord(doc_,captureAt(doc_,source),false);}catch(...){}
    }
  }
}
bool WorkspacePublishGuard::PublishBeforeOmit()
{
  if(published_) return false;published_=true;
  if(!identities.count(doc_))return false;
  POSITION source=sourcePosition(doc_,sourceCookie_);
  if(!source){auto pending=pendingSourceLines.find(doc_);if(pending!=pendingSourceLines.end()){pending->second.erase(sourceCookie_);if(pending->second.empty())pendingSourceLines.erase(pending);}return false;}
  record_=captureAt(doc_,source); // Original retained record, including trigger-adjusted styles.
  bool omit=false;try {omit=WorkspacePublishRecord(doc_,record_,received_ && doc_->m_bLineOmittedFromOutput);}catch(...){}
  auto pending=pendingSourceLines.find(doc_);if(pending!=pendingSourceLines.end()){pending->second.erase(sourceCookie_);if(pending->second.empty())pendingSourceLines.erase(pending);}
  return omit;
}
void WorkspacePanePruned(const CString& connectionId,const CString& name,const std::deque<WorkspaceRecord>& retained)
{
  if(!liveDocument(connectionId) || name=="Output")return;
  const std::string id=std::string((LPCTSTR)connectionId)+"/pane/"+(LPCTSTR)name;
  ensurePane(connectionId,name,id);std::vector<agent_workspace::Line> history;
  // Incoming last record is appended normally after this callback, including capture delivery.
  for(size_t i=0;i+1<retained.size();++i){agent_workspace::Line row;row.text=recordText(retained[i]);row.receivedAt=retained[i].receivedAtMillis;history.push_back(row);}
  agentStore.replaceWindowHistory(id,history);
  paneLastSequence[id]=retained.empty()?0:retained.back().sequence-1;
}
void WorkspacePublishToPane(const CString& connectionId,const CString& name,const WorkspaceRecord& record)
{
  if(!liveDocument(connectionId))return;
  const std::string id=std::string((LPCTSTR)connectionId)+"/pane/"+(LPCTSTR)name;
  if(record.sequence<=paneLastSequence[id])return;
  ensurePane(connectionId,name,id);agentStore.append(id,recordText(record),record.receivedAtMillis);
  paneLastSequence[id]=record.sequence;
}
HWND WorkspaceOpenDraft(HWND parent,const agent_workspace::Draft& draft)
{
  PennEditorSetTransport(WorkspaceSendEditorRead);PennEditorSetSendPrepared(WorkspaceSendPreparedEditor);
  const bool scene=agentStore.hasSceneBinding(draft.id);
  if(scene && !agentStore.validateStagedDraft(draft.id,draft.text))throw std::runtime_error("Scene draft has expired or has already been queued.");
  std::string endpoint=WorkspaceEndpointForWindow(draft.window);
  if(endpoint.empty())throw std::runtime_error("Draft destination no longer exists.");
  HWND editor=OpenPennEditor(parent,endpoint,draft.text,WorkspaceWorldName(endpoint.c_str()));
  if(scene){PennEditorInfo info;if(!PennEditorSnapshotInfo(editor,info))throw std::runtime_error("Could not create the scene review document.");WorkspaceBindSceneDraft(info.documentID,draft.id);}
  return editor;
}
void WorkspaceReviewInput(const CString& connectionId,const std::wstring& command)
{
  PennEditorSetTransport(WorkspaceSendEditorRead);PennEditorSetSendPrepared(WorkspaceSendPreparedEditor);
  OpenPennEditor(Frame.GetSafeHwnd(),(LPCTSTR)connectionId,utf8(command),WorkspaceWorldName(connectionId));
}
std::string WorkspaceWorldName(const CString& connectionId)
{
  CMUSHclientDoc* doc=liveDocument(connectionId);
  return doc?utf8(wide(doc->m_mush_name,doc->m_mush_name.GetLength(),false)):std::string();
}
bool WorkspaceIsConnectionLive(const CString& connectionId)
{
  CMUSHclientDoc* doc=liveDocument(connectionId);
  return doc && doc->m_iConnectPhase==eConnectConnectedToMud;
}
std::wstring WorkspaceHumanInputError(const CString& connectionId)
{
  if(!liveDocument(connectionId))return L"Original world is closed; draft was not submitted.";
  auto error=humanInputErrors.find(connectionId);
  return error==humanInputErrors.end()?L"Not submitted; draft preserved.":error->second;
}
bool WorkspaceSendHumanInput(const CString& connectionId,const std::wstring& command)
{
  // This callback is installed only on human-operated transcript input controls.
  // Agent/code/scene proposals retain their separate reviewed-send path.
  CMUSHclientDoc* doc=liveDocument(connectionId);
  if(!doc)return false;
  auto reject=[&](const wchar_t* message){humanInputErrors[connectionId]=message;return false;};
  if(doc->m_iConnectPhase!=eConnectConnectedToMud)
    return reject(L"Original world disconnected; draft preserved.");
  if(command.empty() || command.size()>4096)
    return reject(L"Enter a command within the 4096-byte limit; draft preserved.");
  for(wchar_t c:command)if(c<32 || c==127)
    return reject(L"One-line input cannot contain control characters; draft preserved.");
  int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,command.data(),(int)command.size(),NULL,0,NULL,NULL);
  if(!bytes || bytes>4096)return reject(L"Input is invalid Unicode or exceeds 4096 UTF-8 bytes; draft preserved.");
  std::string source(bytes,'\0');
  if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,command.data(),(int)command.size(),&source[0],bytes,NULL,NULL))
    return reject(L"Input contains invalid Unicode; draft preserved.");
  std::string encoded;
  if(!encodeCommand(doc,source,encoded))
    return reject(L"This world's text encoding cannot represent the command; draft preserved.");
  struct HumanAction {
    CMUSHclientDoc* doc;CString id;unsigned short previous;
    HumanAction(CMUSHclientDoc* value,const CString& endpoint):doc(value),id(endpoint),previous(value->m_iCurrentActionSource)
      {doc->m_iCurrentActionSource=eUserTyping;}
    ~HumanAction(){if(liveDocument(id)==doc)doc->m_iCurrentActionSource=previous;}
  } action(doc,connectionId);
  try {
    // Execute on the captured document, never the currently focused world.
    // Preserve its alias/script/command handling and the main input draft.
    CString entered(encoded.c_str());
    doc->SendToAllPluginCallbacksRtn(ON_PLUGIN_COMMAND_ENTERED,entered);
    if(liveDocument(connectionId)!=doc)return false;
    if(doc->m_iConnectPhase!=eConnectConnectedToMud)
      return reject(L"Original world disconnected during command processing; draft preserved.");
    if(entered=="\r" || entered=="\t")
      return reject(L"A world plugin declined this command; draft preserved.");
    const long result=doc->Execute(entered);
    // Command callbacks may close a world. Do not dereference it afterward.
    if(!liveDocument(connectionId))return false;
    if(result!=eOK)return reject(L"Original world did not accept the command; draft preserved.");
    humanInputErrors.erase(connectionId);
    return true; // Accepted locally; this is not a server execution receipt.
  }catch(CException* error){error->Delete();}
  catch(...){ }
  if(!liveDocument(connectionId))return false;
  return reject(L"Submission interrupted; check world output before retrying.");
}
bool WorkspaceSendEditorRead(const std::string& endpoint,const std::string& command)
{
  static const std::regex allowed("^think [0-9a-f]{32} \\[(encode64\\(get\\(#[0-9]+/[A-Za-z_][A-Za-z0-9_.-]*\\)\\)|functions\\(\\))\\]$");
  if(command.size()>1024 || !std::regex_match(command,allowed)) return false;
  for(auto it=identities.begin();it!=identities.end();++it)
    if(endpoint==(LPCTSTR)it->second.id && it->first->m_iConnectPhase==eConnectConnectedToMud)
      {it->first->SendMsg(command.c_str(),false,false,false);return true;}
  return false;
}
void WorkspaceBindSceneDraft(unsigned __int64 documentID,const std::string& draftId)
{
  std::set<unsigned __int64> open;for(const auto& editor:PennEditorListSnapshots())open.insert(editor.documentID);
  for(auto it=sceneDraftBindings.begin();it!=sceneDraftBindings.end();)if(!open.count(it->first))it=sceneDraftBindings.erase(it);else ++it;
  sceneDraftBindings[documentID]=draftId;
}
bool WorkspaceSendPreparedEditor(const std::string& endpoint,unsigned __int64 documentID,unsigned __int64,const std::vector<std::string>& commands)
{
  auto binding=sceneDraftBindings.find(documentID);
  if(binding!=sceneDraftBindings.end() && (commands.size()!=1 || !agentStore.validateStagedDraft(binding->second,commands[0])))return false;
  if(commands.empty() || commands.size()>128)return false;
  size_t bytes=0;
  for(const auto& command:commands)
    {
    bytes+=command.size();if(command.empty() || command.size()>4096 || bytes>65536)return false;
    for(unsigned char c:command)if(c<32 || c==127)return false;
    }
  for(auto it=identities.begin();it!=identities.end();++it)
    if(endpoint==(LPCTSTR)it->second.id && it->first->m_iConnectPhase==eConnectConnectedToMud)
      {
      CMUSHclientDoc* doc=it->first;
      std::vector<std::string> encoded;
      for(const auto& command:commands){std::string line;if(!encodeCommand(doc,command,line))return false;encoded.push_back(line);}
      size_t commandIndex=0;
      for(const auto& command:commands)
        {
        // Validate connection before every line; SendMsg may run Lua callbacks.
        auto live=identities.find(doc);
        if(live==identities.end() || endpoint!=(LPCTSTR)live->second.id || doc->m_iConnectPhase!=eConnectConnectedToMud)return false;
        if(binding!=sceneDraftBindings.end() && !agentStore.validateStagedDraft(binding->second,command))return false;
        const auto sent=doc->m_nTotalLinesSent;const auto queued=doc->m_QueuedCommandsList.GetCount();
        doc->SendMsg(encoded[commandIndex++].c_str(),true,false,false);
        live=identities.find(doc);
        if(live==identities.end())return false;
        if(doc->m_nTotalLinesSent==sent && doc->m_QueuedCommandsList.GetCount()<=queued)return false;
        }
      if(binding!=sceneDraftBindings.end())agentStore.markStagedQueued(binding->second);
      return true;
      }
  return false;
}
std::string WorkspaceEndpointForWindow(const std::string& windowId)
{
  for(const auto& item:identities)
    {
    std::string endpoint=(LPCTSTR)item.second.id;
    if(windowId==endpoint || windowId.compare(0,endpoint.size()+6,endpoint+"/pane/")==0)return endpoint;
    }
  return std::string();
}
void ActivateWorkspaceAction(const CString& connectionId,unsigned short actionType,const CString& action)
{
  if(action.IsEmpty()) return;
  CMUSHclientDoc* doc=NULL;
  for(auto it=identities.begin();it!=identities.end();++it)
    if(it->second.id==connectionId) {doc=it->first;break;}
  if(!doc) return;
  if(actionType==ACTION_HYPERLINK)
    {
    CString lower=action;lower.MakeLower();
    if(lower.Left(7)=="http://" || lower.Left(8)=="https://")
      ShellExecute(Frame.GetSafeHwnd(),"open",action,NULL,NULL,SW_SHOWNORMAL);
    return;
    }
  PennEditorSetTransport(WorkspaceSendEditorRead);PennEditorSetSendPrepared(WorkspaceSendPreparedEditor);
  // A transcript command link opens an unsent destination-bound editor.
  // Agent bridge operations never enter this human click callback.
  OpenPennEditor(Frame.GetSafeHwnd(),(LPCTSTR)connectionId,utf8(wide(action,action.GetLength(),doc->m_bUTF_8)),WorkspaceWorldName(connectionId));
}
