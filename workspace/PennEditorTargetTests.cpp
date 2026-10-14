// Native own-window fixture. Build this TU plus PennLanguage.cpp; do not also
// compile PennEditorWindow.cpp. Run beside Scintilla.dll in a fictional Temp dir.
#define NOMINMAX
#include <windows.h>
int ClipboardAttempts = 0;
BOOL TargetTestOpenClipboard(HWND) {
  ++ClipboardAttempts;
  return FALSE;
}
// Observe review authorization without touching the user's clipboard.
#define OpenClipboard TargetTestOpenClipboard
#include "PennEditorWindow.cpp"
#undef OpenClipboard
#include <cassert>
#include <iostream>

namespace {
HWND FixtureOwner = 0;
std::wstring ExpectedDialog;
std::function<void()> DialogAction;
int DialogAnswer = IDOK;
bool DialogSeen = false;
constexpr UINT FixtureDialog = WM_APP + 731;
LRESULT CALLBACK FixtureProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR,
                             DWORD_PTR) {
  if (msg == FixtureDialog) {
    auto dialog = reinterpret_cast<HWND>(l);
    assert(h == FixtureOwner && GetWindow(dialog, GW_OWNER) == h);
    if (w && DialogAction)
      DialogAction();
    if (w)
      PostMessageW(dialog, WM_COMMAND, DialogAnswer, 0);
    else
      PostMessageW(dialog, WM_CLOSE, 0, 0);
    return 0;
  }
  return DefSubclassProc(h, msg, w, l);
}
LRESULT CALLBACK FixtureHook(int code, WPARAM w, LPARAM l) {
  if (code == HCBT_ACTIVATE) {
    auto dialog = reinterpret_cast<HWND>(w);
    wchar_t title[256] = {};
    GetWindowTextW(dialog, title, 256);
    if (GetWindow(dialog, GW_OWNER) == FixtureOwner) {
      if (ExpectedDialog == title) {
        DialogSeen = true;
        PostMessageW(FixtureOwner, FixtureDialog, 1,
                     reinterpret_cast<LPARAM>(dialog));
      } else {
        // Only this fixture's expected post-review rejection is dismissible.
        assert(std::wstring(title) == L"Prepared commands");
        PostMessageW(FixtureOwner, FixtureDialog, 0,
                     reinterpret_cast<LPARAM>(dialog));
      }
    }
  }
  return CallNextHookEx(0, code, w, l);
}
void Dialog(HWND owner, const wchar_t *title, const std::function<void()> &run,
            const std::function<void()> &action = {}, int answer = IDOK) {
  FixtureOwner = owner;
  ExpectedDialog = title;
  DialogAction = action;
  DialogAnswer = answer;
  DialogSeen = false;
  assert(SetWindowSubclass(owner, FixtureProc, 731, 0));
  auto hook = SetWindowsHookExW(WH_CBT, FixtureHook, 0, GetCurrentThreadId());
  assert(hook);
  run();
  assert(DialogSeen);
  UnhookWindowsHookEx(hook);
  RemoveWindowSubclass(owner, FixtureProc, 731);
  FixtureOwner = 0;
  DialogAction = {};
}
void Targets(Editor *e, const wchar_t *object, const wchar_t *attribute) {
  SetWindowTextW(e->object, object);
  SetWindowTextW(e->attribute, attribute);
}
void Check(Editor *e, const std::string &object, const std::string &attribute,
           const std::string &source) {
  Sync(e);
  assert(Utf8(e->object) == object && Utf8(e->attribute) == attribute);
  const auto &draft = e->docs[e->active].draft;
  assert(draft.object == object && draft.attribute == attribute);
  assert(draft.source == source && Text(e) == source);
}
std::string Base64(const std::string &s) {
  DWORD size = 0;
  assert(CryptBinaryToStringA(reinterpret_cast<const BYTE *>(s.data()),
                              s.size(), CRYPT_STRING_BASE64 |
                                            CRYPT_STRING_NOCRLF,
                              0, &size));
  std::string result(size, '\0');
  assert(CryptBinaryToStringA(reinterpret_cast<const BYTE *>(s.data()),
                              s.size(), CRYPT_STRING_BASE64 |
                                            CRYPT_STRING_NOCRLF,
                              &result[0], &size));
  result.resize(size);
  return result;
}
} // namespace

