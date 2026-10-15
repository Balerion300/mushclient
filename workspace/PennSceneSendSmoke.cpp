// Native own-window scene-to-editor acceptance with the real Store and editor.
// The runner supplies the exact production prepared-dispatch function body;
// only the destination document's transport is a fictional in-memory fixture.
#define NOMINMAX
#include "PennEditorWindow.cpp"
#include "../AgentWorkspaceCore.h"
#include <cassert>
#include <crtdbg.h>
#include <iostream>

enum { eConnectConnectedToMud = 1 };
struct Identity { std::string id; };
struct FixtureQueue {
  size_t size = 0;
  size_t GetCount() const { return size; }
};
struct CMUSHclientDoc {
  int m_iConnectPhase = eConnectConnectedToMud;
  long m_nTotalLinesSent = 0;
  FixtureQueue m_QueuedCommandsList;
  unsigned calls = 0;
  bool dispatch = true;
  std::function<void()> callback;
  void SendMsg(const char *, bool, bool, bool) {
    ++calls;
    if (callback) callback();
    if (dispatch) ++m_nTotalLinesSent;
  }
};
std::map<CMUSHclientDoc *, Identity> identities;
std::map<unsigned __int64, std::string> sceneDraftBindings;
agent_workspace::Store agentStore;
bool encodeCommand(CMUSHclientDoc *, const std::string &source, std::string &out) {
  out = source;
  return true;
}
#include "prepared_editor_dispatch_body.inc"

namespace {
HWND FixtureOwner = 0;
std::string ExpectedCommand;
std::function<void()> DuringReview;
int ReviewAnswer = IDOK;
unsigned Reviews = 0, Warnings = 0;
constexpr UINT AnswerDialog = WM_APP + 735;
LRESULT CALLBACK FixtureProc(HWND h, UINT message, WPARAM w, LPARAM l,
                             UINT_PTR, DWORD_PTR) {
  if (message == AnswerDialog) {
    auto dialog = reinterpret_cast<HWND>(l);
    assert(GetWindow(dialog, GW_OWNER) == h && h == FixtureOwner);
    if (w) {
      const auto preview = Utf8(GetDlgItem(dialog, 10));
      assert(preview.find("Destination: Fictional A (session ") != std::string::npos);
      assert(preview.find("\n" + ExpectedCommand + "\n") != std::string::npos);
      ++Reviews;
      if (DuringReview) DuringReview();
      PostMessageW(dialog, WM_COMMAND, ReviewAnswer, 0);
    } else {
      ++Warnings;
      PostMessageW(dialog, WM_CLOSE, 0, 0);
    }
    return 0;
  }
  return DefSubclassProc(h, message, w, l);
}
LRESULT CALLBACK FixtureHook(int code, WPARAM w, LPARAM l) {
  if (code == HCBT_ACTIVATE) {
    auto dialog = reinterpret_cast<HWND>(w);
    if (GetWindow(dialog, GW_OWNER) == FixtureOwner) {
      auto title = Utf8(dialog);
      assert(title == "Review and send prepared commands" || title == "Prepared commands");
      PostMessageW(FixtureOwner, AnswerDialog,
                   title == "Review and send prepared commands", reinterpret_cast<LPARAM>(dialog));
    }
  }
  return CallNextHookEx(0, code, w, l);
}
void ReviewSend(HWND window, const std::string &command,
                const std::function<void()> &during = {}, int answer = IDOK) {
  FixtureOwner = window;
  ExpectedCommand = command;
  DuringReview = during;
  ReviewAnswer = answer;
  const auto before = Reviews;
  assert(SetWindowSubclass(window, FixtureProc, 735, 0));
  auto hook = SetWindowsHookExW(WH_CBT, FixtureHook, 0, GetCurrentThreadId());
  assert(hook);
  SendMessageW(window, WM_COMMAND, SendCommands, 0);
  assert(Reviews == before + 1);
  UnhookWindowsHookEx(hook);
  if (IsWindow(window)) RemoveWindowSubclass(window, FixtureProc, 735);
  FixtureOwner = 0;
  DuringReview = {};
}
struct Case {
  CMUSHclientDoc doc;
  std::string scene, action, draft, command;
  HWND editor = 0;
  uint64_t document = 0;
  Case() {
    agentStore = agent_workspace::Store();
    identities.clear();
    sceneDraftBindings.clear();
    identities[&doc].id = "fictional-scene-world-A";
    agentStore.addWindow("fictional-scene-world-A", "Fictional A", "Main");
    auto session = agentStore.enable("Fixture", {"fictional-scene-world-A"}, true);
    scene = agentStore.createScene("Native final-send acceptance");
    agent_workspace::Npc npc;
    npc.id = "guard"; npc.name = "Fixture Guard"; npc.agent = "Fixture";
    npc.window = "fictional-scene-world-A"; npc.posePrefix = ":";
    agentStore.assignNpc(scene, npc);
    agentStore.directScene(scene, "", "Observe the gate.", "guard", "running");
    auto proposed = agentStore.proposeAction("Fixture", session, scene, "guard",
        agentStore.humanScene(scene).revision, "pose", "Fixture Guard waits.", "once");
    action = proposed.id;
    agentStore.editAction(scene, action, "Fixture Guard studies the gate.");
    auto staged = agentStore.stageAction(scene, action);
    draft = staged.id; command = staged.text;
    editor = OpenPennEditor(0, "fictional-scene-world-A", command, "Fictional A");
    assert(editor);
    ShowWindow(editor, SW_HIDE);
    PennEditorInfo info;
    assert(PennEditorSnapshotInfo(editor, info));
    document = info.documentID;
    sceneDraftBindings[document] = draft;
    PennEditorSetSendPrepared([this](const std::string &endpoint, uint64_t id,
                                      uint64_t revision, const std::vector<std::string> &commands) {
      PennEditorInfo current;
      assert(PennEditorSnapshotInfo(editor, current));
      assert(id == document && revision == current.revision);
      return WorkspaceSendPreparedEditor(endpoint, id, revision, commands);
    });
    assert(doc.calls == 0 && status() == "staged");
  }
  ~Case() {
    if (IsWindow(editor)) DestroyWindow(editor);
    PennEditorSetSendPrepared({});
  }
  std::string status() const { return agentStore.humanScene(scene).actions.front().status; }
};
} // namespace

