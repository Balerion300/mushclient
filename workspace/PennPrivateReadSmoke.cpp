// Exact production publication function, real editor nonce ownership and Store;
// only the original document/frame transport and rendering endpoints are spies.
#include "PennEditorWindow.cpp"
#include "../AgentWorkspaceCore.h"
#include <cassert>
#include <crtdbg.h>
#include <iostream>

struct CString : std::string {
  using std::string::string;
  CString(const std::string &s) : std::string(s) {}
  bool IsEmpty() const { return empty(); }
  operator const char *() const { return c_str(); }
};
struct CMUSHclientDoc {};
struct Identity { CString id; uint64_t sequence = 0; };
struct WorkspaceRun { std::wstring text; };
struct WorkspaceRecord {
  CString connectionId;
  uint64_t sequence = 0, receivedAtMillis = 0;
  std::vector<WorkspaceRun> runs;
};
std::map<CMUSHclientDoc *, Identity> identities;
agent_workspace::Store agentStore;
struct PublicationSpies {
  unsigned routed = 0, main = 0, cleared = 0;
  bool GetSafeHwnd() const { return true; }
  void ClearWorkspaceImageAssociation(const CString &) { ++cleared; }
  bool RouteWorkspaceRecord(const WorkspaceRecord &record) {
    ++routed;
    std::wstring text;
    for (const auto &run : record.runs) text += run.text;
    agentStore.append(std::string(record.connectionId) + "/pane/Sentinel", Narrow(text));
    return false;
  }
  void AppendWorkspaceRecord(const char *, const WorkspaceRecord &) { ++main; }
  static std::string Narrow(const std::wstring &s) {
    std::string result;
    for (auto ch : s) { assert(ch >= 0 && ch <= 127); result += static_cast<char>(ch); }
    return result; // Fictional protocol is ASCII.
  }
} Frame;
struct { bool m_pMainWnd = true; } App;
std::string utf8(const std::wstring &s) { return PublicationSpies::Narrow(s); }
#include "private_read_publication_body.inc"

namespace {
HWND Owner = 0;
bool Approved = false;
constexpr UINT ApproveRead = WM_APP + 739;
LRESULT CALLBACK ApproveProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR,
                             DWORD_PTR) {
  if (msg == ApproveRead) {
    assert(h == Owner && GetWindow(reinterpret_cast<HWND>(l), GW_OWNER) == Owner);
    PostMessageW(reinterpret_cast<HWND>(l), WM_COMMAND, IDOK, 0);
    return 0;
  }
  return DefSubclassProc(h, msg, w, l);
}
LRESULT CALLBACK ApproveHook(int code, WPARAM w, LPARAM l) {
  if (code == HCBT_ACTIVATE && GetWindow(reinterpret_cast<HWND>(w), GW_OWNER) == Owner) {
    wchar_t title[256] = {};
    GetWindowTextW(reinterpret_cast<HWND>(w), title, 256);
    assert(std::wstring(title) == L"Approve explicit read request");
    Approved = true;
    PostMessageW(Owner, ApproveRead, 0, w);
  }
  return CallNextHookEx(0, code, w, l);
}
std::string Base64(const std::string &s) {
  DWORD size = 0;
  assert(CryptBinaryToStringA(reinterpret_cast<const BYTE *>(s.data()), s.size(),
      CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, 0, &size));
  std::string result(size, '\0');
  assert(CryptBinaryToStringA(reinterpret_cast<const BYTE *>(s.data()), s.size(),
      CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &result[0], &size));
  result.resize(size);
  return result;
}
std::string Request(HWND window) {
  auto e = State(window);
  SetWindowTextW(e->object, L"#1");
  SetWindowTextW(e->attribute, L"TEST");
  Owner = window;
  Approved = false;
  assert(SetWindowSubclass(window, ApproveProc, 739, 0));
  auto hook = SetWindowsHookExW(WH_CBT, ApproveHook, 0, GetCurrentThreadId());
  assert(hook);
  Command(e, ReadAttribute);
  assert(Approved && !e->read.nonce.empty());
  UnhookWindowsHookEx(hook);
  RemoveWindowSubclass(window, ApproveProc, 739);
  Owner = 0;
  return e->read.nonce;
}
WorkspaceRecord Record(const std::string &endpoint, const std::string &text) {
  WorkspaceRecord record;
  record.connectionId = endpoint;
  record.runs.push_back({std::wstring(text.begin(), text.end())});
  return record;
}
}

