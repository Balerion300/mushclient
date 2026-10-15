#include "stdafx.h"
#include "WorkspacePane.h"
#include "tests/workspace/ModernFixture.h"
#include <fstream>

// Isolated MFC focus probe. Both top-level windows and both edit controls
// belong to this process; no desktop input or installed client is touched.
class FocusProbeApp : public CWinApp {
public:
  int result = 1;
  BOOL InitInstance() override {
    AfxOleInit();
    AfxInitRichEdit2();
    std::ofstream log("tests/qa-cycle1/focus_reactivation.txt");
    log.setf(std::ios::unitbuf);
    WorkspaceTestFrame* frame = new WorkspaceTestFrame;
    CWorkspacePane pane;
    log << "starting\n";
    bool ok = frame->Create(NULL, "Focus fixture", WS_OVERLAPPEDWINDOW,
                           CRect(40, 40, 1100, 750)) != FALSE;
    log << "frame=" << ok << "\n";
    if (!ok) { log << "frame creation failed\n"; return FALSE; }
    frame->EnableDocking(CBRS_ALIGN_ANY);
    ok = pane.Create(frame) != FALSE;
    log << "pane=" << ok << "\n";
    if (!ok) { log << "pane creation failed\n"; frame->DestroyWindow(); return FALSE; }
    pane.EnableDocking(CBRS_ALIGN_ANY);
    frame->DockControlBar(&pane);
    frame->ShowControlBar(&pane, TRUE, FALSE);
    ok = pane.OpenTranscript("fictional-alpha", "Chat");
    log << "transcript=" << ok << "\n";
    HWND channel = pane.GetDlgItem(2)->GetSafeHwnd();
    HWND main = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
      10, 10, 400, 32, frame->GetSafeHwnd(), (HMENU)9001,
      AfxGetInstanceHandle(), NULL);
    HWND other = ::CreateWindowExW(0, L"STATIC", L"Other fixture window",
      WS_OVERLAPPEDWINDOW, 100, 100, 250, 150, NULL, NULL,
      AfxGetInstanceHandle(), NULL);
    ok = ok && ::IsWindow(channel) && ::IsWindow(main) && ::IsWindow(other);
    log << "controls=" << ok << "\n";
    frame->ShowWindow(SW_SHOWNOACTIVATE);
    frame->RecalcLayout();
    ::ShowWindow(other, SW_SHOWNOACTIVATE);
    ::SetActiveWindow(frame->GetSafeHwnd());
    ::SetFocus(main);
    ::SetFocus(channel);
    const bool before = ::GetFocus() == channel;
    ::SetActiveWindow(other);
    const HWND focusWhileOther = ::GetFocus();
    ::SetActiveWindow(frame->GetSafeHwnd());
    const HWND restored = ::GetFocus();
    const bool after = restored == channel;
    wchar_t focusClass[128] = {};
    if (restored) ::GetClassNameW(restored, focusClass, 128);
    log << "channel-focus-before=" << before
        << " channel-focus-after=" << after
        << " main-focus-after=" << (restored == main)
        << " null-focus-after=" << (restored == NULL)
        << " focus-was-other=" << (focusWhileOther == other)
        << " focus-class-after=";
    for (const wchar_t* c = focusClass; *c; ++c) log << char(*c < 128 ? *c : '?');
    log << " focus-is-pane-child=" << ::IsChild(pane.GetSafeHwnd(), restored)
        << " active-frame-after=" << (::GetActiveWindow() == frame->GetSafeHwnd()) << "\n";
    ok=ok && before && after;
    auto check=[&](const char* label,bool passed){log<<(passed?"PASS ":"FAIL ")<<label<<"\n";ok=ok && passed;};
    ::SetFocus(channel);::SetFocus(frame->m_hWnd);
    check("direct focus on already active frame retains channel composer",::GetFocus()==channel);
    ::SetFocus(main);
    check("explicit different control focus wins after reactivation",::GetFocus()==main);
    ::SetFocus(channel);::SetActiveWindow(other);::EnableWindow(channel,FALSE);::SetActiveWindow(frame->m_hWnd);
    check("disabled remembered composer cannot receive focus",::GetFocus()!=channel);
    ::EnableWindow(channel,TRUE);::SetFocus(channel);::SetActiveWindow(other);
    ::SetParent(channel,other);::SetActiveWindow(frame->m_hWnd);
    check("remembered control moved to another top-level owner cannot receive focus",::GetFocus()!=channel);
    ::SetParent(channel,pane.m_hWnd);::SetFocus(channel);::SetActiveWindow(other);
    ::RemovePropW(channel,L"Valyria.WorkspaceFocusOwner");::SetActiveWindow(frame->m_hWnd);
    check("missing lifetime property rejects stale or replaced focus target",::GetFocus()!=channel);
    ::SetFocus(channel);::SetActiveWindow(other);::ShowWindow(channel,SW_HIDE);::SetActiveWindow(frame->m_hWnd);
    check("hidden remembered composer cannot receive focus",::GetFocus()!=channel);
    ::ShowWindow(channel,SW_SHOW);::SetFocus(channel);::SetActiveWindow(other);::SetActiveWindow(frame->m_hWnd);
    ::SendMessage(frame->m_hWnd,WM_ACTIVATE,WA_CLICKACTIVE,(LPARAM)other);
    check("mouse reactivation through non-input chrome retains composer",::GetFocus()==channel);::SetFocus(main);
    MSG pending;while(::PeekMessage(&pending,NULL,0,0,PM_REMOVE)){::TranslateMessage(&pending);::DispatchMessage(&pending);}
    check("explicit click target is not overwritten by delayed focus restoration",::GetFocus()==main);
    bool live=false;pane.SetConnectionHandler([&](const CString&){return live;});
    ::SendMessage(pane.m_hWnd,WM_TIMER,1,0);
    check("disconnect feedback is visible without a resize",::IsWindowVisible(pane.GetDlgItem(8)->m_hWnd) && !::IsWindowEnabled(pane.GetDlgItem(3)->m_hWnd));
    live=true;::SendMessage(pane.m_hWnd,WM_TIMER,1,0);
    check("reconnection feedback is visible and send reenabled",::IsWindowVisible(pane.GetDlgItem(8)->m_hWnd) && ::IsWindowEnabled(pane.GetDlgItem(3)->m_hWnd));
    frame->FloatWorkspacePane(pane,CRect(200,180,840,660));
    HWND floating=pane.GetParentMiniFrame()?pane.GetParentMiniFrame()->GetSafeHwnd():NULL;
    check("channel detaches into original native mini-frame",floating && ::IsChild(floating,channel));
    if(floating){
      ::ShowWindow(floating,SW_SHOWNOACTIVATE);::SetActiveWindow(floating);::SetFocus(channel);
      ::SetActiveWindow(other);::SetActiveWindow(floating);
      check("detached mini-frame activation retains its own composer",::GetFocus()==channel);
      ::SetFocus(channel);::SetFocus(floating);
      check("direct focus on detached mini-frame retains its own composer",::GetFocus()==channel);
      ::SetFocus(main);check("explicit main composer focus wins over detached bookmark",::GetFocus()==main);
      pane.DockToRecentPos();::SetActiveWindow(frame->m_hWnd);::SetFocus(channel);::SetFocus(frame->m_hWnd);
      check("redocked channel remains direct-frame-focus target",::GetFocus()==channel && ::IsChild(frame->m_hWnd,channel));
    }
    log << (ok ? "PASS" : "FAIL")
        << " original channel composer focus after native frame reactivation\n";
    result = ok ? 0 : 1;
    if (::IsWindow(other)) ::DestroyWindow(other);
    frame->DestroyWindow();
    return FALSE;
  }
};

FocusProbeApp app;