int main(int argc, char **argv) {
  _set_error_mode(_OUT_TO_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  std::cout << std::unitbuf;
  assert(argc == 2 && PennEditorConfigureStorage(Wide(argv[1])));
  {
    Case c;
    ReviewSend(c.editor, c.command);
    assert(c.doc.calls == 1 && c.status() == "queued");
    const auto warnings = Warnings;
    ReviewSend(c.editor, c.command);
    assert(c.doc.calls == 1 && Warnings == warnings + 1);
    std::cout << "PASS human-edited scene prose remains unsent until exact native review, sends once, and rejects replay\n";
  }
  {
    Case c;
    ReviewSend(c.editor, c.command, {}, IDCANCEL);
    assert(c.doc.calls == 0 && c.status() == "staged");
    std::cout << "PASS cancelled final native review preserves staged action without dispatch\n";
  }
  {
    Case c;
    ReviewSend(c.editor, c.command, [&] {
      agentStore.directScene(c.scene, "", "Changed during review", "guard", "paused");
    });
    assert(c.doc.calls == 0 && c.status() == "expired");
    std::cout << "PASS scene revision/state changes during native review prevent dispatch\n";
  }
  {
    Case c;
    const auto changed = c.command + " Edited after staging.";
    Sci(State(c.editor), SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(changed.c_str()));
    ReviewSend(c.editor, changed);
    assert(c.doc.calls == 0 && c.status() == "staged");
    std::cout << "PASS editor source changed after scene staging cannot bypass its exact-command binding\n";
  }
  {
    Case c;
    c.doc.callback = [&] { sceneDraftBindings.erase(c.document); };
    ReviewSend(c.editor, c.command);
    assert(c.doc.calls == 1 && c.status() == "queued");
    std::cout << "PASS send callback can erase the original document binding without invalid iterator access\n";
  }
  {
    Case c;
    c.doc.callback = [&] {
      agentStore.directScene(c.scene, "", "Changed during send", "guard", "paused");
    };
    const auto warnings = Warnings;
    ReviewSend(c.editor, c.command);
    assert(c.doc.calls == 1 && c.status() == "expired" && Warnings == warnings + 1);
    std::cout << "PASS scene expiry during dispatch returns an unconfirmed warning without an uncaught exception\n";
  }
  {
    Case c;
    c.doc.dispatch = false;
    c.doc.callback = [&] { DestroyWindow(c.editor); };
    const auto warnings = Warnings;
    ReviewSend(c.editor, c.command);
    assert(!IsWindow(c.editor) && c.doc.calls == 1 && Warnings == warnings);
    std::cout << "PASS sender destroys editor and declines dispatch without dereferencing its deleted state\n";
  }
  assert(Windows.empty());
  std::cout << "All native scene/editor acceptance checks passed; no live client or network was used.\n";
}