int main() {
  std::cout << std::unitbuf;
  std::wcout << std::unitbuf;
  wchar_t executable[32768] = {};
  GetModuleFileNameW(0, executable, 32768);
  std::wstring folder = executable;
  folder.resize(folder.find_last_of(L"\\/"));
  assert(PennEditorConfigureStorage(folder + L"\\target-state"));
  auto window = OpenPennEditor(0, "fictional-targets/session-1", "original",
                               "Fictional World");
  assert(window);
  ShowWindow(window, SW_HIDE);
  auto e = State(window);
  auto duplicate = OpenPennEditor(0, "fictional-duplicate/session-1", "",
                                  "Fictional World");
  auto sameDestination = OpenPennEditor(
      0, "fictional-targets/session-1", "", "Fictional World");
  assert(duplicate && sameDestination);
  ShowWindow(duplicate, SW_HIDE);
  ShowWindow(sameDestination, SW_HIDE);
  assert(Destination(e).find("Fictional World (session ") == 0);
  assert(Destination(e) != Destination(State(duplicate)));
  assert(Destination(e) == Destination(State(sameDestination)));
  assert(Utf8(window).find(Destination(e)) != std::string::npos);
  assert(Utf8(e->status).find("fictional-targets/session-1") == std::string::npos);
  PennEditorInvalidateEndpoint("fictional-duplicate/session-1");
  assert(Utf8(duplicate).find("Fictional World") != std::string::npos);
  assert(Utf8(duplicate).find("disconnected") != std::string::npos);
  assert(State(duplicate)->endpoint.empty());
  DestroyWindow(duplicate);
  DestroyWindow(sameDestination);
  std::cout << "PASS friendly destination captions, duplicate names and disconnection\n";
  Sci(e, SCI_SETSAVEPOINT);
  const auto initial = e->docs[0].draft.revision;
  Targets(e, L"#123", L"TEST_ATTR");
  assert(e->docs[0].draft.revision == initial + 2);
  assert(!Sci(e, SCI_GETMODIFY));
  const auto targetRevision = e->docs[0].draft.revision;
  Command(e, New);
  Check(e, "", "", "");
  Targets(e, L"#456", L"OTHER_ATTR");
  const auto secondRevision = e->docs[1].draft.revision;
  Select(e, 0);
  Check(e, "#123", "TEST_ATTR", "original");
  assert(e->docs[0].draft.revision == targetRevision);
  assert(!Sci(e, SCI_GETMODIFY));
  Select(e, 1);
  Check(e, "#456", "OTHER_ATTR", "");
  assert(e->docs[1].draft.revision == secondRevision);
  // Closing this empty neighbor must not copy its fields onto the survivor.
  Command(e, CloseDraft);
  assert(e->docs.size() == 1);
  Check(e, "#123", "TEST_ATTR", "original");
  assert(e->docs[0].draft.revision == targetRevision);
  std::cout << "PASS new, tab return, neighboring close and clean revision\n";

  int reads = 0, sends = 0;
  std::string response, sent;
  PennEditorSetTransport([&](const std::string &endpoint,
                              const std::string &command) {
    assert(endpoint == "fictional-targets/session-1");
    ++reads;
    const auto end = command.find(' ', 6);
    response = command.substr(6, end - 6) + " " + Base64("loaded");
    return true;
  });
  PennEditorSetSendPrepared([&](const std::string &endpoint, uint64_t id,
                                 uint64_t revision,
                                 const std::vector<std::string> &commands) {
    assert(endpoint == "fictional-targets/session-1");
    assert(id == e->docs[e->active].id);
    assert(revision == e->docs[e->active].draft.revision);
    assert(commands.size() == 1);
    sent = commands[0];
    ++sends;
    return true;
  });
  Dialog(window, L"Approve explicit read request",
         [&] { Command(e, ReadAttribute); });
  assert(reads == 1);
  // A valid reply can arrive for a background draft after mere navigation.
  Add(e, "background neighbor");
  assert(PennEditorHandleOutput(e->endpoint, response));
  Select(e, 0);
  Check(e, "#123", "TEST_ATTR", "loaded");
  Dialog(window, L"Approve explicit read request",
         [&] { Command(e, ReadAttribute); });
  const auto requested = e->docs[0].draft.revision;
  Targets(e, L"#999", L"CHANGED");
  Targets(e, L"#123", L"TEST_ATTR");
  assert(e->docs[0].draft.revision > requested);
  Select(e, 1);
  assert(PennEditorHandleOutput(e->endpoint, response));
  Select(e, 0);
  Check(e, "#123", "TEST_ATTR", "loaded");
  assert(!e->read.consumed);
  std::cout << "PASS outstanding background read accepts unchanged draft and rejects target edit/revert\n";

  const auto beforeApproval = reads;
  Dialog(window, L"Approve explicit read request",
         [&] { Command(e, ReadAttribute); },
         [&] { Targets(e, L"#777", L"TEST_ATTR"); });
  assert(reads == beforeApproval);
  Targets(e, L"#123", L"TEST_ATTR");
  Dialog(window, L"Review and send prepared commands",
         [&] { Command(e, SendCommands); }, [&] {
           Targets(e, L"#777", L"TEST_ATTR");
           Targets(e, L"#123", L"TEST_ATTR");
         });
  assert(sends == 0);
  Dialog(window, L"Review and send prepared commands",
         [&] { Command(e, SendCommands); }, [&] { Add(e, "modal new"); });
  assert(sends == 0);
  Select(e, 0);
  Dialog(window, L"Review and send prepared commands",
         [&] { Command(e, SendCommands); });
  assert(sends == 1 && sent == "&TEST_ATTR #123=loaded");
  Dialog(window, L"Review exact command (unsent)",
         [&] { Command(e, Review); }, [&] {
           Targets(e, L"#777", L"TEST_ATTR");
           Targets(e, L"#123", L"TEST_ATTR");
         });
  assert(ClipboardAttempts == 0);
  Dialog(window, L"Review exact command (unsent)",
         [&] { Command(e, Review); });
  assert(ClipboardAttempts == 1);
  std::cout << "PASS approved read/send revalidation and exact attribute destination\n";

  // Target changes also stale prepared reviews and source proposals, without
  // turning target edits into Scintilla source edits or undo records.
  const auto proposalRevision = e->docs[0].draft.revision;
  PennIDE::Review review;
  std::string error;
  assert(PennIDE::PrepareReview(e->docs[0].draft, review, error));
  Targets(e, L"#321", L"TEST_ATTR");
  assert(!PennIDE::ReviewCurrent(e->docs[0].draft, review, e->endpoint));
  assert(!PennEditorApplyProposal(window, proposalRevision, 0, 6, "wrong"));
  bool applied = true;
  const auto freshRevision = e->docs[0].draft.revision;
  Dialog(window, L"Review proposed edit", [&] {
    applied = PennEditorApplyProposal(window, freshRevision, 0, 6, "wrong");
  }, [&] { Targets(e, L"#123", L"TEST_ATTR"); });
  assert(!applied);
  Check(e, "#123", "TEST_ATTR", "loaded");
  const auto beforeNew = e->docs[0].draft.revision;
  Dialog(window, L"Review proposed edit", [&] {
    applied = PennEditorApplyProposal(window, beforeNew, 0, 6, "wrong");
  }, [&] { Add(e, "proposal modal neighbor"); });
  assert(!applied);
  Select(e, 0);
  Check(e, "#123", "TEST_ATTR", "loaded");
  Sci(e, SCI_SETSAVEPOINT);
  Targets(e, L"#456", L"TEST_ATTR");
  assert(!Sci(e, SCI_GETMODIFY));
  const auto cleanRevision = e->docs[0].draft.revision;
  Sci(e, SCI_APPENDTEXT, 1, reinterpret_cast<sptr_t>("!"));
  Sync(e);
  assert(Sci(e, SCI_GETMODIFY));
  assert(e->docs[0].draft.revision > cleanRevision);
  Sci(e, SCI_UNDO);
  Sync(e);
  Check(e, "#456", "TEST_ATTR", "loaded");
  assert(!Sci(e, SCI_GETMODIFY));
  assert(e->docs[0].draft.revision > cleanRevision + 1);
  std::cout << "PASS stale review/proposal, source dirty state and undo\n";

  // Recovery remains deliberately source-only: no accidental target rebinding.
  const auto path = folder + L"\\targets.recovery";
  assert(PennEditorSaveRecoveryFile(path));
  DestroyWindow(window);
  auto recovered = OpenPennEditor(0, "fictional-recovered/session-2", "");
  assert(recovered);
  ShowWindow(recovered, SW_HIDE);
  Dialog(recovered, L"Review draft recovery", [&] {
    assert(PennEditorRestoreRecoveryFile(recovered, path));
  });
  auto r = State(recovered);
  for (const auto &document : r->docs)
    assert(document.draft.object.empty() && document.draft.attribute.empty());
  std::cout << "PASS recovery never rebinds targets\n";
  DestroyWindow(recovered);
  std::cout << "All target regressions passed.\n";
}
