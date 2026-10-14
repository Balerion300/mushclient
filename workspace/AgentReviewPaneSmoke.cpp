#define NOMINMAX
#include "../AgentWorkspaceCore.h"
#include "../third_party/scintilla/include/Scintilla.h"
#include "AgentReviewPane.h"
#include "PennEditorWindow.h"
#include <cassert>
#include <chrono>
#include <gdiplus.h>
#include <iostream>
#pragma comment(lib, "gdiplus.lib")
void CaptureFixture(HWND window, const wchar_t *path) {
  ShowWindow(window, SW_SHOWNOACTIVATE);
  RedrawWindow(window, 0, 0,
               RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_FRAME);
  Gdiplus::GdiplusStartupInput input;
  ULONG_PTR token = 0;
  assert(Gdiplus::GdiplusStartup(&token, &input, 0) == Gdiplus::Ok);
  RECT r;
  GetWindowRect(window, &r);
  HDC dc = GetWindowDC(window), memory = CreateCompatibleDC(dc);
  HBITMAP bitmap =
      CreateCompatibleBitmap(dc, r.right - r.left, r.bottom - r.top);
  auto old = SelectObject(memory, bitmap);
  assert(PrintWindow(window, memory, 0));
  SelectObject(memory, old);
  {
    Gdiplus::Bitmap image(bitmap, 0);
    const CLSID png = {0x557cf406,
                       0x1a04,
                       0x11d3,
                       {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
    assert(image.Save(path, &png, 0) == Gdiplus::Ok);
  }
  DeleteObject(bitmap);
  DeleteDC(memory);
  ReleaseDC(window, dc);
  Gdiplus::GdiplusShutdown(token);
  ShowWindow(window, SW_HIDE);
}
agent_workspace::Store StoreFixture;
agent_workspace::Store &WorkspaceAgentStore() { return StoreFixture; }
std::string WorkspaceEndpointForWindow(const std::string &window) {
  for (auto &w : StoreFixture.humanWindows())
    if (w.id == window)
      return window.find("/pane/") != std::string::npos
                 ? window.substr(0, window.find("/pane/"))
                 : window;
  return "";
}
HWND WorkspaceOpenDraft(HWND parent, const agent_workspace::Draft &draft) {
  auto endpoint = WorkspaceEndpointForWindow(draft.window);
  return endpoint.empty() ? 0 : OpenPennEditor(parent, endpoint, draft.text);
}
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
void ChooseMode(HWND pane, int mode) {
  SendDlgItemMessageW(pane, Mode, CB_SETCURSEL, mode, 0);
  SendMessageW(pane, WM_COMMAND, MAKEWPARAM(Mode, CBN_SELCHANGE),
               reinterpret_cast<LPARAM>(GetDlgItem(pane, Mode)));
}
void Select(HWND pane, int index = 0) {
  SendDlgItemMessageW(pane, List, LB_SETCURSEL, index, 0);
  SendMessageW(pane, WM_COMMAND, MAKEWPARAM(List, LBN_SELCHANGE),
               reinterpret_cast<LPARAM>(GetDlgItem(pane, List)));
}
void Click(HWND pane, int button) {
  SendMessageW(pane, WM_COMMAND, MAKEWPARAM(button, BN_CLICKED),
               reinterpret_cast<LPARAM>(GetDlgItem(pane, button)));
}
int main() {
  const std::string endpoint = "fixture-output";
  StoreFixture.addWindow(endpoint, "Fixture", "Shared transcript");
  auto paneEndpoint = endpoint + "/pane/staff";
  StoreFixture.addWindow(paneEndpoint, "Fixture", "Staff");
  auto session =
      StoreFixture.enable("Assistant", {endpoint, paneEndpoint}, true);
  HWND editor = OpenPennEditor(0, endpoint, "original source");
  assert(editor);
  ShowWindow(editor, SW_HIDE);
  HWND pane = OpenAgentReviewPane(0);
  assert(pane);
  ShowWindow(pane, SW_HIDE);
  assert(SendDlgItemMessageW(pane, List, LB_GETCOUNT, 0, 0) == 1);
  int sends = 0;
  PennEditorSetSendPrepared([&](const std::string &, uint64_t, uint64_t,
                                const std::vector<std::string> &) {
    ++sends;
    return true;
  });
  Select(pane);
  Click(pane, Primary);
  auto info = PennEditorListSnapshots().front();
  auto key = "native-editor-" + std::to_string(info.documentID);
  assert(StoreFixture.humanWorkspace("Assistant").editors.count(key));
  assert(StoreFixture.readEditor("Assistant", session, key).text ==
         "original source");
  auto proposal = StoreFixture.proposeEdit(
      "Assistant", session, key, info.revision, "changed by human acceptance",
      "Fixture explanation");
  ChooseMode(pane, 2);
  assert(SendDlgItemMessageW(pane, List, LB_GETCOUNT, 0, 0) == 1);
  Select(pane);
  assert(GetWindowTextLengthW(GetDlgItem(pane, Before)) == 15);
  CaptureFixture(pane, L"workspace/test-build/agent-review-fixture.png");
  Click(pane, Primary);
  assert(StoreFixture.proposals().at(proposal.id).status == "accepted");
  assert(PennEditorListSnapshots().front().source ==
         "changed by human acceptance");
  assert(sends == 0);
  info = PennEditorListSnapshots().front();
  proposal = StoreFixture.proposeEdit("Assistant", session, key, info.revision,
                                      "stale proposed source", "stale test");
  Click(pane, Refresh);
  Select(pane, 1);
  auto text = FindWindowExW(editor, 0, L"Scintilla", 0);
  SendMessageW(text, SCI_APPENDTEXT, 1, reinterpret_cast<LPARAM>("!"));
  Click(pane, Primary);
  assert(StoreFixture.proposals().at(proposal.id).status == "pending");
  assert(PennEditorListSnapshots().front().source.back() == '!');
  Click(pane, Secondary);
  assert(StoreFixture.proposals().at(proposal.id).status == "rejected");
  ChooseMode(pane, 1);
  Select(pane);
  Click(pane, Primary);
  assert(StoreFixture.captures("Assistant", session).size() == 1);
  StoreFixture.append(endpoint, "A selected completed-output fixture line.");
  Select(pane);
  SendDlgItemMessageW(pane, Before, EM_SETSEL, 0, 10);
  Click(pane, Snapshot);
  assert(StoreFixture.humanWorkspace("Assistant").contexts.size() == 1);
  auto context =
      StoreFixture.humanWorkspace("Assistant").contexts.begin()->second;
  assert(context.text == "A selected");
  ChooseMode(pane, 4);
  Select(pane);
  Click(pane, Primary);
  assert(GetWindowTextLengthW(GetDlgItem(pane, Before)) > 0);
  Click(pane, Secondary);
  assert(!StoreFixture.captures("Assistant", session).front().active);
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  auto task =
      StoreFixture.startTask("Assistant", session, "fixture task", now - 60000);
  ChooseMode(pane, 3);
  Select(pane);
  Click(pane, Primary);
  assert(StoreFixture.humanWorkspace("Assistant")
             .tasks.at(task.id)
             .cancellationRequested);
  StoreFixture.prepareDraft("Assistant", session, paneEndpoint, "fixture-draft",
                            "say unsent fixture");
  ChooseMode(pane, 5);
  Select(pane);
  Click(pane, Primary);
  auto draftEditors = PennEditorListSnapshots();
  assert(draftEditors.size() == 2 && draftEditors.back().endpoint == endpoint &&
         draftEditors.back().source == "say unsent fixture");
  assert(sends == 0);
  ChooseMode(pane, 0);
  Select(pane);
  Click(pane, Secondary);
  assert(!StoreFixture.humanWorkspace("Assistant").editors.count(key));
  SetWindowPos(pane, 0, 0, 0, 440, 420, SWP_NOMOVE | SWP_NOZORDER);
  RECT client;
  GetClientRect(pane, &client);
  for (int id : {Primary, Secondary, Snapshot, Refresh, Before, After}) {
    RECT control;
    GetWindowRect(GetDlgItem(pane, id), &control);
    MapWindowPoints(0, pane, reinterpret_cast<POINT *>(&control), 2);
    assert(control.left >= 0 && control.right <= client.right &&
           control.top >= 0 && control.bottom <= client.bottom);
  }
  assert(sends == 0);
  Click(pane, Primary);
  assert(StoreFixture.humanWorkspace("Assistant").editors.count(key));
  PennEditorInvalidateEndpoint(endpoint);
  Click(pane, Refresh);
  assert(!StoreFixture.humanWorkspace("Assistant").editors.count(key));
  DestroyWindow(pane);
  DestroyWindow(editor);
  PennEditorSetSendPrepared({});
  std::cout << "Native agent review smoke passed: explicit editor grants, full "
               "diff acceptance, stale refusal, rejection, output-only "
               "snapshots, capture start/read/stop, heartbeat cancellation, "
               "disconnect expiration, zero game sends\n";
}
