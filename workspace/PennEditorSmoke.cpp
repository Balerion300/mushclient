#define NOMINMAX
#include "../third_party/scintilla/include/Scintilla.h"
#include "PennEditorWindow.h"
#include <cassert>
#include <commctrl.h>
#include <iostream>
#include <thread>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
// Synthetic fixtures only: these approval helpers operate exclusively on the
// window created by this test. The transport never connects to a live game.
std::thread Approve(HWND owner, const wchar_t *title) {
  return std::thread([owner, name = std::wstring(title)] {
    for (int i = 0; i < 500; ++i) {
      auto dialog = FindWindowW(L"#32770", name.c_str());
      if (dialog && GetWindow(dialog, GW_OWNER) == owner) {
        PostMessageW(dialog, WM_COMMAND, IDOK, 0);
        return;
      }
      Sleep(10);
    }
  });
}
std::string Base64(const std::string &text) {
  DWORD n = 0;
  assert(CryptBinaryToStringA(
      reinterpret_cast<const BYTE *>(text.data()), text.size(),
      CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, 0, &n));
  std::string out(n, '\0');
  assert(CryptBinaryToStringA(
      reinterpret_cast<const BYTE *>(text.data()), text.size(),
      CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &n));
  out.resize(n);
  return out;
}
LRESULT CompletionCount(HWND text) {
  HWND popup = 0;
  for (;;) {
    popup = FindWindowExW(0, popup, L"ListBoxX", 0);
    if (!popup)
      break;
    if (GetWindow(popup, GW_OWNER) == GetAncestor(text, GA_ROOT)) {
      auto list = FindWindowExW(popup, 0, WC_LISTBOXW, 0);
      return list ? SendMessageW(list, LB_GETCOUNT, 0, 0) : 0;
    }
  }
  return 0;
}
int main() {
  HWND editor = OpenPennEditor(0, "fixture-world/session-1",
                               "[add(1,2)] dragon \xF0\x9F\x90\x89");
  assert(editor);
  ShowWindow(editor, SW_HIDE);
  HWND text = FindWindowExW(editor, 0, L"Scintilla", 0);
  assert(text);
  assert(SendMessage(text, SCI_GETCODEPAGE, 0, 0) == SC_CP_UTF8);
  assert(SendMessage(text, SCI_GETMARGINWIDTHN, 0, 0) > 0);
  assert(SendMessage(text, SCI_GETSTYLEAT, 1, 0) == 1);
  // Real keyboard input must open a multi-choice dropdown without Ctrl+Space.
  SendMessage(text, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("["));
  SendMessage(text, SCI_GOTOPOS, 1, 0);
  for (char ch : std::string("REGEDIT"))
    SendMessage(text, WM_CHAR, ch, 1);
  assert(SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  assert(CompletionCount(text) == 4);
  assert(SendMessage(text, SCI_AUTOCGETIGNORECASE, 0, 0));
  assert(!SendMessage(text, SCI_AUTOCGETCHOOSESINGLE, 0, 0));
  char selected[128] = {};
  SendMessage(text, SCI_AUTOCGETCURRENTTEXT, 0,
              reinterpret_cast<LPARAM>(selected));
  assert(std::string(selected) == "regedit");
  SendMessage(text, WM_KEYDOWN, VK_DOWN, 1);
  SendMessage(text, SCI_AUTOCGETCURRENTTEXT, 0,
              reinterpret_cast<LPARAM>(selected));
  assert(std::string(selected) == "regeditall");
  SendMessage(text, WM_KEYDOWN, VK_TAB, 1);
  assert(!SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  std::string completed;
  uint64_t completionRevision = 0;
  std::string completionEndpoint;
  assert(PennEditorSnapshot(editor, completed, completionRevision,
                            completionEndpoint));
  assert(completed == "[regeditall");
  SendMessage(text, WM_CHAR, '(', 1);
  assert(!SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, WM_CHAR, 's', 1);
  SendMessage(text, WM_CHAR, 'o', 1);
  SendMessage(text, WM_CHAR, 'r', 1);
  SendMessage(text, WM_CHAR, 't', 1);
  assert(SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, WM_KEYDOWN, VK_RETURN, 1);
  assert(PennEditorSnapshot(editor, completed, completionRevision,
                            completionEndpoint));
  assert(completed == "[regeditall(sort");
  SendMessage(text, WM_CHAR, '(', 1);
  SendMessage(text, WM_CHAR, '1', 1);
  assert(!SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("regeditall"));
  SendMessage(text, SCI_GOTOPOS, 10, 0);
  SendMessage(editor, WM_COMMAND, 106, 0);
  assert(SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  for (int i = 0; i < 3; ++i)
    SendMessage(text, WM_KEYDOWN, VK_BACK, 1);
  SendMessage(text, SCI_AUTOCSELECT, 0,
              reinterpret_cast<LPARAM>("regediti"));
  SendMessage(text, SCI_AUTOCGETCURRENTTEXT, 0,
              reinterpret_cast<LPARAM>(selected));
  assert(std::string(selected) == "regediti");
  SendMessage(text, WM_KEYDOWN, VK_ESCAPE, 1);
  assert(!SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(""));
  for (char ch : std::string("@pe"))
    SendMessage(text, WM_CHAR, ch, 1);
  assert(SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, SCI_AUTOCGETCURRENTTEXT, 0,
              reinterpret_cast<LPARAM>(selected));
  assert(std::string(selected) == "@pemit");
  SendMessage(text, WM_KEYDOWN, VK_ESCAPE, 1);
  SendMessage(editor, WM_COMMAND, 114, 0); // Lua source mode.
  SendMessage(text, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(""));
  for (char ch : std::string("ret"))
    SendMessage(text, WM_CHAR, ch, 1);
  assert(SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, SCI_AUTOCGETCURRENTTEXT, 0,
              reinterpret_cast<LPARAM>(selected));
  assert(std::string(selected) == "return");
  SendMessage(editor, WM_COMMAND, 114, 0); // Back to PennMUSH.
  assert(!SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, SCI_SETTEXT, 0,
              reinterpret_cast<LPARAM>("[add(1,2)] dragon \xF0\x9F\x90\x89"));
  std::string source, endpoint;
  uint64_t revision = 0;
  assert(PennEditorSnapshot(editor, source, revision, endpoint));
  assert(source == "[add(1,2)] dragon \xF0\x9F\x90\x89");
  assert(endpoint == "fixture-world/session-1");
  auto firstRevision = revision;
  SendMessage(text, SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>("!"));
  assert(PennEditorSnapshot(editor, source, revision, endpoint));
  assert(source.back() == '!');
  assert(revision > firstRevision);
  SendMessage(text, SCI_UNDO, 0, 0);
  assert(PennEditorSnapshot(editor, source, revision, endpoint));
  assert(source.back() != '!');
  assert(!PennEditorApplyProposal(editor, firstRevision, 0, 1, "bad"));
  SendMessage(editor, WM_COMMAND, 101, 0);
  assert(PennEditorSnapshot(editor, source, revision, endpoint));
  assert(source.empty());
  assert(revision != firstRevision);
  PennEditorInfo info;
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.documentID > 0 && info.source.empty());
  int sends = 0;
  PennEditorSetTransport([&](const std::string &, const std::string &) {
    ++sends;
    return false;
  });
  PennEditorHandleOutput("fixture-world/session-1", "unsolicited source");
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.source.empty());
  assert(sends == 0);
  auto snapshots = PennEditorListSnapshots();
  assert(snapshots.size() == 2);
  auto original = snapshots[0];
  auto second = snapshots[1];
  auto approve = Approve(editor, L"Review proposed edit");
  assert(PennEditorApplyDocumentProposal(original.documentID, original.revision,
                                         "targeted first draft"));
  approve.join();
  snapshots = PennEditorListSnapshots();
  assert(snapshots[0].source == "targeted first draft");
  assert(snapshots[1].source == second.source);
  assert(!PennEditorApplyDocumentProposal(
      original.documentID, original.revision, "stale overwrite"));
  assert(sends == 0);
  HWND object = FindWindowExW(editor, 0, L"Edit", 0),
       attribute = FindWindowExW(editor, object, L"Edit", 0);
  assert(object && attribute);
  SetWindowTextW(object, L"#123");
  SetWindowTextW(attribute, L"CODE");
  const std::string loaded = "[add(3,4)]\nUnicode dragon \xF0\x9F\x90\x89";
  std::string response;
  std::string lastCommand;
  PennEditorSetTransport(
      [&](const std::string &target, const std::string &command) {
        assert(target == "fixture-world/session-1");
        ++sends;
        lastCommand = command;
        auto end = command.find(' ', 6);
        auto nonce = command.substr(6, end - 6);
        response = nonce + " " +
                   (command.find("encode64(get(") != std::string::npos
                        ? Base64(loaded)
                        : "ADD CUSTOM_FIXTURE ADD");
        assert(!PennEditorHandleOutput("wrong-world", response));
        assert(!PennEditorHandleOutput(
            target, "ffffffffffffffffffffffffffffffff unrelated"));
        assert(PennEditorHandleOutput(target, response));
        return true;
      });
  approve = Approve(editor, L"Approve explicit read request");
  SendMessage(editor, WM_COMMAND, 115, 0);
  approve.join();
  assert(sends == 1);
  assert(lastCommand.find("[encode64(get(#123/CODE))]") != std::string::npos);
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.source == loaded);
  auto loadedRevision = info.revision;
  assert(PennEditorHandleOutput("fixture-world/session-1", response));
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.revision == loadedRevision);
  approve = Approve(editor, L"Approve explicit read request");
  SendMessage(editor, WM_COMMAND, 116, 0);
  approve.join();
  assert(sends == 2);
  int preparedSends = 0;
  assert(lastCommand.find("[functions()]") != std::string::npos);
  SendMessage(text, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(""));
  for (char ch : std::string("CUSTOM_F"))
    SendMessage(text, WM_CHAR, ch, 1);
  assert(SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, SCI_AUTOCGETCURRENTTEXT, 0,
              reinterpret_cast<LPARAM>(selected));
  assert(std::string(selected) == "custom_fixture");
  SendMessage(text, WM_KEYDOWN, VK_ESCAPE, 1);
  SendMessage(text, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(loaded.c_str()));
  std::vector<std::string> pendingNonces;
  PennEditorSetTransport(
      [&](const std::string &target, const std::string &command) {
        assert(target == "fixture-world/session-1");
        auto end = command.find(' ', 6);
        pendingNonces.push_back(command.substr(6, end - 6));
        return true;
      });
  approve = Approve(editor, L"Approve explicit read request");
  SendMessage(editor, WM_COMMAND, 115, 0);
  approve.join();
  assert(pendingNonces.size() == 1);
  SendMessage(text, SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>("!"));
  PennEditorInfo stale;
  assert(PennEditorSnapshotInfo(editor, stale));
  auto staleReply = pendingNonces[0] + " " + Base64("private stale source");
  assert(PennEditorHandleOutput("fixture-world/session-1", staleReply));
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.source == stale.source && info.revision == stale.revision);
  assert(!PennEditorHandleOutput("wrong-world", staleReply));
  approve = Approve(editor, L"Approve explicit read request");
  SendMessage(editor, WM_COMMAND, 115, 0);
  approve.join();
  assert(pendingNonces.size() == 2);
  assert(PennEditorHandleOutput("fixture-world/session-1", staleReply));
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.source == stale.source);
  assert(PennEditorHandleOutput("fixture-world/session-1",
                                pendingNonces[1] + " not-valid-base64!"));
  assert(PennEditorHandleOutput("fixture-world/session-1",
                                pendingNonces[0] + " " +
                                    std::string(100001, 'X')));
  assert(sends == 2 && preparedSends == 0);
  const std::string closedEndpoint = "fixture-closed-endpoint";
  HWND closed = OpenPennEditor(0, closedEndpoint, "");
  ShowWindow(closed, SW_HIDE);
  auto closedObject = FindWindowExW(closed, 0, L"Edit", 0),
       closedAttribute = FindWindowExW(closed, closedObject, L"Edit", 0);
  SetWindowTextW(closedObject, L"#123");
  SetWindowTextW(closedAttribute, L"CODE");
  std::string closedNonce;
  PennEditorSetTransport(
      [&](const std::string &target, const std::string &command) {
        assert(target == closedEndpoint);
        auto end = command.find(' ', 6);
        closedNonce = command.substr(6, end - 6);
        return true;
      });
  auto closedApprove = Approve(closed, L"Approve explicit read request");
  SendMessage(closed, WM_COMMAND, 115, 0);
  closedApprove.join();
  assert(!closedNonce.empty());
  DestroyWindow(closed);
  assert(PennEditorHandleOutput(
      closedEndpoint, closedNonce + " " + Base64("private after close")));
  assert(!PennEditorHandleOutput(
      "wrong-world", closedNonce + " " + Base64("private after close")));
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.source == stale.source);
  PennEditorInvalidateEndpoint(closedEndpoint);
  assert(!PennEditorHandleOutput(
      closedEndpoint, closedNonce + " " + Base64("private after close")));
  assert(sends == 2 && preparedSends == 0);
  wchar_t temporary[MAX_PATH], path[MAX_PATH];
  GetTempPathW(MAX_PATH, temporary);
  assert(GetTempFileNameW(temporary, L"ide", 0, path));
  assert(PennEditorSaveRecoveryFile(path));
  auto before = PennEditorListSnapshots();
  approve = Approve(editor, L"Review draft recovery");
  assert(PennEditorRestoreRecoveryFile(editor, path));
  approve.join();
  auto after = PennEditorListSnapshots();
  assert(after.size() == before.size() * 2);
  assert(after[2].source == before[0].source &&
         after[3].source == before[1].source);
  DeleteFileW(path);
  assert(sends == 2);
  SendMessage(text, SCI_SETTEXT, 0,
              reinterpret_cast<LPARAM>("say reviewed fixture"));
  PennEditorInfo reviewed;
  assert(PennEditorSnapshotInfo(editor, reviewed));
  PennEditorSetSendPrepared([&](const std::string &target, uint64_t id,
                                uint64_t rev,
                                const std::vector<std::string> &commands) {
    assert(target == "fixture-world/session-1");
    assert(id == reviewed.documentID && rev == reviewed.revision);
    assert(commands.size() == 1 && commands[0] == reviewed.source);
    ++preparedSends;
    return true;
  });
  approve = Approve(editor, L"Review and send prepared commands");
  SendMessage(editor, WM_COMMAND, 117, 0);
  approve.join();
  assert(preparedSends == 1);
  PennEditorSetSendPrepared({});
  auto normalCount = PennEditorListSnapshots().size();
  SendMessage(text, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("regeditall"));
  SendMessage(text, SCI_GOTOPOS, 10, 0);
  SendMessage(editor, WM_COMMAND, 106, 0);
  assert(SendMessage(text, SCI_AUTOCACTIVE, 0, 0));
  SendMessage(text, SCI_AUTOCCANCEL, 0, 0);
  SendMessage(editor, WM_COMMAND, 107, 0);
  HWND reference = FindWindowW(L"MUSHclientPennIDE",
                               L"PennMUSH reference: regeditall (read only)");
  assert(reference);
  auto referenceText = FindWindowExW(reference, 0, L"Scintilla", 0);
  assert(SendMessage(referenceText, SCI_GETREADONLY, 0, 0));
  PennEditorInfo referenceInfo;
  assert(!PennEditorSnapshotInfo(reference, referenceInfo));
  auto referenceLength = SendMessage(referenceText, SCI_GETLENGTH, 0, 0);
  SendMessage(referenceText, WM_CHAR, 'r', 1);
  assert(!SendMessage(referenceText, SCI_AUTOCACTIVE, 0, 0));
  for (int command : {101, 102, 109, 112, 115, 116, 117, 119})
    SendMessage(reference, WM_COMMAND, command, 0);
  assert(SendMessage(referenceText, SCI_GETLENGTH, 0, 0) == referenceLength);
  assert(PennEditorListSnapshots().size() == normalCount);
  assert(!PennEditorApplyProposal(reference, 1, 0, 0, "blocked"));
  DestroyWindow(reference);
  assert(sends == 2 && preparedSends == 1);
  SendMessage(editor, WM_COMMAND, 101, 0);
  PennEditorInfo fresh;
  assert(PennEditorSnapshotInfo(editor, fresh));
  std::thread rename([&] {
    for (int i = 0; i < 500; ++i) {
      auto dialog =
          FindWindowW(L"MUSHclientRenameDraft", L"Rename unsent draft");
      if (dialog && GetWindow(dialog, GW_OWNER) == editor) {
        SetWindowTextW(GetDlgItem(dialog, 10), L"Renamed local draft");
        PostMessage(dialog, WM_COMMAND, IDOK, 0);
        return;
      }
      Sleep(10);
    }
  });
  SendMessage(editor, WM_COMMAND, 119, 0);
  rename.join();
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.documentID == fresh.documentID &&
         info.revision == fresh.revision);
  auto tabs = FindWindowExW(editor, 0, WC_TABCONTROLW, 0);
  wchar_t title[80] = {};
  TCITEMW item = {};
  item.mask = TCIF_TEXT;
  item.pszText = title;
  item.cchTextMax = 80;
  SendMessageW(tabs, TCM_GETITEMW, TabCtrl_GetCurSel(tabs),
               reinterpret_cast<LPARAM>(&item));
  assert(std::wstring(title) == L"Renamed local draft");
  SendMessage(editor, WM_COMMAND, 118, 0);
  assert(PennEditorListSnapshots().size() == normalCount);
  for (auto &doc : PennEditorListSnapshots())
    assert(doc.documentID != fresh.documentID);
  auto previousFont = SendMessage(text, SCI_STYLEGETSIZE, STYLE_DEFAULT, 0);
  SendMessage(editor, WM_COMMAND, 120, 0);
  assert(SendMessage(text, SCI_STYLEGETSIZE, STYLE_DEFAULT, 0) ==
         std::min<sptr_t>(36, previousFont + 1));
  PennEditorInvalidateEndpoint("fixture-world/session-1");
  assert(!PennEditorHandleOutput("fixture-world/session-1", staleReply));
  assert(PennEditorSnapshotInfo(editor, info));
  assert(info.endpoint.empty());
  assert(info.revision > after.back().revision);
  assert(sends == 2);
  PennEditorSetTransport({});
  DestroyWindow(editor);
  wchar_t exe[32768] = {};
  GetModuleFileNameW(0, exe, 32768);
  std::wstring storage = exe;
  storage = storage.substr(0, storage.find_last_of(L"\\/") + 1) +
            L"editor-state\\preferences.ini";
  assert(GetFileAttributesW(storage.c_str()) != INVALID_FILE_ATTRIBUTES);
  assert(GetPrivateProfileIntW(L"Editor", L"FontSize", 0, storage.c_str()) ==
         std::min<sptr_t>(36, previousFont + 1));
  std::cout << "Native Scintilla editor smoke passed: UTF-8, styles, margin, "
               "undo, tabs, stale proposal rejection\n";
}
