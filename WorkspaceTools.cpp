#include "stdafx.h"
#include "MUSHclient.h"
#include "doc.h"
#include "mainfrm.h"
#include "WorkspaceIntegration.h"
#include "WorkspaceTools.h"
#include "workspace/PennEditorWindow.h"
#include "AgentBridgeHost.h"
#include "WorkspaceSceneDirector.h"
#include "workspace/AgentReviewPane.h"
#include "workspace/WorkflowUi.h"
#include <map>
#include <regex>

namespace {
struct GrantUi {
  agent_workspace::Counter session;
  std::vector<std::string> windows;
  bool drafts;
  std::shared_ptr<agent_workspace::BridgeHost> bridge;
};
std::map<std::string, GrantUi> grants;
struct Ui {
  unsigned kind;
  UINT dpi = 96;
  HFONT font = NULL;
  HWND window, list, name, input, status, session;
  std::wstring renderedConversation;
  std::vector<std::string> ids;
  std::string scene;
};
std::wstring wide(const std::string &s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0);
  std::wstring v(n, L'\0');
  if (n)
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &v[0], n);
  return v;
}
std::string utf8(const std::wstring &s) {
  int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0,
                              NULL, NULL);
  std::string v(n, '\0');
  if (n)
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), &v[0], n, NULL,
                        NULL);
  return v;
}
std::string text(HWND w) {
  int n = GetWindowTextLengthW(w);
  std::wstring s(n + 1, L'\0');
  GetWindowTextW(w, &s[0], n + 1);
  s.resize(n);
  return utf8(s);
}
int scale(Ui &u, int value) { return MulDiv(value, u.dpi, 96); }
void setFont(Ui &u) {
  workflow_ui::Polish(u.window);
}
void layout(Ui &u, int width, int height) {
  if (!u.list)
    return;
  int pad = scale(u, 12), body = max(80, width - 2 * pad), top = scale(u, 75);
  MoveWindow(u.name, scale(u, 100), scale(u, 8),
             max(scale(u, 60), width - scale(u, 112)), scale(u, 24), TRUE);
  MoveWindow(u.session, pad, scale(u, 36), body, scale(u, 34), TRUE);
  bool compact = width < scale(u, 680) || height < scale(u, 520);
  for (int id : {21, 22, 23, 25, 26})
    if (GetDlgItem(u.window, id))
      ShowWindow(GetDlgItem(u.window, id), compact ? SW_HIDE : SW_SHOW);
  ShowWindow(GetDlgItem(u.window, 29), compact ? SW_SHOW : SW_HIDE);
  int extra = u.kind == 59002               ? scale(u, 42)
              : u.kind == 59003 && !compact ? scale(u, 34)
                                            : 0;
  int inputHeight = compact && u.kind == 59002 ? scale(u, 26) : scale(u, 46);
  int footer = scale(u, 135) + extra - (scale(u, 46) - inputHeight);
  int listHeight = max(scale(u, 25), height - top - footer);
  MoveWindow(u.list, pad, top, body, listHeight, TRUE);
  int y = top + listHeight + scale(u, 8);
  MoveWindow(u.input, pad, y, body, inputHeight, TRUE);
  y += inputHeight + scale(u, 8);
  if (compact) {
    MoveWindow(GetDlgItem(u.window, 20), pad, y, max(70, body - scale(u, 102)),
               scale(u, 28), TRUE);
    MoveWindow(GetDlgItem(u.window, 29), pad + body - scale(u, 94), y,
               scale(u, 94), scale(u, 28), TRUE);
  } else {
    int primary = scale(u, 215), step = scale(u, 10);
    MoveWindow(GetDlgItem(u.window, 20), pad, y, primary, scale(u, 28), TRUE);
    int available = body - primary - 3 * step;
    int button = available / 3;
    for (int i = 0; i < 3; ++i)
      MoveWindow(GetDlgItem(u.window, 21 + i),
                 pad + primary + step + i * (button + step), y, button,
                 scale(u, 28), TRUE);
  }
  y += scale(u, 34);
  if (u.kind == 59002) {
    MoveWindow(GetDlgItem(u.window, 24), pad, y, body, scale(u, 38), TRUE);
    y += scale(u, 42);
  }
  if (u.kind == 59003 && !compact) {
    int half = (body - scale(u, 8)) / 2;
    MoveWindow(GetDlgItem(u.window, 25), pad, y, half, scale(u, 28), TRUE);
    MoveWindow(GetDlgItem(u.window, 26), pad + half + scale(u, 8), y, half,
               scale(u, 28), TRUE);
    y += scale(u, 34);
  }
  MoveWindow(u.status, pad, y, body,
             max(scale(u, 25), height - y - scale(u, 8)), TRUE);
}
HWND control(Ui &u, const wchar_t *cls, const wchar_t *caption, DWORD style,
             int id, int x, int y, int w, int h) {
  HWND c = CreateWindowExW(!wcscmp(cls,L"EDIT") || !wcscmp(cls,L"LISTBOX")?WS_EX_CLIENTEDGE:0, cls, caption,
                           WS_CHILD | WS_VISIBLE | style |
                               (!wcscmp(cls, L"STATIC") ? 0 : WS_TABSTOP),
                           x, y, w, h, u.window, (HMENU)(INT_PTR)id,
                           AfxGetInstanceHandle(), NULL);
  SendMessage(
      c, WM_SETFONT,
      (WPARAM)(u.font ? u.font : (HFONT)GetStockObject(DEFAULT_GUI_FONT)),
      TRUE);
  return c;
}
void status(Ui &u, const std::string &value) {
  SetWindowTextW(u.status, wide(value).c_str());
}
std::string agent(Ui &u) {
  std::string value = text(u.name);
  size_t begin = value.find_first_not_of(" \t\r\n"),
         end = value.find_last_not_of(" \t\r\n");
  if (begin == std::string::npos)
    throw std::runtime_error(
        "Enter an agent name before changing access or posting a message.");
  value = value.substr(begin, end - begin + 1);
  if (value.size() > 64)
    throw std::runtime_error("Agent names are limited to 64 UTF-8 bytes.");
  for (unsigned char c : value)
    if (c < 32 || c == 127)
      throw std::runtime_error(
          "Agent names cannot contain control characters.");
  return value;
}
void accessStatus(Ui &u) {
  if (!u.session)
    return;
  const std::string a = agent(u);
  const auto summary = WorkspaceAgentStore().humanWorkspace(a);
  std::string label = summary.session
                          ? "Access enabled for " +
                                std::to_string(summary.windows.size()) +
                                " explicit output windows"
                          : "Access disabled or expired";
  auto bridge = grants.find(a);
  if (bridge != grants.end() && bridge->second.bridge &&
      bridge->second.bridge->running())
    label += " | Bridge ready; external runner state unconfirmed";
  SetWindowTextW(u.session, wide(label).c_str());
  if (u.kind == 59003)
    EnableWindow(GetDlgItem(u.window, 20), summary.session != 0);
  if(u.kind==59002)EnableWindow(GetDlgItem(u.window,20),SendMessageW(u.list,LB_GETSELCOUNT,0,0)>0);
  EnableWindow(GetDlgItem(u.window,22),summary.session!=0);
  if(u.kind==59003 && !summary.session)status(u,"Posting is unavailable while access is off. Open Access & sharing to choose output for this agent.");
  if(u.kind==59002 && SendMessageW(u.list,LB_GETSELCOUNT,0,0)<=0 && !summary.session)status(u,"Select one or more output windows to enable access. Draft sharing is optional and stays off until checked.");
}
std::string displayLines(const std::string &value) {
  std::string result;
  result.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    char c = value[i];
    if (c == '\r') {
      result += '\r';
      if (i + 1 == value.size() || value[i + 1] != '\n')
        result += '\n';
    } else if (c == '\n') {
      if (i == 0 || value[i - 1] != '\r')
        result += '\r';
      result += '\n';
    } else {
      result += c;
    }
  }
  return result;
}
void refresh(Ui &u) {
  if (u.kind != 59003)
    SendMessage(u.list, LB_RESETCONTENT, 0, 0);
  u.ids.clear();
  if (u.kind == 59002) {
    POSITION pos = App.m_pWorldDocTemplate->GetFirstDocPosition();
    while (pos) {
      CMUSHclientDoc *d =
          (CMUSHclientDoc *)App.m_pWorldDocTemplate->GetNextDoc(pos);
      WorkspaceConnectionId(d);
    }
    for (const auto &item : WorkspaceAgentStore().humanWindows()) {
      u.ids.push_back(item.id);
      std::string label = item.world + " / " + item.title;
      SendMessageW(u.list, LB_ADDSTRING, 0, (LPARAM)wide(label).c_str());
    }
  } else if (u.kind == 59003) {
    agent_workspace::Grant summary =
        WorkspaceAgentStore().humanWorkspace(agent(u));
    const auto &messages = summary.messages;
    std::string transcript;
    size_t start = messages.size();
    size_t bytes = 0;
    while (start > 0 && bytes + messages[start - 1].text.size() + 32 <= 65536) {
      --start;
      bytes += messages[start].text.size() + 32;
    }
    if (start)
      transcript = "[Earlier messages omitted from this bounded view.]\r\n\r\n";
    for (size_t i = start; i < messages.size(); ++i) {
      transcript += messages[i].role + ":\r\n" +
                    displayLines(messages[i].text) + "\r\n\r\n";
    }
    auto rendered = wide(transcript);
    if (rendered != u.renderedConversation) {
      DWORD from = 0, to = 0;
      SendMessageW(u.list, EM_GETSEL, (WPARAM)&from, (LPARAM)&to);
      int line = (int)SendMessageW(u.list, EM_GETFIRSTVISIBLELINE, 0, 0),
          count = (int)SendMessageW(u.list, EM_GETLINECOUNT, 0, 0);
      bool follow = count < 8 || line >= count - 8;
      SetWindowTextW(u.list, rendered.c_str());
      if (follow) {
        SendMessageW(u.list, EM_SETSEL, rendered.size(), rendered.size());
        SendMessageW(u.list, EM_SCROLLCARET, 0, 0);
      } else {
        SendMessageW(u.list, EM_SETSEL, from, to);
        SendMessageW(u.list, EM_LINESCROLL, 0, line);
      }
      u.renderedConversation = rendered;
    }
  } else {
    const std::vector<agent_workspace::Draft> &drafts =
        WorkspaceAgentStore().drafts();
    for (size_t i = 0; i < drafts.size(); ++i) {
      u.ids.push_back(drafts[i].id);
      std::string line =
          drafts[i].agent + " / " + drafts[i].window + " / " + drafts[i].id;
      SendMessageW(u.list, LB_ADDSTRING, 0, (LPARAM)wide(line).c_str());
    }
  }
}
LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
  Ui *u = (Ui *)GetWindowLongPtrW(w, GWLP_USERDATA);
  if (msg == WM_NCCREATE) {
    u = (Ui *)((CREATESTRUCTW *)lp)->lpCreateParams;
    u->window = w;
    SetWindowLongPtrW(w, GWLP_USERDATA, (LONG_PTR)u);
  }
  if (!u)
    return DefWindowProcW(w, msg, wp, lp);
  try {
    if (msg == WM_CREATE) {
      auto getDpi = (UINT(WINAPI *)(HWND))GetProcAddress(
          GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
      if (getDpi)
        u->dpi = getDpi(w);
      setFont(*u);
      control(*u, L"STATIC", L"Agent name", 0, 0, 12, 10, 110, 22);
      u->name = control(*u, L"EDIT", L"Assistant", ES_AUTOHSCROLL, 10, 125, 8,
                        350, 24);
      SendMessageW(u->name, EM_SETLIMITTEXT, 64, 0);
      u->list = control(
          *u, u->kind == 59003 ? L"EDIT" : L"LISTBOX", L"",
          u->kind == 59003
              ? (ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL)
              : (WS_VSCROLL | LBS_NOTIFY |
                 (u->kind == 59002 ? LBS_EXTENDEDSEL : 0)),
          11, 12, 65, 740, 265);
      u->input =
          control(*u, L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                  12, 12, 320, 740, 90);
      SendMessageW(u->input, EM_SETLIMITTEXT, 32768, 0);
      if (u->kind == 59002)
        SendMessageW(u->input, EM_SETREADONLY, TRUE, 0);
      SendMessageW(u->input, EM_SETCUEBANNER, 0,
                   (LPARAM)(u->kind == 59003
                                ? L"Message to this agent (Ctrl+Enter posts; "
                                  L"Enter adds a line)"
                                : L"External runner launch command appears "
                                  L"here after explicit access is enabled"));
      control(*u, L"BUTTON",
              u->kind == 59002   ? L"Enable selected output"
              : u->kind == 59003 ? L"Post to agent conversation"
                                 : L"Review selected unsent draft",
              BS_PUSHBUTTON, 20, 12, 425, 215, 30);
      control(*u, L"BUTTON", L"Refresh", BS_PUSHBUTTON, 21, 235, 425, 90, 30);
      control(*u, L"BUTTON", L"Revoke this agent", BS_PUSHBUTTON, 22, 335, 425,
              160, 30);
      control(*u, L"BUTTON", L"Revoke all", BS_PUSHBUTTON, 23, 505, 425, 110,
              30);
      u->session = control(*u, L"STATIC", L"Access starts disabled", 0, 27, 12,
                           36, 740, 25);
      if (u->kind == 59003)
        control(*u, L"BUTTON", L"Access && sharing...", BS_PUSHBUTTON, 25, 12,
                465, 180, 28);
      if (u->kind == 59003)
        control(*u, L"BUTTON", L"Review drafts && activity...", BS_PUSHBUTTON, 26, 202,
                465, 180, 28);
      if (u->kind == 59002)
        control(*u, L"BUTTON", L"Allow unsent drafts and reviewed edits",
                BS_AUTOCHECKBOX | BS_MULTILINE, 24, 12, 465, 480, 28);
      control(*u, L"BUTTON", L"Options...", BS_PUSHBUTTON, 29, 12, 465, 100,
              28);
      u->status = control(
          *u, L"STATIC",
          L"Access starts disabled. Select only the output you want to share.",
          0, 0, 12, 505, 740, 55);
      workflow_ui::Polish(w);
      using workspace_style::Icon;
      workflow_ui::Button(w,20,u->kind==59003?Icon::Send:Icon::Agent,true);
      workflow_ui::Button(w,21,Icon::Refresh);
      workflow_ui::Button(w,22,Icon::Close);
      workflow_ui::Button(w,23,Icon::Close);
      if(u->kind==59003){workflow_ui::Button(w,25,Icon::Settings);workflow_ui::Button(w,26,Icon::Editor);}
      workflow_ui::Button(w,29,Icon::ChevronDown);
      refresh(*u);
      accessStatus(*u);
      SetTimer(w, 1, 1000, NULL);
      return 0;
    }
    if (msg == WM_TIMER) {
      if (u->kind == 59003)
        refresh(*u);
      accessStatus(*u);
      return 0;
    }
    if (msg == WM_GETMINMAXINFO) {
      auto limits = (MINMAXINFO *)lp;
      limits->ptMinTrackSize.x = scale(*u,380);
      limits->ptMinTrackSize.y = scale(*u,440);
      return 0;
    }
    if (msg == WM_SIZE && u->list) {
      layout(*u, LOWORD(lp), HIWORD(lp));
      return 0;
    }
    if (msg == WM_DPICHANGED) {
      u->dpi = LOWORD(wp);
      workflow_ui::DpiChanged(w,lp);
      setFont(*u);
      RECT client;
      GetClientRect(w, &client);
      layout(*u, client.right, client.bottom);
      return 0;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == 10 && HIWORD(wp) == EN_CHANGE &&
        u->session) {
      refresh(*u);
      accessStatus(*u);
      return 0;
    }
    if(msg==WM_COMMAND && LOWORD(wp)==11 && HIWORD(wp)==LBN_SELCHANGE){accessStatus(*u);return 0;}
    if (msg == WM_COMMAND && HIWORD(wp) == BN_CLICKED) {
      unsigned id = LOWORD(wp);
      std::string a;
      if (id != 23 && id != 29 && id != 25 && id != 26)
        a = agent(*u);
      auto &store = WorkspaceAgentStore();
      if (id == 29) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 21, L"Refresh");
        AppendMenuW(menu, MF_STRING, 22, L"Revoke this agent");
        AppendMenuW(menu, MF_STRING, 23, L"Revoke all agents");
        if (u->kind == 59003) {
          AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
          AppendMenuW(menu, MF_STRING, 25, L"Access / sharing...");
          AppendMenuW(menu, MF_STRING, 26, L"Review / captures...");
        }
        RECT bounds;
        GetWindowRect(GetDlgItem(w, 29), &bounds);
        UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN,
                                     bounds.left, bounds.bottom, 0, w, NULL);
        DestroyMenu(menu);
        if (choice)
          SendMessageW(w, WM_COMMAND, MAKEWPARAM(choice, BN_CLICKED), 0);
        return 0;
      }
      if (id == 25)
        OpenWorkspaceTool(Frame, 59002);
      else if (id == 26)
        OpenWorkspaceTool(Frame, 59007);
      else if (id == 21) {
        refresh(*u);
        accessStatus(*u);
      } else if (id == 22) {
        store.revoke(a);
        grants.erase(a);
        status(*u, "Agent access revoked.");
      } else if (id == 23) {
        store.revokeAll();
        grants.clear();
        status(*u, "All agent access revoked.");
      } else if (id == 20 && u->kind == 59002) {
        int count = (int)SendMessage(u->list, LB_GETSELCOUNT, 0, 0);
        if (count <= 0)
          throw std::runtime_error("Select at least one output window.");
        std::vector<int> selected(count);
        SendMessage(u->list, LB_GETSELITEMS, count, (LPARAM)selected.data());
        GrantUi g;
        g.drafts = SendDlgItemMessage(w, 24, BM_GETCHECK, 0, 0) == BST_CHECKED;
        for (int i = 0; i < count; ++i)
          g.windows.push_back(u->ids.at(selected[i]));
        grants.erase(a);
        g.session = store.enable(a, g.windows, g.drafts);
        g.bridge = std::make_shared<agent_workspace::BridgeHost>();
        try {
          g.bridge->start(store, a, g.session);
        } catch (...) {
          store.revoke(a);
          throw;
        }
        grants[a] = g;
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(NULL, path, MAX_PATH);
        std::wstring folder(path);
        folder = folder.substr(0, folder.find_last_of(L"\\/"));
        std::wstring launch = L"\"" + folder +
                              L"\\MUSHclientAgent.exe\" --endpoint \"" +
                              g.bridge->endpoint() + L"\" mcp";
        SetWindowTextW(u->input, launch.c_str());
        status(*u, "Access enabled for selected output only. Copy the launch "
                   "command above into your external agent configuration. No "
                   "game-send tools are exposed.");
      } else if (id == 20 && u->kind == 59003) {
        std::string message = text(u->input);
        if (message.empty())
          throw std::runtime_error(
              "Type a message before posting to the agent conversation.");
        if (message.size() > 32768)
          throw std::runtime_error("The message exceeds 32 KiB in UTF-8. "
                                   "Shorten it before posting.");
        store.postHuman(a, message);
        status(*u, "Posted to " + a + ".");
        SetWindowTextW(u->input, L"");
        refresh(*u);
      } else if (id == 20) {
        int selected = (int)SendMessage(u->list, LB_GETCURSEL, 0, 0);
        if (selected < 0)
          throw std::runtime_error("Select a draft to review.");
        const auto &drafts = store.drafts();
        for (size_t i = 0; i < drafts.size(); ++i)
          if (drafts[i].id == u->ids.at(selected))
            WorkspaceOpenDraft(w, drafts[i]);
      }
      accessStatus(*u);
      return 0;
    }
    if (msg == WM_CLOSE) {
      DestroyWindow(w);
      return 0;
    }
    if (msg == WM_NCDESTROY) {
      KillTimer(w, 1);
      SetWindowLongPtrW(w, GWLP_USERDATA, 0);
      if (u->font)
        DeleteObject(u->font);
      delete u;
    }
  } catch (const std::exception &e) {
    if (u && IsWindow(u->status))
      status(*u, e.what());
    return 0;
  }
  return DefWindowProcW(w, msg, wp, lp);
}
} // namespace
void OpenWorkspaceTool(CMainFrame &frame, UINT command) {
  PennEditorSetTransport(WorkspaceSendEditorRead);
  PennEditorSetSendPrepared(WorkspaceSendPreparedEditor);
  if (command == 59007) {
    HWND pane = OpenAgentReviewPane(frame.GetSafeHwnd());
    if (pane)
      frame.HostWorkspaceTool(pane, "Agent review and captures");
    return;
  }
  if (command == 59004) {
    OpenWorkspaceSceneDirector(frame);
    return;
  }
  if (command == 59001) {
    CMDIChildWnd *child = frame.MDIGetActive();
    CMUSHclientDoc *d =
        child ? DYNAMIC_DOWNCAST(CMUSHclientDoc, child->GetActiveDocument())
              : NULL;
    const CString connectionId = d ? WorkspaceConnectionId(d) : "local:unsent";
    OpenPennEditor(frame.GetSafeHwnd(), (LPCTSTR)connectionId, "",
                   WorkspaceWorldName(connectionId));
    return;
  }
  WNDCLASSW cls = {0};
  cls.hInstance = AfxGetInstanceHandle();
  cls.lpfnWndProc = proc;
  cls.lpszClassName = L"MUSHclientWorkspaceTools";
  cls.hCursor = LoadCursor(NULL, IDC_ARROW);
  cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  RegisterClassW(&cls);
  Ui *u = new Ui();
  u->kind = command;
  HWND w =
      CreateWindowExW(0, cls.lpszClassName,
                      command == 59002   ? L"Agent access"
                      : command == 59003 ? L"Agent conversations"
                                         : L"Scene director - reviewed drafts",
                      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 790,
                      630, frame.GetSafeHwnd(), NULL, cls.hInstance, u);
  if (w) {
    workspace_style::FitToWorkArea(w,380,440);
    if (command == 59003)
      frame.HostWorkspaceTool(w, "Agent conversations");
    ShowWindow(w, SW_SHOW);
  } else
    delete u;
}
void ShutdownWorkspaceTools() {
  WorkspaceAgentStore().revokeAll();
  grants.clear();
}

