#include "stdafx.h"
#include "MUSHclient.h"
#include "WorkspaceRouteManager.h"
#include "WorkspaceIntegration.h"
#include "doc.h"
#include "mainfrm.h"
#include "workspace/WorkspaceInputPolicy.h"
#include <algorithm>
namespace {
std::wstring RouteWide(const char *value) {
  if (!value)
    return std::wstring();
  size_t length = strnlen(value, 4097);
  if (length > 4096)
    return std::wstring();
  int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                                  static_cast<int>(length), NULL, 0);
  std::wstring result(count, L'\0');
  if (count)
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value,
                        static_cast<int>(length), &result[0], count);
  return result;
}
std::wstring WorldLabel(const char *value) {
  auto label = RouteWide(value);
  if (!label.empty() || !value || !*value)
    return label;
  size_t length = strnlen(value, 4097);
  if (length > 4096)
    return L"World";
  int count =
      MultiByteToWideChar(CP_ACP, 0, value, static_cast<int>(length), NULL, 0);
  label.assign(count, L'\0');
  if (count)
    MultiByteToWideChar(CP_ACP, 0, value, static_cast<int>(length), &label[0],
                        count);
  return label.empty() ? L"World" : label;
}
std::wstring Read(HWND window, unsigned maximum) {
  int size = ::GetWindowTextLengthW(window);
  if (size < 0 || static_cast<unsigned>(size) > maximum)
    throw std::runtime_error("A route field exceeds its character limit.");
  std::wstring result(size + 1, L'\0');
  ::GetWindowTextW(window, &result[0], size + 1);
  result.resize(size);
  return result;
}
bool RouteUtf8(const std::wstring &text, std::string &result) {
  if (text.empty()) {
    result.clear();
    return true;
  }
  int size =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), NULL, 0, NULL, NULL);
  if (!size)
    return false;
  result.assign(size, '\0');
  return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                             static_cast<int>(text.size()), &result[0], size,
                             NULL, NULL) != 0;
}
} // namespace
// This modeless tool retains its original connection regardless of world focus.
class CWorkspaceRouteDialog : public CWnd {
  CMainFrame *frame;
  CString identity;
  CEdit name, pattern, prefix;
  CListBox routes;
  CButton regex, omit, save, cancel, newRoute, removeRoute, channelExample,
      tellExample;
  CStatic nameLabel, patternLabel, prefixLabel, destination, hint, feedback;
  CFont font;
  std::vector<WorkspaceRoute> visibleRoutes;
  int editing;
  UINT dpi;
  int Scale(int value) const { return MulDiv(value, dpi, 96); }
  void Feedback(const std::wstring &value) {
    ::SetWindowTextW(feedback.m_hWnd, value.c_str());
  }
  void Font() {
    font.DeleteObject();
    LOGFONT info = {};
    GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(info), &info);
    info.lfHeight = -MulDiv(9, dpi, 72);
    font.CreateFontIndirect(&info);
    for (HWND child = ::GetWindow(m_hWnd, GW_CHILD); child;
         child = ::GetWindow(child, GW_HWNDNEXT))
      ::SendMessage(child, WM_SETFONT, (WPARAM)font.GetSafeHandle(), TRUE);
  }
  void Destination() {
    CString world = "Closed world";
    bool found = false;
    POSITION position = App.m_pWorldDocTemplate->GetFirstDocPosition();
    while (position) {
      CMUSHclientDoc *doc =
          (CMUSHclientDoc *)App.m_pWorldDocTemplate->GetNextDoc(position);
      if (WorkspaceConnectionId(doc) == identity) {
        world = doc->m_mush_name;
        found = true;
        break;
      }
    }
    std::wstring label = L"Original world: " + WorldLabel((LPCTSTR)world) +
                         (WorkspaceIsConnectionLive(identity)
                              ? L" (connected)"
                              : L" (disconnected; drafts stay local)");
    ::SetWindowTextW(destination.m_hWnd, label.c_str());
    save.EnableWindow(found);
    channelExample.EnableWindow(found);
    tellExample.EnableWindow(found);
  }
  void Layout() {
    if (!routes.GetSafeHwnd())
      return;
    CRect client;
    GetClientRect(client);
    int pad = Scale(12), width = max(120, client.Width() - 2 * pad),
        listHeight = max(Scale(80), client.Height() - Scale(485));
    int y = pad;
    destination.MoveWindow(pad, y, width, Scale(30));
    y += Scale(34);
    routes.MoveWindow(pad, y, width, listHeight);
    y += listHeight + Scale(8);
    int button = max(60, (width - 3 * Scale(6)) / 4);
    CWnd *actions[] = {&newRoute, &removeRoute, &channelExample, &tellExample};
    for (int i = 0; i < 4; ++i)
      actions[i]->MoveWindow(pad + i * (button + Scale(6)), y, button,
                             Scale(28));
    y += Scale(38);
    nameLabel.MoveWindow(pad, y, width, Scale(22));
    y += Scale(24);
    name.MoveWindow(pad, y, width, Scale(26));
    y += Scale(34);
    patternLabel.MoveWindow(pad, y, width, Scale(22));
    y += Scale(24);
    pattern.MoveWindow(pad, y, width, Scale(26));
    y += Scale(34);
    prefixLabel.MoveWindow(pad, y, width, Scale(22));
    y += Scale(24);
    prefix.MoveWindow(pad, y, width, Scale(26));
    y += Scale(34);
    regex.MoveWindow(pad, y, width, Scale(26));
    y += Scale(30);
    omit.MoveWindow(pad, y, width, Scale(32));
    y += Scale(36);
    hint.MoveWindow(pad, y, width, Scale(44));
    y += Scale(48);
    save.MoveWindow(pad, y, max(90, width - Scale(112)), Scale(30));
    cancel.MoveWindow(pad + width - Scale(104), y, Scale(104), Scale(30));
    y += Scale(36);
    feedback.MoveWindow(pad, y, width,
                        max(Scale(32), client.Height() - y - pad));
  }
  void RefreshRoutes() {
    visibleRoutes.clear();
    routes.ResetContent();
    const std::vector<WorkspaceRoute> &all = frame->WorkspaceRoutes();
    for (size_t i = 0; i < all.size(); ++i)
      if (all[i].connectionId == identity) {
        visibleRoutes.push_back(all[i]);
        std::wstring label =
            RouteWide((LPCTSTR)all[i].transcriptName) + L" | " +
            (all[i].regex ? L"regex: " : L"literal: ") + all[i].pattern +
            (all[i].omitFromMain ? L" | hidden from Main"
                                 : L" | copied from Main");
        ::SendMessageW(routes.m_hWnd, LB_ADDSTRING, 0, (LPARAM)label.c_str());
      }
    editing = -1;
    removeRoute.EnableWindow(FALSE);
    save.SetWindowText("Create route");
    Destination();
  }

