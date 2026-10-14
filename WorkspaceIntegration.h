#pragma once
#include "WorkspacePane.h"
#include "AgentWorkspaceCore.h"
class CMUSHclientDoc;
CString WorkspaceConnectionId(CMUSHclientDoc* doc);
agent_workspace::Store& WorkspaceAgentStore();
void WorkspaceCloseDocument(CMUSHclientDoc* doc);
void WorkspaceClearOutput(CMUSHclientDoc* doc);
void WorkspaceRebuildOutput(CMUSHclientDoc* doc);
void WorkspacePanePruned(const CString& connectionId,const CString& name,const std::deque<WorkspaceRecord>& retained);
void WorkspaceExpireConnection(CMUSHclientDoc* doc);
void ActivateWorkspaceAction(const CString& connectionId,unsigned short actionType,const CString& action);
WorkspaceRecord WorkspaceCaptureRecord(CMUSHclientDoc* doc);
bool WorkspacePublishRecord(CMUSHclientDoc* doc, const WorkspaceRecord& record, bool omitted);
void WorkspacePublishToPane(const CString& connectionId,const CString& name,const WorkspaceRecord& record);
HWND WorkspaceOpenDraft(HWND parent,const agent_workspace::Draft& draft);
void WorkspaceReviewInput(const CString& connectionId,const std::wstring& command);
bool WorkspaceSendEditorRead(const std::string& endpoint,const std::string& command);
bool WorkspaceSendPreparedEditor(const std::string& endpoint,unsigned __int64 documentID,unsigned __int64 revision,const std::vector<std::string>& commands);
void WorkspaceBindSceneDraft(unsigned __int64 documentID,const std::string& draftId);
std::string WorkspaceEndpointForWindow(const std::string& windowId);
class WorkspacePublishGuard
{
  CMUSHclientDoc* doc_; WorkspaceRecord record_; bool received_,published_;
  unsigned __int64 sourceCookie_;
public:
  WorkspacePublishGuard(CMUSHclientDoc* doc, bool received);
  ~WorkspacePublishGuard();
  bool PublishBeforeOmit();
};
