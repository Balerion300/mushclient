#include "stdafx.h"
#include "workspace/WorkspaceComposerSplitter.h"
#include "workspace/WorkspaceViewLease.h"
#include <fstream>

class QaDocument : public CDocument {
public:
  ~QaDocument() override {}
};

class QaFrame : public CFrameWnd {
public:
  CWorkspaceComposerSplitter splitter;
  CEditView* input = NULL;
  int* sent = NULL;
  bool closeOnSend = false;
  BOOL OnCreateClient(LPCREATESTRUCT, CCreateContext* supplied) override {
    CCreateContext context = {};
    if (supplied) context.m_pCurrentDoc = supplied->m_pCurrentDoc;
    if (!splitter.CreateStatic(this, 2, 1) ||
        !splitter.CreateView(0, 0, RUNTIME_CLASS(CEditView), CSize(400, 400), &context) ||
        !splitter.CreateView(1, 0, RUNTIME_CLASS(CEditView), CSize(400, 100), &context)) return FALSE;
    input = static_cast<CEditView*>(splitter.GetPane(1, 0));
    return splitter.AttachComposer(1, input->m_hWnd, [this]() {
      if (sent) ++*sent;
      if (closeOnSend) DestroyWindow();
    });
  }
};

class QaApp : public CWinApp {
  int result_ = 1;
  BOOL InitInstance() override {
    AfxOleInit();
    std::ofstream out("composer_qa.txt");
    out.setf(std::ios::unitbuf);
    bool ok = true;
    auto check = [&](const char* name, bool passed) {
      out << (passed ? "PASS " : "FAIL ") << name << "\n";
      ok = ok && passed;
    };
    int sent = 0;
    QaFrame* frame = new QaFrame;
    frame->sent = &sent;
    check("frame and exact original edit view exist",
      frame->Create(NULL, "QA Main", WS_OVERLAPPEDWINDOW, CRect(50, 50, 700, 550)) &&
      frame->input && frame->splitter.GetPane(1, 0) == frame->input);
    m_pMainWnd = frame;
    frame->ShowWindow(SW_SHOWNOACTIVATE);
    HWND editor = frame->input->m_hWnd;
    HWND label = ::GetDlgItem(frame->splitter.m_hWnd, 0x7b21);
    HWND send = ::GetDlgItem(frame->splitter.m_hWnd, 0x7b20);
    ::SetWindowTextW(editor, L"unsent QA draft");
    ::SendMessageW(editor, EM_SETSEL, 2, 8);
    const LRESULT selection = ::SendMessageW(editor, EM_GETSEL, 0, 0);
    bool geometry = true;
    for (int width : {100, 140, 180, 260, 650}) {
      frame->SetWindowPos(NULL, 0, 0, width, 550, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      frame->RecalcLayout();
      RECT outer, frameClient, labelRect, sendRect, editRect;
      ::GetWindowRect(frame->splitter.m_hWnd, &outer);
      ::GetClientRect(frame->m_hWnd, &frameClient);
      ::MapWindowPoints(frame->m_hWnd, NULL, (POINT*)&frameClient, 2);
      ::GetWindowRect(label, &labelRect);
      ::GetWindowRect(send, &sendRect);
      ::GetWindowRect(editor, &editRect);
      const bool fits = outer.left >= frameClient.left && outer.right <= frameClient.right &&
        labelRect.left >= outer.left && sendRect.right <= outer.right &&
        labelRect.right <= sendRect.left && sendRect.bottom <= editRect.top &&
        editRect.left >= outer.left && editRect.right <= outer.right;
      out << "width=" << width << " frameClientWidth=" << frameClient.right-frameClient.left
          << " splitterWidth=" << outer.right-outer.left << " labelRight=" << labelRect.right
          << " sendLeft=" << sendRect.left << " editWidth=" << editRect.right-editRect.left
          << " fits=" << fits << "\n";
      geometry = geometry && fits;
    }
    check("composer label Send and original edit remain disjoint and inside narrow frames", geometry);
    wchar_t draft[128] = {};
    ::GetWindowTextW(editor, draft, 128);
    check("layout preserves original HWND, draft and selection",
      frame->splitter.GetPane(1, 0)->m_hWnd == editor &&
      !wcscmp(draft, L"unsent QA draft") &&
      ::SendMessageW(editor, EM_GETSEL, 0, 0) == selection);
    frame->SetWindowPos(NULL, 0, 0, 650, 550, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    frame->RecalcLayout();
    frame->closeOnSend = true;
    ::SendMessageW(send, BM_CLICK, 0, 0);
    check("synchronous Send callback closes its owning frame once", sent == 1 && !::IsWindow(editor));

    // One document can own two frames via New Window. Both Main destinations
    // must refresh; enumerating only the first view leaves the second stale.
    QaDocument document;
    document.m_bAutoDelete = FALSE;
    CCreateContext shared = {};
    shared.m_pCurrentDoc = &document;
    QaFrame* first = new QaFrame;
    QaFrame* second = new QaFrame;
    check("two Main frames share one document",
      first->Create(NULL, "QA Alpha", WS_OVERLAPPEDWINDOW,
                    CRect(50, 50, 700, 550), NULL, NULL, 0, &shared) &&
      second->Create(NULL, "QA Beta", WS_OVERLAPPEDWINDOW,
                     CRect(70, 70, 720, 570), NULL, NULL, 0, &shared) &&
      first->input->GetDocument() == &document &&
      second->input->GetDocument() == &document);
    int visited = 0;
    workspace_composer::ForEachOriginalFrame(document, [&](CFrameWnd& owner) {
      ++visited;
      QaFrame* target = static_cast<QaFrame*>(&owner);
      target->splitter.SetComposerDestination(L"Harbor", L"Alpha", false);
    });
    wchar_t firstLabel[128] = {}, secondLabel[128] = {};
    HWND firstSend = ::GetDlgItem(first->splitter.m_hWnd, 0x7b20);
    HWND secondSend = ::GetDlgItem(second->splitter.m_hWnd, 0x7b20);
    ::GetWindowTextW(::GetDlgItem(first->splitter.m_hWnd, 0x7b21), firstLabel, 128);
    ::GetWindowTextW(::GetDlgItem(second->splitter.m_hWnd, 0x7b21), secondLabel, 128);
    check("multi-child refresh reaches each original frame once and updates offline status",
      visited == 2 && !wcscmp(firstLabel, L"To Alpha / Harbor / Main  - Offline") &&
      !wcscmp(secondLabel, firstLabel) &&
      !::IsWindowEnabled(firstSend) && !::IsWindowEnabled(secondSend));
    WorkspaceDocumentLease documentLease(&document);
    {
      WorkspaceMainSubmission firstSubmission(&document);
      WorkspaceMainSubmission nestedSubmission(second->input->GetDocument());
      check("New Window cannot submit the same document recursively",
        firstSubmission.Acquired() && !nestedSubmission.Acquired() &&
        WorkspaceMainSubmission::IsActive(&document));
    }
    WorkspaceMainSubmission afterCallback(&document);
    check("document submission guard releases after callback", afterCallback.Acquired());
    check("document lease is live while both frames exist", documentLease.IsLive());
    out << "destroying first shared-document frame\n";
    first->DestroyWindow();
    check("document lease survives closing one New Window frame", documentLease.IsLive());
    out << "destroying second shared-document frame\n";
    second->DestroyWindow();
    check("document lease expires when last original view closes", !documentLease.IsLive());
    out << "shared-document frames destroyed\n";
    QaDocument* closedDuringSubmission = new QaDocument;
    {
      WorkspaceMainSubmission closeGuard(closedDuringSubmission);
      delete closedDuringSubmission;
    }
    check("submission scope releases a closed document without dereference",
      !WorkspaceMainSubmission::IsActive(closedDuringSubmission));
    out << (ok ? "PASS" : "FAIL") << " independent Main composer QA\n";
    result_ = ok ? 0 : 1;
    return FALSE;
  }
  int ExitInstance() override { CWinApp::ExitInstance(); return result_; }
} app;