public:
  CWorkspaceRouteDialog(CMainFrame *owner, const CString &id)
      : frame(owner), identity(id), editing(-1), dpi(96) {}
  BOOL Open() {
    return CreateEx(WS_EX_DLGMODALFRAME,
                    AfxRegisterWndClass(0, LoadCursor(NULL, IDC_ARROW),
                                        (HBRUSH)(COLOR_BTNFACE + 1)),
                    "Output routes and spawn input",
                    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                        WS_MINIMIZEBOX,
                    CRect(200, 100, 860, 800), frame, 0);
  }
  virtual void PostNcDestroy() { delete this; }
  virtual BOOL PreTranslateMessage(MSG *message) {
    if (message->message == WM_KEYDOWN) {
      if (message->wParam == VK_ESCAPE) {
        OnClose();
        return TRUE;
      }
      if (message->wParam == VK_RETURN) {
        wchar_t focusedClass[32] = {0};
        ::GetClassNameW(message->hwnd, focusedClass, 32);
        if (!_wcsicmp(focusedClass, L"Button")) {
          ::SendMessageW(message->hwnd, BM_CLICK, 0, 0);
          return TRUE;
        }
        OnSave();
        return TRUE;
      }
      if (message->wParam == VK_DELETE && message->hwnd == routes.m_hWnd) {
        OnDelete();
        return TRUE;
      }
    }
    return CWnd::PreTranslateMessage(message);
  }