BOOL WorkspaceTranslateMessage(MSG *message) {
  if(App.m_pMainWnd && Frame.GetSafeHwnd() && Frame.HandleWorkspaceShortcut(message))return TRUE;
  if (message->message < WM_KEYFIRST || message->message > WM_KEYLAST)
    return FALSE;
  HWND owner = NULL;
  CWorkspacePane *pane = NULL;
  for (HWND cursor = message->hwnd; cursor; cursor = ::GetParent(cursor)) {
    wchar_t name[80] = {0};
    ::GetClassNameW(cursor, name, 80);
    if (::GetPropW(cursor, L"MUSHclientWorkspace") ||
        !wcscmp(name, L"MUSHclientWorkspaceTools") ||
        !wcscmp(name, L"MUSHclientSceneDirector") ||
        !wcscmp(name, L"MUSHclientPennIDE") ||
        !wcscmp(name, L"MUSHclientAgentReview")) {
      owner = cursor;
      break;
    }
    CWnd *native = CWnd::FromHandlePermanent(cursor);
    if (native && native->IsKindOf(RUNTIME_CLASS(CWorkspacePane))) {
      pane = static_cast<CWorkspacePane *>(native);
      owner = cursor;
      break;
    }
  }
  if (!owner)
    return FALSE;
  const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
  if (message->message == WM_KEYDOWN && control &&
      (GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000) &&
      message->wParam >= VK_F1 && message->wParam <= VK_F9 && App.m_pMainWnd) {
    if (Frame.FocusWorkspaceTranscript((size_t)(message->wParam - VK_F1)))
      return TRUE;
  }
  if (::GetPropW(owner, L"MUSHclientRouteManager")) {
    CWnd *route = CWnd::FromHandlePermanent(owner);
    if (route && route->PreTranslateMessage(message))
      return TRUE;
  }
  wchar_t ownerClass[80] = {0};
  ::GetClassNameW(owner, ownerClass, 80);
  if (message->message == WM_KEYDOWN && control &&
      message->wParam == VK_RETURN &&
      !wcscmp(ownerClass, L"MUSHclientWorkspaceTools") &&
      message->hwnd == GetDlgItem(owner, 12) && GetDlgItem(owner, 25)) {
    ::SendMessageW(owner, WM_COMMAND, MAKEWPARAM(20, BN_CLICKED), 0);
    return TRUE;
  }
  if (pane && !pane->IsToolHost() && pane->PreTranslateMessage(message))
    return TRUE;
  wchar_t focusedClass[80] = {0};
  ::GetClassNameW(message->hwnd, focusedClass, 80);
  if(workflow_ui::Keyboard(owner,message))return TRUE;
  if (message->message == WM_KEYDOWN && control && message->wParam == 'A' &&
      (!_wcsicmp(focusedClass, L"EDIT") ||
       !_wcsicmp(focusedClass, L"RICHEDIT50W"))) {
    ::SendMessageW(message->hwnd, EM_SETSEL, 0, -1);
    return TRUE;
  }
  if (message->message == WM_KEYDOWN && message->wParam == VK_TAB &&
      _wcsicmp(focusedClass, L"Scintilla") &&
      ::IsDialogMessageW(owner, message)) {
    // A constrained tool viewport follows keyboard focus, including controls
    // nested inside a native combo/hotkey control.
    ::PostMessageW(owner,WM_APP+742,0,0);
    return TRUE;
  }
  // Deliver native control/subclass shortcuts before the legacy game
  // accelerators.
  ::TranslateMessage(message);
  ::DispatchMessageW(message);
  return TRUE;
}
