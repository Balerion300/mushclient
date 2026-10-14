#define NOMINMAX
// Own-window fixtures include the production implementation to inspect only
// fictional recovery identities and route actual timer/close messages.
#include "PennEditorWindow.cpp"
#include <cassert>
#include <crtdbg.h>
#include <iostream>
#include <thread>

std::thread Answer(HWND owner, const wchar_t *title, int answer) {
  return std::thread([owner, title, answer] {
    for (int i = 0; i < 1000; ++i) {
      auto dialog = FindWindowW(L"#32770", title);
      if (dialog && GetWindow(dialog, GW_OWNER) == owner) {
        auto button = GetDlgItem(dialog, answer);
        // Native MB_OK boxes expose their sole OK button as IDCANCEL.
        if (answer == IDOK && !button)
          button = GetDlgItem(dialog, IDCANCEL);
        if (button) {
          PostMessageW(button, BM_CLICK, 0, 0);
          return;
        }
      }
      Sleep(5);
    }
    assert(!"Fixture dialog did not appear");
  });
}
void Close(HWND window, int answer) {
  auto reply = Answer(window, L"Unsent draft", answer);
  SendMessageW(window, WM_CLOSE, 0, 0);
  reply.join();
}
HWND Fixture(const std::string &world, const std::string &source) {
  auto window = OpenPennEditor(0, world, source);
  assert(window);
  ShowWindow(window, SW_HIDE);
  return window;
}
std::vector<PennIDE::RecoveryEntry> Entries() {
  std::vector<PennIDE::RecoveryEntry> entries;
  assert(LoadRecovery(Recovery(), entries));
  return entries;
}
std::string Bytes() {
  std::ifstream file(Recovery(), std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}
bool Has(const std::vector<PennIDE::RecoveryEntry> &entries,
         const std::string &source) {
  return std::any_of(entries.begin(), entries.end(),
                     [&](const PennIDE::RecoveryEntry &entry) {
                       return entry.source == source;
                     });
}
int wmain(int argc, wchar_t **argv) {
  _set_error_mode(_OUT_TO_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  assert(argc == 3); // Caller supplies its fresh Temp fixture folder and phase.
  assert(PennEditorConfigureStorage(argv[1]));
  const std::wstring phase = argv[2];
  if (phase == L"seed") {
    auto a = Fixture("fictional-A", "KEEP-A");
    auto b = Fixture("fictional-B", "KEEP-B");
    auto aIdentity = State(a)->docs[0].recoveryId;
    auto bIdentity = State(b)->docs[0].recoveryId;
    assert(aIdentity != bIdentity);
    Close(a, IDYES);
    assert(!IsWindow(a));
    SendMessageW(b, WM_TIMER, 2, 0);
    auto entries = Entries();
    assert(entries.size() == 2 && Has(entries, "KEEP-A") && Has(entries, "KEEP-B"));
    assert(entries[0].id == aIdentity && entries[1].id == bIdentity);
    Close(b, IDYES);
    assert(!IsWindow(b));
    auto blank = Fixture("fictional-C", "");
    SendMessageW(blank, WM_TIMER, 2, 0);
    assert(Entries().size() == 2);
    Close(blank, IDNO);
    assert(Entries().size() == 2);
    std::cout << "PASS close-kept A + B timer; kept final + fresh blank timer; blank discard\n";
  } else if (phase == L"restart") {
    auto original = Entries();
    assert(original.size() == 2);
    auto blank = Fixture("fictional-restart", "");
    SendMessageW(blank, WM_TIMER, 2, 0);
    assert(Entries().size() == 2);
    auto approved = Answer(blank, L"Review draft recovery", IDOK);
    assert(RestoreRecovery(State(blank)));
    approved.join();
    auto e = State(blank);
    assert(e->docs.size() == 3);
    assert(e->docs[1].recoveryId == original[0].id &&
           e->docs[2].recoveryId == original[1].id);
    assert(e->docs[1].id != e->docs[2].id);
    for (auto &doc : e->docs)
      assert(doc.draft.endpoint == "fictional-restart" &&
             doc.draft.object.empty() && doc.draft.attribute.empty());
    auto other = Fixture("fictional-other-world", "OTHER-OPEN");
    auto informed = Answer(other, L"Draft recovery", IDOK);
    assert(!RestoreRecovery(State(other)));
    informed.join();
    assert(State(other)->docs.size() == 1);
    assert(SaveRecovery());
    assert(Entries().size() == 3);
    Sci(e, SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("KEEP-B-EDITED"));
    assert(SaveRecovery());
    auto edited = Entries();
    assert(edited.size() == 3 && Has(edited, "KEEP-B-EDITED") && !Has(edited, "KEEP-B"));
    auto discard = Answer(blank, L"Close unsent draft", IDNO);
    Command(e, CloseDraft);
    discard.join();
    assert(Entries().size() == 2 && !Has(Entries(), "KEEP-B-EDITED"));
    Close(other, IDNO);
    assert(Entries().size() == 1 && Has(Entries(), "KEEP-A"));
    Close(blank, IDNO);
    assert(Entries().empty());
    std::cout << "PASS process restart before recovery; source-only restore; no duplicate live recover; stable edits; isolated tab/window discard\n";
  } else if (phase == L"limits") {
    std::vector<std::string> legacy;
    for (int i = 0; i < 32; ++i)
      legacy.push_back("LEGACY-RETAINED-" + std::to_string(i));
    std::string bytes;
    assert(PennIDE::EncodeRecovery(legacy, bytes));
    assert(Write(Recovery(), bytes));
    auto blank = Fixture("fictional-limit", "");
    assert(SaveRecovery());
    assert(Entries().size() == 32 && Entries()[0].id.compare(0, 7, "legacy-") == 0);
    auto preserved = Bytes();
    Add(State(blank), "OVER-CAP-DO-NOT-LOSE");
    assert(!SaveRecovery());
    assert(Bytes() == preserved);
    auto answer = Answer(blank, L"Unsent draft", IDYES);
    auto failure = Answer(blank, L"PennMUSH IDE", IDOK);
    SendMessageW(blank, WM_CLOSE, 0, 0);
    answer.join();
    failure.join();
    assert(IsWindow(blank) && Text(State(blank)) == "OVER-CAP-DO-NOT-LOSE");
    Close(blank, IDNO); // Discard the new draft; keep all 32 previous entries.
    assert(Entries().size() == 32);
    assert(Write(Recovery(), "malformed fictional bundle"));
    blank = Fixture("fictional-malformed", "NEW-SOURCE");
    assert(!SaveRecovery());
    assert(Bytes() == "malformed fictional bundle");
    DestroyWindow(blank);
    assert(Write(Recovery(), "PENNIDE2\n"));
    blank = Fixture("fictional-write-failure", "PRESERVE-ON-IO-FAILURE");
    auto temporary = Recovery() + L".tmp";
    assert(CreateDirectoryW(temporary.c_str(), 0));
    assert(!SaveRecovery());
    assert(Bytes() == "PENNIDE2\n" && Text(State(blank)) == "PRESERVE-ON-IO-FAILURE");
    assert(RemoveDirectoryW(temporary.c_str()));
    Close(blank, IDYES);
    assert(Has(Entries(), "PRESERVE-ON-IO-FAILURE"));
    auto target = Storage() + L"\\fictional-source.txt";
    assert(Write(target, "EXISTING-SOURCE-FILE"));
    HANDLE held = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, 0,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    assert(held != INVALID_HANDLE_VALUE);
    blank = Fixture("fictional-source-write-denied", "UNSAVED-SOURCE-MUST-STAY");
    assert(SaveRecovery());
    auto beforeWrite = Bytes();
    assert(!Write(target, Text(State(blank))));
    assert(IsWindow(blank) && Text(State(blank)) == "UNSAVED-SOURCE-MUST-STAY" &&
           Bytes() == beforeWrite && Has(Entries(), "UNSAVED-SOURCE-MUST-STAY"));
    CloseHandle(held);
    std::ifstream existing(target, std::ios::binary);
    assert(std::string(std::istreambuf_iterator<char>(existing), {}) == "EXISTING-SOURCE-FILE");
    Close(blank, IDYES);
    std::cout << "PASS legacy migration; 32-entry retention + blank; cap blocks keep without loss; malformed/failed writes preserve original\n";
    std::cout << "PASS deterministic source-write sharing denial preserves source file, open draft and recovery bytes\n";
  } else if (phase == L"isolation") {
    std::string bytes;
    assert(PennIDE::EncodeRecovery({"DEFAULT-LEGACY-A"}, bytes));
    assert(Write(Recovery(), bytes));
    auto imported = Storage() + L"\\fictional-import.recovery";
    assert(PennIDE::EncodeRecovery({"IMPORTED-LEGACY-B"}, bytes));
    assert(Write(imported, bytes));
    std::vector<PennIDE::RecoveryEntry> pending, custom;
    assert(LoadRecovery(Recovery(), pending) && LoadRecovery(imported, custom));
    assert(pending[0].id != custom[0].id);
    auto editor = Fixture("fictional-import", "");
    auto approved = Answer(editor, L"Review draft recovery", IDOK);
    assert(RestoreRecovery(State(editor), imported));
    approved.join();
    assert(SaveRecovery());
    assert(Entries().size() == 2 && Has(Entries(), "DEFAULT-LEGACY-A") &&
           Has(Entries(), "IMPORTED-LEGACY-B"));
    Close(editor, IDNO);
    assert(Entries().size() == 1 && Has(Entries(), "DEFAULT-LEGACY-A"));
    assert(PennIDE::EncodeRecovery({"REPLACED-LEGACY-C"}, bytes));
    assert(Write(imported, bytes));
    std::vector<PennIDE::RecoveryEntry> replaced;
    assert(LoadRecovery(imported, replaced) && replaced[0].id != custom[0].id);
    std::cout << "PASS unrelated legacy default/import identities; import autosave preserves both; replaced legacy file gets fresh identity\n";
  } else {
    assert(!"Unknown fixture phase");
  }
  assert(Windows.empty());
  return 0;
}
