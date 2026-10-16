#pragma once

#include "InlineImage.h"
#pragma push_macro("Translate")
#undef Translate
#include <afxole.h>
#include <afxdockablepane.h>
#include <afxtabbedpane.h>
#pragma pop_macro("Translate")
#include "workspace/WorkspaceInputPolicy.h"
#include "workspace/WorkspaceSpawnPolicy.h"
namespace workspace_spell { struct Context; }

// Independently implemented native workspace; no external client code.
#define ID_WORKSPACE_TRANSCRIPT 59000
#define ID_WORKSPACE_BAR 0xE820

struct WorkspaceRun
{
  std::wstring text;  // UTF-16, native Unicode RichEdit surface
  COLORREF foreground, background;
  bool bold, italic, underline;
  bool defaultForeground=false, defaultBackground=false;
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
  bool hasSurfaceColors, bleedBackground;
  bool serverOutput = false, imageOnly = false;
  COLORREF surfaceForeground, surfaceBackground;
  std::vector<WorkspaceRun> runs;
  std::shared_ptr<CInlineImage> image;
  int imageWidth, imageHeight;
  CString imageAction;
  WorkspaceRecord() : sequence(0), receivedAtMillis(0), firstSourceCookie(0),hasSurfaceColors(false),bleedBackground(false),surfaceForeground(RGB(225,225,225)),surfaceBackground(RGB(25,25,28)),imageWidth(0), imageHeight(0) {}
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
  bool followingImages = false;
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
  bool FocusSearch(){if(!m_search.GetSafeHwnd())return false;m_searchVisible=true;LayoutChildren();m_search.SetFocus();return true;}
  // Display metadata never replaces the transcript's immutable connection ID.
  void SetPresentationIdentity(const std::wstring& game,const std::wstring& character);
  enum class Placement {Join,Left,Right,Above,Below};
  bool ArrangeBeside(CWorkspacePane& target,Placement placement);
  void ShowPlacementMenu();
  bool DetachToWindow();
  unsigned int UnreadCount(const CString& id,const CString& name) const;
  unsigned int MentionCount(const CString& id,const CString& name) const;
  bool SetPreferences(const CString& id,const CString& name,const workspace_spawn::Preferences& preferences);
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
  void SetSpellingContextProvider(std::function<workspace_spell::Context(const CString&)> provider){m_spellingContext=provider;}
  typedef std::function<bool(const CString&,workspace_spawn::Appearance&)> AppearanceHandler;
  void SetAppearanceHandler(const AppearanceHandler& handler) {m_appearanceHandler=handler;}
  bool GetAppearance(const CString& id,const CString& name,workspace_spawn::Appearance& appearance) const;
  typedef std::function<bool(CWorkspacePane&,bool)> SharedInputHandler;
  void SetSharedInputHandler(const SharedInputHandler& handler) { m_sharedInputHandler=handler;if(GetSafeHwnd())LayoutChildren(); }
  bool SharedInputIdentity(CString& connectionId,CString& name) const;
  HWND SharedInputWindow() const { return m_input.GetSafeHwnd(); }
  std::wstring SharedInputDestination() const;
  bool SelectSharedInput(bool focus=true);
  void SubmitSharedInput() { OnReviewInput(); }
  bool HasUnsentInput(const CString& connectionId) const;
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
    // Model-only pruning leaves the frozen native surface intact. This counts
    // the prefix of that surface whose records no longer exist in the model.
    size_t deferredRemoved;
    size_t retainedBytes;
    std::wstring inputDraft, inputPrefix;
    workspace_input::InputHistory history;
    bool paused;
    bool deferred;
    bool batchPending;
    ViewState batchView;
    unsigned int unread;
    unsigned int mentions = 0;
    workspace_spawn::Preferences preferences;
    workspace_spawn::Appearance worldAppearance,appearance;
    bool appearanceKnown=false;
    std::wstring foldedMention;
    int renderedWidth,connectionState;
    COLORREF surfaceForeground,surfaceBackground;
    Transcript():deferredRemoved(0),retainedBytes(0),paused(false),deferred(false),batchPending(false),unread(0),renderedWidth(0),connectionState(-1),surfaceForeground(RGB(225,225,225)),surfaceBackground(RGB(25,25,28)){}
  };
  std::vector<Transcript*> m_transcripts;
  CTabCtrl m_tabs;
  HWND m_toolWindow;
  CString m_toolTitle;
  ActionHandler m_actionHandler;
  InputHandler m_inputHandler;
  std::function<workspace_spell::Context(const CString&)> m_spellingContext;
  AppearanceHandler m_appearanceHandler;
  SharedInputHandler m_sharedInputHandler;
  CPoint m_inputPointerStart;
  bool m_inputPointerCancelled=true;
  static LRESULT CALLBACK TranscriptInputProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data);
  ConnectionHandler m_connectionHandler;
  InputErrorHandler m_inputErrorHandler;
  CStatic m_inputStatus;
  CStatic m_destination;
  CButton m_arrange,m_findToggle;
  bool m_searchVisible=false;
  std::wstring m_gameLabel,m_characterLabel;
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
  void RefreshAppearance(Transcript& transcript);
  void LayoutChildren();
  CSize MinimumSize() const;
  virtual void GetMinSize(CSize& size) const { size=MinimumSize(); }
  void SaveInputDraft();
  void UpdateTabLabel(size_t index);
  void MarkVisibleTranscriptRead();
  long RenderRecord(Transcript& transcript, const WorkspaceRecord& record,long insertAt=-1,bool track=true);
  void Rebuild(Transcript& transcript);
  void ReflowImages(Transcript& transcript,ViewState view);
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
  afx_msg void OnLButtonUp(UINT flags,CPoint point);
  afx_msg void OnReviewInput();
  afx_msg void OnFindNext();
  afx_msg void OnPause();
  afx_msg void OnArrange(){ShowPlacementMenu();}
  afx_msg void OnFindToggle(){m_searchVisible=!m_searchVisible;LayoutChildren();if(m_searchVisible)m_search.SetFocus();}
  afx_msg void OnTimer(UINT_PTR timer);
  afx_msg void OnLink(UINT id, NMHDR* hdr, LRESULT* result);
  afx_msg void OnTabChanged(NMHDR* hdr, LRESULT* result);
  DECLARE_MESSAGE_MAP()
};
