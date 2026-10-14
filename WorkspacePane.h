#pragma once

#include "InlineImage.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxole.h>
#include <afxdockablepane.h>
#include <afxtabbedpane.h>
#pragma pop_macro("Translate")
#include "workspace/WorkspaceInputPolicy.h"

// Independently implemented native workspace; no external client code.
#define ID_WORKSPACE_TRANSCRIPT 59000
#define ID_WORKSPACE_BAR 0xE820

struct WorkspaceRun
{
  std::wstring text;  // UTF-16, native Unicode RichEdit surface
  COLORREF foreground, background;
  bool bold, italic, underline;
  unsigned short actionType;
  CString action;  // immutable action metadata; activation is destination-bound
  WorkspaceRun() : foreground(RGB(225,225,225)), background(RGB(25,25,28)),
    bold(false), italic(false), underline(false), actionType(0) {}
};

struct WorkspaceRecord
{
  unsigned __int64 sequence;
  unsigned __int64 receivedAtMillis;
  unsigned __int64 firstSourceCookie;
  CString connectionId;
  std::vector<WorkspaceRun> runs;
  std::shared_ptr<CInlineImage> image;
  int imageWidth, imageHeight;
  CString imageAction;
  WorkspaceRecord() : sequence(0), receivedAtMillis(0), firstSourceCookie(0),imageWidth(0), imageHeight(0) {}
};

struct WorkspacePaneLayout
{
  CString connectionId, name, groupConnectionId, groupName;
  CString splitConnectionId,splitName;
  DWORD splitAlignment;
  int splitDepth,splitPercent;
  int tabOrder;
  bool active;
  bool detached, floating;
  DWORD alignment;
  int x,y,width,height;
  WorkspacePaneLayout():splitAlignment(0),splitDepth(0),splitPercent(50),tabOrder(0),active(true),detached(false),floating(false),alignment(CBRS_ALIGN_RIGHT),x(80),y(120),width(360),height(250){}
};

struct WorkspaceRoute
{
  CString connectionId, transcriptName;
  std::wstring pattern;
  bool regex, omitFromMain;
  std::shared_ptr<void> compiled;
  WorkspaceRoute() : regex(false), omitFromMain(false) {}
  bool Compile(CString& error);
  bool Matches(const WorkspaceRecord& record) const;
};

class CWorkspacePane : public CDockablePane
{
  DECLARE_DYNAMIC(CWorkspacePane)
public:
  CWorkspacePane();
  virtual ~CWorkspacePane();
  BOOL Create(CFrameWnd* frame, UINT barId=ID_WORKSPACE_BAR);
  virtual CDockablePane* AttachToTabWnd(CDockablePane* target,AFX_DOCK_METHOD method,BOOL active=TRUE,CDockablePane** group=NULL);
  virtual BOOL FloatPane(CRect bounds,AFX_DOCK_METHOD method=DM_UNKNOWN,bool show=true);
  virtual LRESULT WindowProc(UINT message,WPARAM wp,LPARAM lp);
  CSize WorkspaceMinimumSize() const { return MinimumSize(); }
  bool MoveSelectedTo(CWorkspacePane& target);
  bool HostTool(HWND toolWindow, const CString& title);
  int CloseHostedTool(); // 0 transcript/empty, 1 closed tool, -1 child vetoed close
  bool IsToolHost() const { return m_toolWindow && ::IsWindow(m_toolWindow); }

  void GetLayouts(std::vector<WorkspacePaneLayout>& layouts,bool detached) const;
  void ApplyLayout(const WorkspacePaneLayout& layout);
  bool HasTranscript(const CString& id,const CString& name) const;
  bool RetainedSourceAfter(const CString& id,const CString& name,unsigned __int64 boundary) const;
  size_t TranscriptCount() const { return m_transcripts.size(); }
  bool FocusTranscript(size_t index);
  unsigned int UnreadCount(const CString& id,const CString& name) const;
  std::wstring InputPrefix(const CString& id,const CString& name) const;
  bool IsEmpty() const { return m_transcripts.empty(); }
  bool OpenTranscript(const CString& connectionId, const CString& name);
  bool AppendRecord(const CString& name, const WorkspaceRecord& record);
  void BeginRenderBatch() { ++m_renderBatchDepth; }
  void EndRenderBatch();
  unsigned RenderBatchDepth() const { return m_renderBatchDepth; }
  void CloseConnection(const CString& connectionId);
  bool ClearTranscript(const CString& connectionId,const CString& name);
  typedef std::function<void(const CString&, unsigned short, const CString&)> ActionHandler;
  typedef std::function<bool(const CString&, const std::wstring&)> InputHandler;
  typedef std::function<std::wstring(const CString&)> InputErrorHandler;
  void SetInputErrorHandler(const InputErrorHandler& handler) { m_inputErrorHandler=handler; }
  typedef std::function<bool(const CString&)> ConnectionHandler;
  void SetConnectionHandler(const ConnectionHandler& handler) { m_connectionHandler=handler; }
  typedef std::function<void(const CString&,const CString&,const std::deque<WorkspaceRecord>&)> PruneHandler;
  void SetPruneHandler(const PruneHandler& handler) { m_pruneHandler=handler; }
  void SetInputHandler(const InputHandler& handler) { m_inputHandler=handler; }
  bool SetInputPrefix(const CString& connectionId, const CString& name, const std::wstring& prefix);
  virtual BOOL PreTranslateMessage(MSG* message);
  void SetActionHandler(const ActionHandler& handler) { m_actionHandler=handler; }