protected:
  afx_msg int OnCreate(LPCREATESTRUCT cs) {
    if (CWnd::OnCreate(cs) == -1)
      return -1;
    SetPropW(GetSafeHwnd(), L"MUSHclientWorkspace", (HANDLE)1);
    SetPropW(GetSafeHwnd(), L"MUSHclientRouteManager", (HANDLE)1);
    auto getDpi = (UINT(WINAPI *)(HWND))GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (getDpi)
      dpi = getDpi(m_hWnd);
    HWND list = ::CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                      WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY,
                                  0, 0, 1, 1, m_hWnd, (HMENU)7,
                                  AfxGetInstanceHandle(), NULL);
    if (!list || !routes.Attach(list))
      return -1;
    newRoute.Create("New", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    CRect(0, 0, 1, 1), this, 8);
    removeRoute.Create("Delete",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(0, 0, 1, 1), this, 9);
    channelExample.Create("Channel example",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                          CRect(0, 0, 1, 1), this, 11);
    tellExample.Create("Tell example",
                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                       CRect(0, 0, 1, 1), this, 12);
    nameLabel.Create("Transcript name", WS_CHILD | WS_VISIBLE,
                     CRect(0, 0, 1, 1), this);
    patternLabel.Create("Incoming text to match (case-sensitive unless your "
                        "regex says otherwise)",
                        WS_CHILD | WS_VISIBLE, CRect(0, 0, 1, 1), this);
    prefixLabel.Create(
        "Outgoing command prefix (include the needed trailing space or =)",
        WS_CHILD | WS_VISIBLE, CRect(0, 0, 1, 1), this);
    destination.Create("Original world", WS_CHILD | WS_VISIBLE,
                       CRect(0, 0, 1, 1), this);
    hint.Create("Blank prefix: type a complete game command. Enter submits "
                "human input to this original world; agent/editor proposals "
                "require their separate review.",
                WS_CHILD | WS_VISIBLE, CRect(0, 0, 1, 1), this);
    feedback.Create("Choose New or select an existing rule to edit it.",
                    WS_CHILD | WS_VISIBLE, CRect(0, 0, 1, 1), this);
    CEdit *edits[] = {&name, &pattern, &prefix};
    int ids[] = {1, 2, 10};
    for (int i = 0; i < 3; ++i) {
      HWND input = ::CreateWindowExW(
          WS_EX_CLIENTEDGE, L"EDIT", i == 0 ? L"Tells" : L"",
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 1, 1,
          m_hWnd, (HMENU)(INT_PTR)ids[i], AfxGetInstanceHandle(), NULL);
      if (!input || !edits[i]->Attach(input))
        return -1;
      ::SendMessageW(input, EM_SETLIMITTEXT, i == 0 ? 64 : 1024, 0);
    }
    ::SendMessageW(
        pattern.m_hWnd, EM_SETCUEBANNER, 0,
        (LPARAM)L"Paste a distinctive label from the received output");
    ::SendMessageW(prefix.m_hWnd, EM_SETCUEBANNER, 0,
                   (LPARAM)L"Example: +pub  or  page Alice=");
    regex.Create("Use regular expression (literal text is the default)",
                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                 CRect(0, 0, 1, 1), this, 3);
    omit.Create("Show matches only here (hide them from Main)",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX |
                    BS_MULTILINE,
                CRect(0, 0, 1, 1), this, 4);
    save.Create("Create route",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                CRect(0, 0, 1, 1), this, 5);
    cancel.Create("Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                  CRect(0, 0, 1, 1), this, 6);
    Font();
    Layout();
    RefreshRoutes();
    SetTimer(1, 1000, NULL);
    return 0;
  }
  afx_msg void OnSize(UINT type, int width, int height) {
    CWnd::OnSize(type, width, height);
    Layout();
  }
  afx_msg void OnGetMinMaxInfo(MINMAXINFO *info) {
    info->ptMinTrackSize.x = Scale(520);
    info->ptMinTrackSize.y = Scale(650);
    CWnd::OnGetMinMaxInfo(info);
  }
  afx_msg LRESULT OnDpiChanged(WPARAM value, LPARAM bounds) {
    dpi = LOWORD(value);
    Font();
    if (bounds) {
      RECT *r = (RECT *)bounds;
      SetWindowPos(NULL, r->left, r->top, r->right - r->left,
                   r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    Layout();
    return 0;
  }
  afx_msg void OnTimer(UINT_PTR id) {
    if (id == 1)
      Destination();
    CWnd::OnTimer(id);
  }
  afx_msg void OnSelect() {
    editing = routes.GetCurSel();
    if (editing < 0 || editing >= (int)visibleRoutes.size())
      return;
    const WorkspaceRoute &route = visibleRoutes[editing];
    ::SetWindowTextW(name.m_hWnd,
                     RouteWide((LPCTSTR)route.transcriptName).c_str());
    ::SetWindowTextW(pattern.m_hWnd, route.pattern.c_str());
    ::SetWindowTextW(
        prefix.m_hWnd,
        frame->WorkspaceInputPrefix(identity, route.transcriptName).c_str());
    regex.SetCheck(route.regex ? BST_CHECKED : BST_UNCHECKED);
    omit.SetCheck(route.omitFromMain ? BST_CHECKED : BST_UNCHECKED);
    removeRoute.EnableWindow(TRUE);
    save.SetWindowText("Save changes");
    Feedback(L"Editing the selected rule. The prefix applies to this entire "
             L"named transcript.");
  }
  afx_msg void OnNew() {
    editing = -1;
    routes.SetCurSel(-1);
    ::SetWindowTextW(name.m_hWnd, L"Tells");
    ::SetWindowTextW(pattern.m_hWnd, L"");
    ::SetWindowTextW(prefix.m_hWnd, L"");
    regex.SetCheck(BST_UNCHECKED);
    omit.SetCheck(BST_UNCHECKED);
    removeRoute.EnableWindow(FALSE);
    save.SetWindowText("Create route");
    Feedback(L"New rule: choose received text to match and an optional "
             L"outgoing prefix.");
    name.SetFocus();
  }
  afx_msg void OnChannel() {
    OnNew();
    ::SetWindowTextW(name.m_hWnd, L"Public");
    ::SetWindowTextW(pattern.m_hWnd, L"[Public]");
    ::SetWindowTextW(prefix.m_hWnd, L"+pub ");
    Feedback(L"Example only: replace [Public] and +pub with this game's actual "
             L"label and command before saving.");
    pattern.SetFocus();
  }
  afx_msg void OnTell() {
    OnNew();
    ::SetWindowTextW(name.m_hWnd, L"Tells (Alice)");
    ::SetWindowTextW(pattern.m_hWnd, L"pages:");
    ::SetWindowTextW(prefix.m_hWnd, L"page Alice=");
    Feedback(L"Example only: outgoing replies go to Alice. Change the "
             L"recipient and match text for your game; no recipient is "
             L"inferred from incoming tells.");
    prefix.SetFocus();
  }
  afx_msg void OnDelete() {
    if (editing < 0 || editing >= (int)visibleRoutes.size())
      return;
    if (AfxMessageBox("Delete this local routing rule? The transcript and its "
                      "text remain open.",
                      MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
      return;
    if (!frame->RemoveWorkspaceRoute(visibleRoutes[editing])) {
      Feedback(L"The route could not be removed; it remains selected.");
      return;
    }
    RefreshRoutes();
    OnNew();
    Feedback(L"Routing rule deleted. Existing transcript text is preserved.");
  }
  afx_msg void OnSave() {
    try {
      if (WorkspaceEndpointForWindow((LPCTSTR)identity).empty()) {
        Feedback(L"The original world has closed. Open its route manager again "
                 L"after reopening that world.");
        return;
      }
      auto nameText = Read(name.m_hWnd, 64),
           patternText = Read(pattern.m_hWnd, 1024),
           prefixText = Read(prefix.m_hWnd, 1024);
      CString transcriptName;
      for (wchar_t c : nameText) {
        if (c < 32 || c > 126) {
          Feedback(
              L"Transcript names use printable English-keyboard characters; "
              L"received text and outgoing messages support Unicode.");
          name.SetFocus();
          return;
        }
        transcriptName += (char)c;
      }
      transcriptName.TrimLeft();
      transcriptName.TrimRight();
      std::string prefixUtf8, patternUtf8;
      if (!RouteUtf8(prefixText, prefixUtf8) ||
          !RouteUtf8(patternText, patternUtf8)) {
        Feedback(
            L"The match text or prefix contains an invalid Unicode character.");
        return;
      }
      auto checked = workspace_input::PrepareHumanInput(
          (LPCTSTR)identity, (LPCTSTR)identity, true, prefixUtf8, "x");
      if (!checked.ok) {
        Feedback(RouteWide(checked.error.c_str()));
        prefix.SetFocus();
        return;
      }
      for (wchar_t c : patternText)
        if (c < 32 || c == 127) {
          Feedback(L"Match one completed incoming line; remove control "
                   L"characters from the match text.");
          pattern.SetFocus();
          return;
        }
      WorkspaceRoute route;
      route.connectionId = identity;
      route.transcriptName = transcriptName;
      route.pattern = patternText;
      route.regex = regex.GetCheck() == BST_CHECKED;
      route.omitFromMain = omit.GetCheck() == BST_CHECKED;
      CString error;
      if (!route.Compile(error)) {
        Feedback(RouteWide((LPCTSTR)error));
        pattern.SetFocus();
        return;
      }
      bool saved = editing >= 0 && editing < (int)visibleRoutes.size()
                       ? frame->ReplaceWorkspaceRoute(visibleRoutes[editing],
                                                      route, error)
                       : frame->AddWorkspaceRoute(route, error);
      if (!saved) {
        Feedback(RouteWide((LPCTSTR)error));
        return;
      }
      if (!frame->SetWorkspaceInputPrefix(identity, transcriptName,
                                          prefixText)) {
        Feedback(L"The routing rule was saved, but its outgoing prefix was not "
                 L"saved. Retry the prefix before sending.");
        return;
      }
      RefreshRoutes();
      for (size_t i = 0; i < visibleRoutes.size(); ++i)
        if (visibleRoutes[i].transcriptName == route.transcriptName &&
            visibleRoutes[i].pattern == route.pattern &&
            visibleRoutes[i].regex == route.regex) {
          routes.SetCurSel((int)i);
          OnSelect();
          break;
        }
      Feedback(L"Rule and outgoing prefix saved locally. Incoming matches use "
               L"the selected copy/hide behavior.");
    } catch (const std::exception &error) {
      Feedback(RouteWide(error.what()));
    }
  }
  afx_msg void OnDestroy() {
    KillTimer(1);
    RemovePropW(GetSafeHwnd(), L"MUSHclientWorkspace");
    RemovePropW(GetSafeHwnd(), L"MUSHclientRouteManager");
    CWnd::OnDestroy();
  }
  afx_msg void OnClose() { DestroyWindow(); }
  DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(CWorkspaceRouteDialog, CWnd)
ON_WM_CREATE()
ON_WM_CLOSE()
ON_WM_DESTROY()
ON_WM_SIZE()
ON_WM_GETMINMAXINFO()
ON_WM_TIMER()
ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
ON_BN_CLICKED(5, OnSave)
ON_BN_CLICKED(6, OnClose)
ON_BN_CLICKED(8, OnNew)
ON_BN_CLICKED(9, OnDelete)
ON_BN_CLICKED(11, OnChannel)
ON_BN_CLICKED(12, OnTell)
ON_LBN_SELCHANGE(7, OnSelect)
END_MESSAGE_MAP()
void OpenWorkspaceRouteManager(CMainFrame *frame, const CString &identity) {
  CWorkspaceRouteDialog *dialog = new CWorkspaceRouteDialog(frame, identity);
  if (!dialog->Open())
    return;
  dialog->ShowWindow(SW_SHOW);
  dialog->SetForegroundWindow();
}