int main(int argc, char **argv) {
  _set_error_mode(_OUT_TO_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
  std::cout << std::unitbuf;
  assert(argc == 2 && PennEditorConfigureStorage(Wide(argv[1])));
  CMUSHclientDoc a, b;
  const std::string endpoint = "fictional-private-A", other = "fictional-private-B";
  identities[&a].id = endpoint;
  identities[&b].id = other;
  std::vector<std::string> granted;
  for (const auto &id : {endpoint, other}) {
    agentStore.addWindow(id, id, "Main");
    agentStore.addWindow(id + "/pane/Sentinel", id, "Sentinel");
    granted.push_back(id);
    granted.push_back(id + "/pane/Sentinel");
  }
  const auto session = agentStore.enable("Fixture", granted, false);
  std::vector<std::string> captures;
  for (const auto &id : granted)
    captures.push_back(agentStore.startCapture("Fixture", session, id, "Future only"));
  auto rows = [&](const std::string &id) {
    return agentStore.read("Fixture", session, id).lines.size();
  };
  auto totalCaptureRows = [&] {
    size_t total = 0;
    for (const auto &id : captures)
      total += agentStore.readCapture("Fixture", session, id, 0).lines.size();
    return total;
  };
  unsigned requests = 0;
  PennEditorSetTransport([&](const std::string &id, const std::string &command) {
    assert(id == endpoint && command.find(" [encode64(get(#1/TEST))]") != std::string::npos);
    ++requests;
    return true;
  });
  const std::string source = "&TEST #1=FICTIONAL_PRIVATE_SOURCE";
  auto window = OpenPennEditor(0, endpoint, "initial", "Fictional private A");
  assert(window);
  ShowWindow(window, SW_HIDE);
  auto nonce = Request(window);
  auto reply = Record(endpoint, nonce + " " + Base64(source));
  // Trigger omission does not make the correlated response public or lose it.
  assert(WorkspacePublishRecord(&a, reply, true));
  assert(Text(State(window)) == source);
  assert(Frame.routed == 0 && Frame.main == 0 && Frame.cleared == 1);
  assert(rows(endpoint) == 0 && rows(endpoint + "/pane/Sentinel") == 0 && totalCaptureRows() == 0);
  // Duplicate and malformed owned payloads remain private after consumption.
  assert(WorkspacePublishRecord(&a, reply, false));
  assert(WorkspacePublishRecord(&a, Record(endpoint, nonce + " !!!invalid-base64"), false));
  assert(Frame.routed == 0 && Frame.main == 0 && totalCaptureRows() == 0);
  DestroyWindow(window);
  assert(WorkspacePublishRecord(&a, reply, false));
  assert(Frame.routed == 0 && Frame.main == 0 && totalCaptureRows() == 0);
  std::cout << "PASS owned, omitted, duplicate, malformed and closed-editor replies bypass shared rendering, routes, agent output and active captures\n";

  // The same bytes on a different runtime connection are ordinary output.
  auto elsewhere = reply;
  elsewhere.connectionId = other;
  assert(!WorkspacePublishRecord(&b, elsewhere, false));
  assert(rows(other) == 1 && rows(other + "/pane/Sentinel") == 1 && totalCaptureRows() == 2);
  assert(!WorkspacePublishRecord(&a, Record(endpoint, "ffffffffffffffffffffffffffffffff ordinary wrong nonce"), false));
  assert(Frame.routed == 2 && Frame.main == 2 && totalCaptureRows() == 4);
  std::cout << "PASS nonce ownership is runtime-connection scoped; wrong nonce stays ordinary routed/captured output\n";

  window = OpenPennEditor(0, endpoint, "unchanged", "Fictional private A");
  ShowWindow(window, SW_HIDE);
  nonce = Request(window);
  SetWindowTextW(State(window)->object, L"#2");
  SetWindowTextW(State(window)->object, L"#1");
  const auto before = totalCaptureRows();
  assert(WorkspacePublishRecord(&a, Record(endpoint, nonce + " " + Base64(source)), false));
  assert(Text(State(window)) == "unchanged" && totalCaptureRows() == before && Frame.routed == 2);
  DestroyWindow(window);
  assert(requests == 2 && Windows.empty());
  std::cout << "PASS target edit/revert stales the read but keeps its late response private\n";
  std::cout << "All native private publication checks passed; no client or network was used.\n";
}
