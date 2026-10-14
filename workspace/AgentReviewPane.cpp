#define NOMINMAX
#include "AgentReviewPane.h"
#include "../AgentWorkspaceCore.h"
#include "PennEditorWindow.h"
#include "PennLanguage.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <stdexcept>
#pragma comment(lib, "user32.lib")
agent_workspace::Store &WorkspaceAgentStore();
HWND WorkspaceOpenDraft(HWND parent, const agent_workspace::Draft &draft);
namespace {
enum {
  Name = 201,
  Mode,
  List,
  Before,
  After,
  Status,
  Detail,
  Primary,
  Secondary,
  Snapshot,
  Refresh
};
enum Category { Editors, Outputs, Proposals, Tasks, Captures, Drafts };
struct Ui {
  HWND window = 0, name = 0, mode = 0, list = 0, before = 0, after = 0,
       status = 0, detail = 0;
  std::vector<std::string> ids, labels;
  std::vector<PennEditorInfo> snapshots;
  std::map<std::string, uint64_t> registered;
  std::string previewID, previewAgent, selectionWindow;
  uint64_t previewRevision = 0, previewSession = 0;
  bool previewCurrent = false;
};
HWND Pane = 0;
std::wstring Wide(const std::string &s) {
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                              static_cast<int>(s.size()), 0, 0);
  std::wstring w(n, L' ');
  if (n)
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0],
                        n);
  return w;
}
std::string Utf8(const std::wstring &w) {
  int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                              0, 0, 0, 0);
  std::string s(n, ' ');
  if (n)
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0],
                        n, 0, 0);
  return s;
}
std::wstring Text(HWND h) {
  int n = GetWindowTextLengthW(h);
  std::wstring w(n + 1, L' ');
  GetWindowTextW(h, &w[0], n + 1);
  w.resize(n);
  return w;
}
std::string Agent(Ui *u) { return Utf8(Text(u->name)); }
uint64_t Now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
void StatusText(Ui *u, const std::string &s) {
  SetWindowTextW(u->status, Wide(s).c_str());
}
HWND Control(Ui *u, const wchar_t *cls, const wchar_t *title, DWORD style,
             int id) {
  auto h = CreateWindowExW(WS_EX_CLIENTEDGE, cls, title,
                           WS_CHILD | WS_VISIBLE | style, 0, 0, 1, 1, u->window,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(0), 0);
  SendMessageW(h, WM_SETFONT,
               reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
               TRUE);
  return h;
}
int ModeValue(Ui *u) {
  return static_cast<int>(SendMessageW(u->mode, CB_GETCURSEL, 0, 0));
}
std::string Selected(Ui *u) {
  int i = static_cast<int>(SendMessageW(u->list, LB_GETCURSEL, 0, 0));
  if (i < 0 || static_cast<size_t>(i) >= u->ids.size())
    throw std::runtime_error("Select an item first.");
  return u->ids[i];
}
std::string EditorKey(uint64_t id) {
  return "native-editor-" + std::to_string(id);
}
bool Actual(uint64_t id, PennEditorInfo &found) {
  for (auto &e : PennEditorListSnapshots())
    if (e.documentID == id) {
      found = e;
      return true;
    }
  return false;
}
bool Cached(Ui *u, uint64_t id, PennEditorInfo &found) {
  for (auto &e : u->snapshots)
    if (e.documentID == id) {
      found = e;
      return true;
    }
  return false;
}
void Put(const std::string &key, const PennEditorInfo &e) {
  agent_workspace::Editor record;
  record.id = key;
  record.window = e.endpoint;
  record.text = e.source;
  record.revision = e.revision;
  record.uploading = false;
  WorkspaceAgentStore().putEditor(record);
}
void Sync(Ui *u) {
  u->snapshots = PennEditorListSnapshots();
  auto &store = WorkspaceAgentStore();
  auto windows = store.humanWindows();
  std::set<std::string> live;
  for (auto &w : windows)
    live.insert(w.id);
  for (auto i = u->registered.begin(); i != u->registered.end();) {
    PennEditorInfo actual;
    if (!Cached(u, i->second, actual) || actual.endpoint.empty() ||
        !live.count(actual.endpoint) || actual.source.size() > 32768) {
      store.closeEditor(i->first);
      i = u->registered.erase(i);
    } else {
      Put(i->first, actual);
      ++i;
    }
  }
}
void RefreshList(Ui *u) {
  if (!u->list || !u->mode || !u->name || !u->status)
    return;
  Sync(u);
  auto &store = WorkspaceAgentStore();
  auto agent = Agent(u);
  auto grant = store.humanWorkspace(agent);
  std::vector<std::string> ids, labels;
  int mode = ModeValue(u);
  if (mode == Editors) {
    for (auto &e : u->snapshots) {
      ids.push_back(EditorKey(e.documentID));
      labels.push_back(
          "Document " + std::to_string(e.documentID) + " | " + e.endpoint +
          " | revision " + std::to_string(e.revision) +
          (grant.editors.count(ids.back()) ? " | SHARED" : " | private"));
    }
  } else if (mode == Outputs) {
    for (auto &w : store.humanWindows())
      if (grant.windows.count(w.id)) {
        ids.push_back(w.id);
        labels.push_back(w.world + " / " + w.title);
      }
  } else if (mode == Proposals) {
    for (auto &p : store.proposals())
      if (p.second.agent == agent) {
        ids.push_back(p.first);
        auto text = p.second.status;
        if (p.second.status == "pending") {
          auto registered = u->registered.find(p.second.editor);
          PennEditorInfo actual;
          if (grant.session != p.second.session ||
              registered == u->registered.end() ||
              !Cached(u, registered->second, actual) ||
              actual.revision != p.second.revision)
            text = "stale";
        }
        labels.push_back(p.first + " | " + p.second.editor + " | " + text);
      }
  } else if (mode == Tasks) {
    for (auto &p : grant.tasks) {
      ids.push_back(p.first);
      auto &t = p.second;
      labels.push_back(t.title + " | " + t.status +
                       (t.cancellationRequested ? " | cancel requested" : "") +
                       ((Now() > t.heartbeat && Now() - t.heartbeat > 30000)
                            ? " | heartbeat stale"
                            : ""));
    }
  } else if (mode == Captures && grant.session) {
    for (auto &c : store.captures(agent, grant.session)) {
      ids.push_back(c.id);
      labels.push_back(c.name + " | " + (c.active ? "recording" : "stopped") +
                       " | dropped " + std::to_string(c.dropped) + " | gaps " +
                       std::to_string(c.gaps));
    }
  } else if (mode == Drafts) {
    for (auto &d : store.drafts())
      if (d.agent == agent) {
        ids.push_back(d.id);
        labels.push_back(d.id + " | " + d.window + " | unsent");
      }
  }
  std::string selected;
  int index = static_cast<int>(SendMessageW(u->list, LB_GETCURSEL, 0, 0));
  if (index >= 0 && static_cast<size_t>(index) < u->ids.size())
    selected = u->ids[index];
  if (labels != u->labels || ids != u->ids) {
    SendMessageW(u->list, LB_RESETCONTENT, 0, 0);
    u->ids = ids;
    u->labels = labels;
    for (size_t i = 0; i < labels.size(); ++i) {
      SendMessageW(u->list, LB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(Wide(labels[i]).c_str()));
      if (ids[i] == selected)
        SendMessageW(u->list, LB_SETCURSEL, i, 0);
    }
  }
  if (u->previewCurrent && mode == Proposals) {
    auto p = store.proposals().find(u->previewID);
    if (p == store.proposals().end() || p->second.status != "pending" ||
        grant.session != u->previewSession) {
      u->previewCurrent = false;
    } else {
      auto r = u->registered.find(p->second.editor);
      PennEditorInfo actual;
      if (r == u->registered.end() || !Cached(u, r->second, actual) ||
          actual.revision != u->previewRevision)
        u->previewCurrent = false;
    }
  }
  SetWindowTextW(GetDlgItem(u->window, Primary),
                 mode == Editors     ? L"Share selected editor"
                 : mode == Outputs   ? L"Start capture"
                 : mode == Proposals ? L"Accept reviewed diff"
                 : mode == Tasks     ? L"Request cancellation"
                 : mode == Captures  ? L"Read capture"
                                     : L"Open unsent draft");
  SetWindowTextW(GetDlgItem(u->window, Secondary),
                 mode == Editors     ? L"Unshare editor"
                 : mode == Proposals ? L"Reject proposal"
                                     : L"Stop selected capture");
  EnableWindow(GetDlgItem(u->window, Secondary),
               mode == Editors || mode == Proposals || mode == Captures);
  EnableWindow(GetDlgItem(u->window, Snapshot),
               mode == Outputs || mode == Captures);
  EnableWindow(GetDlgItem(u->window, Primary),
               mode != Proposals || u->previewCurrent);
  ShowWindow(u->detail, mode == Outputs ? SW_SHOW : SW_HIDE);
  if (!grant.session)
    StatusText(u, "Agent access is disabled or expired. Enable explicit output "
                  "grants in Agent access first.");
}
void Preview(Ui *u) {
  RefreshList(u);
  auto key = Selected(u);
  auto &store = WorkspaceAgentStore();
  auto agent = Agent(u);
  auto grant = store.humanWorkspace(agent);
  std::string before, after;
  u->selectionWindow.clear();
  u->previewCurrent = false;
  u->previewID = key;
  u->previewAgent = agent;
  u->previewSession = grant.session;
  switch (ModeValue(u)) {
  case Editors:
    for (auto &e : PennEditorListSnapshots())
      if (EditorKey(e.documentID) == key) {
        before = e.source;
        after = grant.editors.count(key)
                    ? "This exact document is shared with " + agent +
                          ". Unshare revokes its editor access."
                    : "Sharing is off until Share selected editor is pressed.";
      }
    break;
  case Outputs: {
    auto output = store.read(agent, grant.session, key, 0, 100);
    for (auto &line : output.lines)
      before += line.text + "\r\n";
    u->selectionWindow = key;
    after = "Select text in the left preview, then Share selected text "
            "snapshot. This preview contains completed shared output only.";
    break;
  }
  case Proposals: {
    auto p = store.proposals().at(key);
    auto registered = u->registered.find(p.editor);
    PennEditorInfo actual;
    if (registered == u->registered.end() ||
        !Actual(registered->second, actual))
      throw std::runtime_error(
          "The original editor no longer exists or is no longer registered.");
    before = actual.source;
    after = p.text;
    u->previewRevision = actual.revision;
    u->previewCurrent = p.status == "pending" && p.session == grant.session &&
                        p.revision == actual.revision;
    StatusText(u,
               p.explanation +
                   (u->previewCurrent
                        ? " | Current full diff. Accept applies locally only."
                        : " | STALE: cannot accept."));
    break;
  }
  case Tasks: {
    auto task = grant.tasks.at(key);
    before = task.title + "\r\nStatus: " + task.status + "\r\n" + task.progress;
    after = "Heartbeat: " + std::to_string(task.heartbeat) +
            "\r\nCancellation is cooperative; requesting it does not prove the "
            "external runner stopped.";
    break;
  }
  case Captures: {
    auto capture = store.readCapture(agent, grant.session, key, 0, 200);
    for (auto &line : capture.lines)
      before += std::to_string(line.sourceIndex) + " " + line.text + "\r\n";
    for (auto &c : store.captures(agent, grant.session))
      if (c.id == key)
        u->selectionWindow = c.window;
    after = "Bounded completed-output capture. Select only the text to attach; "
            "stopped captures remain readable until revoked.";
    break;
  }
  case Drafts:
    for (auto &d : store.drafts())
      if (d.id == key) {
        before = d.text;
        after = "Agent: " + d.agent + "\r\nDestination: " + d.window +
                "\r\nUnsent. Opening an editor never sends.";
      }
    break;
  }
  SetWindowTextW(u->before, Wide(before).c_str());
  SetWindowTextW(u->after, Wide(after).c_str());
  EnableWindow(GetDlgItem(u->window, Primary),
               ModeValue(u) != Proposals || u->previewCurrent);
}
void Accept(Ui *u) {
  auto &store = WorkspaceAgentStore();
  auto key = Selected(u);
  if (key != u->previewID || !u->previewCurrent || Agent(u) != u->previewAgent)
    throw std::runtime_error("Select the proposal to display its full current "
                             "diff before accepting.");
  Sync(u);
  auto p = store.proposals().at(key);
  auto registration = u->registered.find(p.editor);
  if (registration == u->registered.end())
    throw std::runtime_error("Editor is no longer shared.");
  PennEditorInfo before;
  if (!Actual(registration->second, before) || before.revision != p.revision ||
      before.revision != u->previewRevision)
    throw std::runtime_error(
        "The actual draft changed. Refresh and review the new diff.");
  auto validate = [&]() {
    try {
      auto pending = store.proposals().at(key);
      auto editor = store.readEditor(p.agent, p.session, p.editor);
      return pending.status == "pending" && pending.revision == p.revision &&
             pending.text == p.text && editor.revision == p.revision &&
             editor.text == before.source && !editor.uploading;
    } catch (...) {
      return false;
    }
  };
  if (!validate() || !PennEditorApplyReviewedDocumentProposal(
                         before.documentID, p.revision, p.text, validate))
    throw std::runtime_error(
        "Proposal expired or the editor changed. Nothing was accepted.");
  try {
    store.acceptEdit(key);
    PennEditorInfo actual;
    if (Actual(before.documentID, actual))
      Put(p.editor, actual);
  } catch (...) {
    PennEditorInfo current;
    if (Actual(before.documentID, current) && current.source == p.text) {
      PennEditorApplyReviewedDocumentProposal(current.documentID,
                                              current.revision, before.source,
                                              [] { return true; });
      if (Actual(before.documentID, current))
        Put(p.editor, current);
    }
    throw;
  }
  u->previewCurrent = false;
  StatusText(u, "Proposal accepted into the original local editor. Nothing was "
                "sent to the game.");
  RefreshList(u);
}
void Action(Ui *u, int id) {
  auto &store = WorkspaceAgentStore();
  auto agent = Agent(u);
  auto grant = store.humanWorkspace(agent);
  int mode = ModeValue(u);
  if (id == Refresh) {
    RefreshList(u);
    return;
  }
  auto key = Selected(u);
  if (id == Snapshot) {
    if (u->previewID != key || u->previewAgent != agent ||
        u->selectionWindow.empty() || grant.session != u->previewSession ||
        (mode != Outputs && mode != Captures))
      throw std::runtime_error(
          "Preview an explicitly shared output or capture first.");
    DWORD from = 0, to = 0;
    SendMessageW(u->before, EM_GETSEL, reinterpret_cast<WPARAM>(&from),
                 reinterpret_cast<LPARAM>(&to));
    auto text = Text(u->before);
    if (from >= to || to > text.size())
      throw std::runtime_error(
          "Select the exact output text to share in the left preview.");
    auto split = [&](size_t p) {
      return p > 0 && p < text.size() && text[p - 1] >= 0xD800 &&
             text[p - 1] <= 0xDBFF && text[p] >= 0xDC00 && text[p] <= 0xDFFF;
    };
    if (split(from) || split(to))
      throw std::runtime_error("Selection splits a Unicode character.");
    auto selected = Utf8(text.substr(from, to - from));
    auto context = store.attachSelection(agent, grant.session,
                                         u->selectionWindow, selected, Now());
    StatusText(u, "Selected completed output attached as immutable context " +
                      context);
    return;
  }
  if (mode == Editors) {
    uint64_t target = 0;
    PennEditorInfo actual;
    for (auto &e : PennEditorListSnapshots())
      if (EditorKey(e.documentID) == key) {
        target = e.documentID;
        actual = e;
      }
    if (!target)
      throw std::runtime_error("Editor closed.");
    if (id == Primary) {
      if (actual.source.size() > 32768 || actual.endpoint.empty())
        throw std::runtime_error(
            "Sharing requires a bound draft of at most 32 KiB.");
      store.authorize(agent, grant.session);
      if (!grant.windows.count(actual.endpoint))
        throw std::runtime_error("The original editor destination is not among "
                                 "this agent's explicit output grants.");
      Put(key, actual);
      store.shareEditor(agent, grant.session, key, true);
      u->registered[key] = target;
      StatusText(u, "Only the selected editor document is shared with " +
                        agent + ".");
    } else
      store.shareEditor(agent, grant.session, key, false);
  } else if (mode == Outputs && id == Primary) {
    auto name = Utf8(Text(u->detail));
    auto capture = store.startCapture(agent, grant.session, key, name);
    StatusText(u, "Explicit completed-output capture started: " + capture);
  } else if (mode == Proposals) {
    if (id == Primary) {
      Accept(u);
      return;
    }
    auto p = store.proposals().at(key);
    if (p.agent != agent)
      throw std::runtime_error("Proposal belongs to another agent.");
    if (p.status != "pending")
      throw std::runtime_error("Proposal already resolved or expired.");
    store.rejectEdit(key);
    u->previewCurrent = false;
    StatusText(u, "Proposal rejected. Source is unchanged.");
  } else if (mode == Tasks && id == Primary) {
    store.cancelTask(agent, key);
    StatusText(
        u,
        "Cancellation requested; waiting for external runner acknowledgment.");
  } else if (mode == Captures) {
    if (id == Primary) {
      Preview(u);
      return;
    }
    store.stopCapture(key);
    StatusText(u,
               "Capture stopped. Existing bounded records remain available.");
  } else if (mode == Drafts && id == Primary) {
    for (auto &d : store.drafts())
      if (d.id == key && d.agent == agent) {
        auto windows = store.humanWindows();
        if (!grant.session || !grant.windows.count(d.window) ||
            std::none_of(windows.begin(), windows.end(),
                         [&](const agent_workspace::Window &w) {
                           return w.id == d.window;
                         }))
          throw std::runtime_error("Draft destination access has expired.");
        if (!WorkspaceOpenDraft(u->window, d))
          throw std::runtime_error(
              "The original destination or staged scene binding has expired.");
        StatusText(
            u, "Draft opened in a local unsent editor. Nothing dispatched.");
      }
  }
  RefreshList(u);
}
void Layout(Ui *u, int width, int height) {
  const int pad = 8, top = 58, columns = width < 720 ? 2 : 4,
            rows = 4 / columns, bottom = 110 + (rows - 1) * 34;
  int body = std::max(30, height - top - bottom),
      left = std::max(100, width / 3),
      right = std::max(50, width - left - 3 * pad);
  MoveWindow(u->name, pad, pad, std::max(70, width / 3 - 16), 24, TRUE);
  MoveWindow(u->mode, left + pad, pad, std::max(80, width - left - 2 * pad),
             240, TRUE);
  MoveWindow(u->list, pad, 38, left - pad, body + 20, TRUE);
  int column = std::max(20, (right - pad) / 2);
  MoveWindow(GetDlgItem(u->window, 212), left + pad, 36, column, 20, TRUE);
  MoveWindow(GetDlgItem(u->window, 213), left + 2 * pad + column, 36,
             std::max(20, right - column - pad), 20, TRUE);
  MoveWindow(u->before, left + pad, top, column, body, TRUE);
  MoveWindow(u->after, left + 2 * pad + column, top,
             std::max(20, right - column - pad), body, TRUE);
  int y = top + body + pad;
  MoveWindow(u->detail, pad, y, std::max(80, width / 3 - pad), 24, TRUE);
  int button = std::max(60, (width - (columns + 1) * pad) / columns);
  for (int i = 0; i < 4; ++i)
    MoveWindow(GetDlgItem(u->window, Primary + i),
               pad + (i % columns) * (button + pad),
               y + 30 + (i / columns) * 34, button, 28, TRUE);
  MoveWindow(u->status, pad, y + 30 + rows * 34, std::max(20, width - 2 * pad),
             34, TRUE);
}
LRESULT CALLBACK Proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
  auto u = reinterpret_cast<Ui *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (msg == WM_NCCREATE) {
    u = static_cast<Ui *>(reinterpret_cast<CREATESTRUCT *>(l)->lpCreateParams);
    u->window = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(u));
  }
  if (!u)
    return DefWindowProcW(h, msg, w, l);
  try {
    switch (msg) {
    case WM_CREATE:
      u->name =
          Control(u, L"EDIT", L"Assistant", ES_AUTOHSCROLL | WS_TABSTOP, Name);
      SendMessageW(u->name, EM_SETLIMITTEXT, 64, 0);
      u->mode =
          Control(u, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, Mode);
      for (auto title : {L"Editor sharing", L"Shared output / snapshots",
                         L"Editor proposals / full diff", L"Tasks / heartbeat",
                         L"Debug captures", L"Unsent drafts"})
        SendMessageW(u->mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(title));
      SendMessageW(u->mode, CB_SETCURSEL, 0, 0);
      u->list = Control(u, L"LISTBOX", L"",
                        LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, List);
      u->before =
          Control(u, L"EDIT", L"",
                  ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                      WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
                  Before);
      u->after =
          Control(u, L"EDIT", L"",
                  ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                      WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
                  After);
      SendMessageW(u->before, EM_SETLIMITTEXT, 8 * 1024 * 1024, 0);
      SendMessageW(u->after, EM_SETLIMITTEXT, 8 * 1024 * 1024, 0);
      u->detail = Control(u, L"EDIT", L"Debug capture",
                          ES_AUTOHSCROLL | WS_TABSTOP, Detail);
      SendMessageW(u->detail, EM_SETLIMITTEXT, 128, 0);
      Control(u, L"BUTTON", L"Share selected editor",
              BS_PUSHBUTTON | WS_TABSTOP, Primary);
      Control(u, L"BUTTON", L"Unshare editor", BS_PUSHBUTTON | WS_TABSTOP,
              Secondary);
      Control(u, L"BUTTON", L"Share selected text", BS_PUSHBUTTON | WS_TABSTOP,
              Snapshot);
      Control(u, L"BUTTON", L"Refresh", BS_PUSHBUTTON | WS_TABSTOP, Refresh);
      Control(u, L"STATIC", L"Current / completed output", 0, 212);
      Control(u, L"STATIC", L"Proposed / details", 0, 213);
      u->status = Control(
          u, L"STATIC",
          L"Select explicit existing output grants and individual editor tabs.",
          0, Status);
      SetTimer(h, 1, 500, 0);
      RefreshList(u);
      return 0;
    case WM_GETMINMAXINFO: {
      auto bounds = reinterpret_cast<MINMAXINFO *>(l);
      bounds->ptMinTrackSize.x = 420;
      bounds->ptMinTrackSize.y = 360;
      return 0;
    }
    case WM_SIZE:
      Layout(u, LOWORD(l), HIWORD(l));
      return 0;
    case WM_TIMER:
      RefreshList(u);
      return 0;
    case WM_COMMAND:
      if (LOWORD(w) == Mode && HIWORD(w) == CBN_SELCHANGE) {
        u->previewCurrent = false;
        u->previewID.clear();
        SetWindowTextW(u->before, L"");
        SetWindowTextW(u->after, L"");
        RefreshList(u);
      } else if (LOWORD(w) == List && HIWORD(w) == LBN_SELCHANGE)
        Preview(u);
      else if (LOWORD(w) >= Primary && LOWORD(w) <= Refresh &&
               HIWORD(w) == BN_CLICKED)
        Action(u, LOWORD(w));
      else if (LOWORD(w) == Name && HIWORD(w) == EN_CHANGE) {
        u->previewCurrent = false;
        RefreshList(u);
      }
      return 0;
    case WM_CLOSE:
      DestroyWindow(h);
      return 0;
    case WM_NCDESTROY:
      KillTimer(h, 1);
      for (auto &p : u->registered)
        WorkspaceAgentStore().closeEditor(p.first);
      SetWindowLongPtrW(h, GWLP_USERDATA, 0);
      Pane = 0;
      delete u;
      return DefWindowProcW(h, msg, w, l);
    }
  } catch (const std::exception &e) {
    StatusText(u, e.what());
    return 0;
  }
  return DefWindowProcW(h, msg, w, l);
}
} // namespace
HWND OpenAgentReviewPane(HWND parent) {
  if (Pane && IsWindow(Pane)) {
    ShowWindow(Pane, SW_SHOW);
    SetForegroundWindow(Pane);
    return Pane;
  }
  WNDCLASSW cls = {};
  cls.hInstance = GetModuleHandleW(0);
  cls.lpszClassName = L"MUSHclientAgentReview";
  cls.lpfnWndProc = Proc;
  cls.hCursor = LoadCursor(0, IDC_ARROW);
  cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  RegisterClassW(&cls);
  auto u = new Ui;
  Pane = CreateWindowExW(0, cls.lpszClassName,
                         L"Agent review - explicit sharing and local proposals",
                         WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                         1040, 720, parent, 0, cls.hInstance, u);
  if (Pane)
    ShowWindow(Pane, SW_SHOW);
  return Pane;
}