  virtual void OnUpdateCmdUI(CFrameWnd*, BOOL) {}
protected:
  struct ViewState { long start,end,anchor;LONG anchorY;bool follow; };
  struct LinkRange { long first,last; unsigned short type; CString action; };
  struct Transcript
  {
    CString connectionId, name, groupConnectionId, groupName;
  CString splitConnectionId,splitName;
  DWORD splitAlignment;
  int splitDepth,splitPercent;
  int tabOrder;
  bool active;
    CRichEditCtrl text;
    std::deque<WorkspaceRecord> records;
    std::vector<LinkRange> links;
    std::vector<int> imageStates;
    std::deque<long> renderedEnds;
    size_t retainedBytes;
    std::wstring inputDraft, inputPrefix;
    workspace_input::InputHistory history;
    bool paused;
    bool deferred;
    bool batchPending;
    ViewState batchView;
    unsigned int unread;
    int renderedWidth,connectionState;
    Transcript():retainedBytes(0),paused(false),deferred(false),batchPending(false),unread(0),renderedWidth(0),connectionState(-1){}
  };
  std::vector<Transcript*> m_transcripts;
  CTabCtrl m_tabs;
  HWND m_toolWindow;
  CString m_toolTitle;
  ActionHandler m_actionHandler;
  InputHandler m_inputHandler;
  ConnectionHandler m_connectionHandler;
  InputErrorHandler m_inputErrorHandler;
  CStatic m_inputStatus;
  PruneHandler m_pruneHandler;
  CEdit m_input;
  CButton m_reviewInput;
  CEdit m_search;
  CButton m_findNext,m_pause;
  int m_selectedInput;
  bool m_labelDirty;
  unsigned m_renderBatchDepth;
  CSize m_floatSize;
  int m_dockWidth, m_dockHeight;
  bool m_resizing;
  CPoint m_resizeStart;
  int m_resizeOriginal;
  Transcript* Find(const CString& connectionId, const CString& name);
  void LayoutChildren();
  CSize MinimumSize() const;
  virtual void GetMinSize(CSize& size) const { size=MinimumSize(); }
  void SaveInputDraft();
  void UpdateTabLabel(size_t index);
  long RenderRecord(Transcript& transcript, const WorkspaceRecord& record,long insertAt=-1,bool track=true);
  void Rebuild(Transcript& transcript);
  long TrimRenderedPrefix(Transcript& transcript,size_t records);
  void RefreshDeferred();
  afx_msg void OnShowWindow(BOOL show,UINT status);
  static long TextLength(const CRichEditCtrl& control);
  static bool ShouldFollow(const CRichEditCtrl& control);
  static ViewState SaveView(CRichEditCtrl& control);
  static void RestoreView(CRichEditCtrl& control,const ViewState& state);
  long RenderImage(Transcript& transcript, const WorkspaceRecord& record,long insertAt);
  long ReplaceRenderedRecord(Transcript& transcript,size_t index);
  afx_msg int OnCreate(LPCREATESTRUCT cs);
  afx_msg void OnSize(UINT type, int cx, int cy);
  afx_msg void OnPaint();
  afx_msg void OnSetFocus(CWnd* oldWindow);
  afx_msg void OnReviewInput();
  afx_msg void OnFindNext();
  afx_msg void OnPause();
  afx_msg void OnTimer(UINT_PTR timer);
  afx_msg void OnLink(UINT id, NMHDR* hdr, LRESULT* result);
  afx_msg void OnTabChanged(NMHDR* hdr, LRESULT* result);
  DECLARE_MESSAGE_MAP()
};
