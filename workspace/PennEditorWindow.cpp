#define NOMINMAX
#include "PennEditorWindow.h"
#include "../third_party/scintilla/include/Scintilla.h"
#include "PennLanguage.h"
#include <bcrypt.h>
#include <cctype>
#include <commctrl.h>
#include <commdlg.h>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <sstream>
#include <vector>
#include <wincrypt.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
namespace {
std::vector<HWND> Windows;
std::function<bool(const std::string &, const std::string &)> Transport;
// Ownership survives editor close. A late private protocol reply must never
// become shared game output merely because its original editor disappeared.
std::map<std::string, std::set<std::string>> ReplyOwnership;
size_t OwnedNonceCount = 0;
bool KnownReply(const std::string &endpoint, const std::string &nonce) {
  auto found = ReplyOwnership.find(endpoint);
  return found != ReplyOwnership.end() && found->second.count(nonce);
}
bool OwnReply(const std::string &endpoint, const std::string &nonce) {
  if (KnownReply(endpoint, nonce))
    return false;
  if (OwnedNonceCount >= 8192)
    return false;
  ReplyOwnership[endpoint].insert(nonce);
  ++OwnedNonceCount;
  return true;
}
std::function<bool(const std::string &, uint64_t, uint64_t,
                   const std::vector<std::string> &)>
    SendPrepared;
PennIDE::HelpCatalog Catalog;
std::string FullHelp;
std::wstring StorageFolder;
std::wstring ExeFolder() {
  wchar_t path[32768] = {};
  GetModuleFileNameW(0, path, 32768);
  std::wstring p = path;
  return p.substr(0, p.find_last_of(L"\\/") + 1);
}
std::wstring Storage() {
  if (StorageFolder.empty())
    StorageFolder = ExeFolder() + L"editor-state";
  CreateDirectoryW(StorageFolder.c_str(), 0);
  return StorageFolder;
}
void LoadCatalog() {
  static bool loaded = false;
  if (loaded)
    return;
  loaded = true;
  for (auto file : {L"pennfunc.hlp", L"penncmd.hlp"}) {
    std::ifstream input(ExeFolder() + L"pennmush-reference\\" + file,
                        std::ios::binary | std::ios::ate);
    auto size = input.tellg();
    if (size < 0 || size > 1024 * 1024)
      continue;
    input.seekg(0);
    std::string text(static_cast<size_t>(size), '\0');
    if (size)
      input.read(&text[0], size);
    if (PennIDE::ParseHelp(text, Catalog)) {
      FullHelp += text;
      FullHelp += '\n';
    }
  }
}
constexpr size_t MaxSource = 4 * 1024 * 1024;
enum {
  New = 101,
  Open,
  Save,
  Undo,
  Redo,
  Complete,
  Reference,
  Theme,
  Review,
  Find,
  Replace,
  Recover,
  FullReference,
  Language,
  ReadAttribute,
  Discover,
  SendCommands,
  CloseDraft,
  RenameDraft,
  LargerFont,
  SmallerFont
};
struct Document {
  PennIDE::Draft draft;
  sptr_t scintilla = 0;
  std::wstring path;
  std::wstring title;
  bool lua = false;
  uint64_t id = 0;
};
struct Editor {
  HWND window = 0, text = 0, tabs = 0, status = 0, object = 0, attribute = 0,
       search = 0, replacement = 0;
  std::vector<Document> docs;
  size_t active = 0;
  bool loading = false, dark = true;
  bool recoverable = true;
  int fontSize = 11;
  sptr_t lastCaret = -1;
  std::string endpoint;
  PennIDE::ReadRequest read, discovery;
  size_t readDocument = 0;
  std::vector<std::string> discovered;
};
Editor *State(HWND h) {
  return reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
}
sptr_t Sci(Editor *e, UINT msg, uptr_t w = 0, sptr_t l = 0) {
  return SendMessage(e->text, msg, w, l);
}
std::wstring Wide(const std::string &s) {
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(),
                              static_cast<int>(s.size()), 0, 0);
  std::wstring w(n, L' ');
  if (n)
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0],
                        n);
  return w;
}
std::string Utf8(HWND h) {
  int n = GetWindowTextLengthW(h);
  std::wstring w(n + 1, L' ');
  GetWindowTextW(h, &w[0], n + 1);
  int z = WideCharToMultiByte(CP_UTF8, 0, w.data(), n, 0, 0, 0, 0);
  std::string s(z, ' ');
  if (z)
    WideCharToMultiByte(CP_UTF8, 0, w.data(), n, &s[0], z, 0, 0);
  return s;
}
std::string Text(Editor *e) {
  size_t n = static_cast<size_t>(Sci(e, SCI_GETLENGTH));
  if (n > MaxSource)
    return "";
  std::string s(n + 1, '\0');
  Sci(e, SCI_GETTEXT, n + 1, reinterpret_cast<sptr_t>(&s[0]));
  s.resize(n);
  return s;
}
void Sync(Editor *e) {
  if (e->docs.empty() || static_cast<size_t>(Sci(e, SCI_GETLENGTH)) > MaxSource)
    return;
  auto &d = e->docs[e->active].draft;
  auto s = Text(e);
  if (s != d.source)
    d.Replace(s);
}
void Colors(Editor *e) {
  COLORREF back = e->dark ? RGB(28, 31, 36) : RGB(255, 255, 255),
           fore = e->dark ? RGB(225, 228, 235) : RGB(30, 30, 35);
  Sci(e, SCI_STYLESETFORE, STYLE_DEFAULT, fore);
  Sci(e, SCI_STYLESETBACK, STYLE_DEFAULT, back);
  Sci(e, SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<sptr_t>("Consolas"));
  Sci(e, SCI_STYLESETSIZE, STYLE_DEFAULT, e->fontSize);
  Sci(e, SCI_STYLECLEARALL);
  Sci(e, SCI_SETCARETFORE, fore);
  Sci(e, SCI_STYLESETFORE, PennIDE::Function, RGB(80, 170, 230));
  Sci(e, SCI_STYLESETFORE, PennIDE::Command, RGB(210, 130, 230));
  Sci(e, SCI_STYLESETFORE, PennIDE::Substitution, RGB(220, 155, 70));
  Sci(e, SCI_STYLESETFORE, PennIDE::Bracket, RGB(70, 180, 130));
  Sci(e, SCI_STYLESETFORE, PennIDE::Escape, RGB(220, 100, 100));
  Sci(e, SCI_STYLESETBACK, STYLE_BRACELIGHT, RGB(80, 95, 110));
}
void Analyze(Editor *e) {
  Sync(e);
  auto s = Text(e);
  if (!e->recoverable) {
    SetWindowTextW(e->status,
                   L"Read-only pinned PennMUSH reference. Source sharing, "
                   L"proposals and game commands are disabled.");
    return;
  }
  auto a =
      e->docs[e->active].lua ? PennIDE::AnalyzeLua(s) : PennIDE::Analyze(s);
  Sci(e, SCI_STARTSTYLING, 0);
  if (!a.styles.empty())
    Sci(e, SCI_SETSTYLINGEX, a.styles.size(),
        reinterpret_cast<sptr_t>(a.styles.data()));
  Sci(e, SCI_SETINDICATORCURRENT, 0);
  Sci(e, SCI_INDICATORCLEARRANGE, 0, s.size());
  for (auto &d : a.diagnostics)
    Sci(e, SCI_INDICATORFILLRANGE, d.byteOffset, 1);
  std::string status = "Unsent draft | " + e->endpoint + " | revision " +
                       std::to_string(e->docs[e->active].draft.revision) +
                       " | " + std::to_string(a.diagnostics.size()) +
                       " advisory bracket hints";
  if (a.limited)
    status += " | analysis limited to first 256 KiB";
  auto hint = e->docs[e->active].lua
                  ? std::string("Lua/plugin source mode")
                  : PennIDE::ArgumentHint(s, Sci(e, SCI_GETCURRENTPOS));
  if (!e->docs[e->active].lua) {
    auto name = PennIDE::ActiveFunctionName(s, Sci(e, SCI_GETCURRENTPOS));
    auto found = Catalog.find(name);
    if (found != Catalog.end())
      hint = found->second.signature.substr(0, 512);
  }
  if (!hint.empty())
    status += " | " + hint;
  SetWindowTextW(e->status, Wide(status).c_str());
}
void Select(Editor *e, size_t i) {
  if (i >= e->docs.size())
    return;
  Sync(e);
  e->loading = true;
  e->active = i;
  Sci(e, SCI_SETDOCPOINTER, 0, e->docs[i].scintilla);
  e->loading = false;
  TabCtrl_SetCurSel(e->tabs, static_cast<int>(i));
  SetWindowTextW(e->object, Wide(e->docs[i].draft.object).c_str());
  SetWindowTextW(e->attribute, Wide(e->docs[i].draft.attribute).c_str());
  Analyze(e);
  SetFocus(e->text);
}
void Add(Editor *e, const std::string &s) {
  Sync(e);
  Document d;
  d.draft.endpoint = e->endpoint;
  d.draft.source = s;
  static volatile LONG nextIdentity = 0;
  d.id = static_cast<uint64_t>(InterlockedIncrement(&nextIdentity));
  d.draft.revision = d.id << 32;
  d.scintilla = Sci(e, SCI_CREATEDOCUMENT, 0, 0);
  e->docs.push_back(d);
  TCITEMW item = {};
  item.mask = TCIF_TEXT;
  std::wstring name = L"Draft " + std::to_wstring(e->docs.size());
  e->docs.back().title = name;
  item.pszText = &name[0];
  SendMessageW(e->tabs, TCM_INSERTITEMW, e->docs.size() - 1,
               reinterpret_cast<LPARAM>(&item));
  e->loading = true;
  e->active = e->docs.size() - 1;
  Sci(e, SCI_SETDOCPOINTER, 0, d.scintilla);
  Sci(e, SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(s.c_str()));
  Sci(e, SCI_EMPTYUNDOBUFFER);
  e->loading = false;
  TabCtrl_SetCurSel(e->tabs, static_cast<int>(e->active));
  SetWindowTextW(e->object, L"");
  SetWindowTextW(e->attribute, L"");
  Analyze(e);
  if (!e->recoverable)
    Sci(e, SCI_SETREADONLY, 1);
}
std::wstring File(Editor *e, bool save) {
  wchar_t path[32768] = {};
  OPENFILENAMEW f = {};
  f.lStructSize = sizeof(f);
  f.hwndOwner = e->window;
  f.lpstrFilter = L"UTF-8 source\0*.txt;*.mush;*.lua\0All files\0*.*\0";
  f.lpstrFile = path;
  f.nMaxFile = 32768;
  f.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR |
            (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  return (save ? GetSaveFileNameW(&f) : GetOpenFileNameW(&f)) ? path : L"";
}
std::wstring Recovery() { return Storage() + L"\\drafts.recovery"; }
void Preferences(Editor *e, bool save) {
  auto path = Storage() + L"\\preferences.ini";
  if (save) {
    RECT r;
    GetWindowRect(e->window, &r);
    WritePrivateProfileStringW(L"Editor", L"Dark", e->dark ? L"1" : L"0",
                               path.c_str());
    WritePrivateProfileStringW(L"Editor", L"FontSize",
                               std::to_wstring(e->fontSize).c_str(),
                               path.c_str());
    WritePrivateProfileStringW(L"Editor", L"Width",
                               std::to_wstring(r.right - r.left).c_str(),
                               path.c_str());
    WritePrivateProfileStringW(L"Editor", L"Height",
                               std::to_wstring(r.bottom - r.top).c_str(),
                               path.c_str());
  } else {
    e->dark = GetPrivateProfileIntW(L"Editor", L"Dark", 1, path.c_str()) != 0;
    e->fontSize = std::max(
        8, std::min(36, static_cast<int>(GetPrivateProfileIntW(
                            L"Editor", L"FontSize", 11, path.c_str()))));
  }
}
bool Write(const std::wstring &path, const std::string &s) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(s.data(), s.size());
  return out.good();
}
bool SaveRecovery(const std::wstring &path = Recovery()) {
  std::vector<std::string> sources;
  for (auto h : Windows) {
    auto e = State(h);
    if (!e || !e->recoverable)
      continue;
    Sync(e);
    for (auto &d : e->docs)
      sources.push_back(d.draft.source);
  }
  std::string bytes;
  if (!PennIDE::EncodeRecovery(sources, bytes))
    return false;
  auto temporary = path + L".tmp";
  return Write(temporary, bytes) &&
         MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
bool RestoreRecovery(Editor *e, const std::wstring &path = Recovery()) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  auto size = input.tellg();
  if (size < 0 || size > 8 * 1024 * 1024) {
    MessageBoxW(e->window, L"Recovery is missing or exceeds the 8 MiB limit.",
                L"Draft recovery", MB_ICONWARNING);
    return false;
  }
  input.seekg(0);
  std::string bytes(static_cast<size_t>(size), '\0');
  if (size)
    input.read(&bytes[0], size);
  std::vector<std::string> sources;
  if (!PennIDE::DecodeRecovery(bytes, sources)) {
    MessageBoxW(e->window,
                L"Recovery is malformed. The current drafts remain unchanged.",
                L"Draft recovery", MB_ICONWARNING);
    return false;
  }
  if (MessageBoxW(
          e->window,
          L"Restore source-only drafts as new unsent tabs in this window? "
          L"Destination bindings and sharing grants are never recovered. Check "
          L"the current destination before preparing a command.",
          L"Review draft recovery", MB_OKCANCEL) != IDOK)
    return false;
  for (auto &source : sources)
    Add(e, source);
  return true;
}
void Load(Editor *e, const std::wstring &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  auto size = in.tellg();
  if (size < 0 || size > static_cast<std::streamoff>(MaxSource)) {
    MessageBoxW(e->window,
                L"Cannot open source: missing file or larger than 4 MiB.",
                L"PennMUSH IDE", MB_ICONERROR);
    return;
  }
  in.seekg(0);
  std::string s(static_cast<size_t>(size), '\0');
  if (!s.empty())
    in.read(&s[0], s.size());
  if (s.compare(0, 3, "\xEF\xBB\xBF") == 0)
    s.erase(0, 3);
  if (!PennIDE::ValidUtf8(s)) {
    MessageBoxW(e->window, L"Source must be UTF-8 without embedded NUL.",
                L"PennMUSH IDE", MB_ICONERROR);
    return;
  }
  Add(e, s);
  e->docs.back().path = path;
}
struct RenameState {
  HWND window = 0, edit = 0;
  bool done = false, accepted = false;
  std::wstring value;
};
LRESULT CALLBACK RenameProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
  auto state =
      reinterpret_cast<RenameState *>(GetWindowLongPtrW(h, GWLP_USERDATA));
  if (msg == WM_NCCREATE) {
    state = static_cast<RenameState *>(
        reinterpret_cast<CREATESTRUCT *>(l)->lpCreateParams);
    state->window = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  }
  if (!state)
    return DefWindowProcW(h, msg, w, l);
  if (msg == WM_CREATE) {
    state->edit =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->value.c_str(),
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 12,
                        14, 350, 28, h, reinterpret_cast<HMENU>(10), 0, 0);
    SendMessageW(state->edit, EM_SETLIMITTEXT, 64, 0);
    CreateWindowW(L"BUTTON", L"Rename",
                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 160,
                  55, 96, 28, h, reinterpret_cast<HMENU>(IDOK), 0, 0);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 266,
                  55, 96, 28, h, reinterpret_cast<HMENU>(IDCANCEL), 0, 0);
    SetFocus(state->edit);
    SendMessageW(state->edit, EM_SETSEL, 0, -1);
    return 0;
  }
  if (msg == WM_CLOSE ||
      (msg == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL))) {
    if (msg == WM_COMMAND && LOWORD(w) == IDOK) {
      int n = GetWindowTextLengthW(state->edit);
      state->value.assign(n + 1, L' ');
      GetWindowTextW(state->edit, &state->value[0], n + 1);
      state->value.resize(n);
      state->accepted = !state->value.empty();
    }
    state->done = true;
    DestroyWindow(h);
    return 0;
  }
  if (msg == WM_NCDESTROY)
    state->done = true;
  return DefWindowProcW(h, msg, w, l);
}
bool Rename(Editor *e) {
  HWND owner = e->window;
  uint64_t documentID = e->docs[e->active].id;
  RenameState state;
  state.value = e->docs[e->active].title;
  WNDCLASSW cls = {};
  cls.lpfnWndProc = RenameProc;
  cls.hInstance = GetModuleHandleW(0);
  cls.lpszClassName = L"MUSHclientRenameDraft";
  cls.hCursor = LoadCursor(0, IDC_ARROW);
  cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  RegisterClassW(&cls);
  RECT r;
  GetWindowRect(e->window, &r);
  auto dialog = CreateWindowExW(
      WS_EX_DLGMODALFRAME, cls.lpszClassName, L"Rename unsent draft",
      WS_POPUP | WS_CAPTION | WS_SYSMENU, r.left + 40, r.top + 60, 390, 130,
      e->window, 0, cls.hInstance, &state);
  if (!dialog)
    return false;
  EnableWindow(e->window, FALSE);
  ShowWindow(dialog, SW_SHOW);
  MSG msg = {};
  while (!state.done && GetMessageW(&msg, 0, 0, 0) > 0) {
    if (!IsDialogMessageW(dialog, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  if (!state.done) {
    if (IsWindow(dialog))
      DestroyWindow(dialog);
    if (msg.message == WM_QUIT)
      PostQuitMessage(static_cast<int>(msg.wParam));
  }
  if (!IsWindow(owner) || State(owner) != e)
    return false;
  EnableWindow(owner, TRUE);
  if (state.accepted) {
    size_t selected = 0;
    while (selected < e->docs.size() && e->docs[selected].id != documentID)
      ++selected;
    if (selected == e->docs.size())
      return false;
    e->docs[selected].title = state.value;
    TCITEMW item = {};
    item.mask = TCIF_TEXT;
    item.pszText = &state.value[0];
    SendMessageW(e->tabs, TCM_SETITEMW, selected,
                 reinterpret_cast<LPARAM>(&item));
  }
  return state.accepted;
}
struct CommandReview {
  std::wstring title, text;
};
INT_PTR CALLBACK ReviewProc(HWND dialog, UINT msg, WPARAM w, LPARAM l) {
  if (msg == WM_INITDIALOG) {
    auto review = reinterpret_cast<CommandReview *>(l);
    SetWindowTextW(dialog, review->title.c_str());
    auto text = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY |
            ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
        0, 0, 1, 1, dialog, reinterpret_cast<HMENU>(10), 0, 0);
    SendMessageW(text, EM_SETLIMITTEXT, 256 * 1024, 0);
    SetWindowTextW(text, review->text.c_str());
    for (auto id : {IDOK, IDCANCEL}) {
      auto button =
          CreateWindowW(L"BUTTON", id == IDOK ? L"Send once" : L"Cancel",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0,
                        0, 1, 1, dialog, reinterpret_cast<HMENU>(id), 0, 0);
      SendMessageW(button, WM_SETFONT,
                   reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                   TRUE);
    }
    SendMessageW(text, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
    RECT r;
    GetClientRect(dialog, &r);
    SendMessageW(dialog, WM_SIZE, 0, MAKELPARAM(r.right, r.bottom));
    return TRUE;
  }
  if (msg == WM_SIZE) {
    int width = LOWORD(l), height = HIWORD(l);
    MoveWindow(GetDlgItem(dialog, 10), 8, 8, std::max(30, width - 16),
               std::max(30, height - 54), TRUE);
    MoveWindow(GetDlgItem(dialog, IDOK), std::max(8, width - 208), height - 38,
               96, 28, TRUE);
    MoveWindow(GetDlgItem(dialog, IDCANCEL), std::max(110, width - 104),
               height - 38, 96, 28, TRUE);
    return TRUE;
  }
  if (msg == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL)) {
    EndDialog(dialog, LOWORD(w));
    return TRUE;
  }
  if (msg == WM_CLOSE) {
    EndDialog(dialog, IDCANCEL);
    return TRUE;
  }
  return FALSE;
}
bool ReviewCommands(HWND owner, const std::string &preview) {
  CommandReview state;
  state.title = L"Review and send prepared commands";
  state.text = Wide(preview);
  std::vector<WORD> templateBytes((sizeof(DLGTEMPLATE) + 1) / 2 + 3, 0);
  auto dialog = reinterpret_cast<DLGTEMPLATE *>(templateBytes.data());
  dialog->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                  DS_MODALFRAME | DS_CENTER;
  dialog->cx = 480;
  dialog->cy = 300;
  return DialogBoxIndirectParamW(GetModuleHandleW(0), dialog, owner, ReviewProc,
                                 reinterpret_cast<LPARAM>(&state)) == IDOK;
}
void Command(Editor *e, int id) {
  if (!e->recoverable && id != Save && id != Find && id != Theme &&
      id != LargerFont && id != SmallerFont && id != Reference &&
      id != FullReference && id != CloseDraft)
    return;
  if (static_cast<size_t>(Sci(e, SCI_GETLENGTH)) > MaxSource) {
    SetTimer(e->window, 1, 1, 0);
    return;
  }
  switch (id) {
  case RenameDraft:
    Rename(e);
    break;
  case CloseDraft: {
    if (!e->recoverable) {
      DestroyWindow(e->window);
      break;
    }
    Sync(e);
    size_t closing = e->active;
    if (!e->docs[closing].draft.source.empty()) {
      int answer = MessageBoxW(
          e->window, L"Save this draft to a source file before closing it?",
          L"Close unsent draft", MB_YESNOCANCEL);
      if (answer == IDCANCEL)
        break;
      if (answer == IDYES) {
        auto path = File(e, true);
        if (path.empty() || !Write(path, e->docs[closing].draft.source))
          break;
      }
    }
    if (e->docs.size() == 1)
      Add(e, "");
    size_t next = closing ? closing - 1 : 1;
    auto pointer = e->docs[closing].scintilla;
    Sci(e, SCI_SETDOCPOINTER, 0, e->docs[next].scintilla);
    TabCtrl_DeleteItem(e->tabs, closing);
    e->docs.erase(e->docs.begin() + closing);
    e->active = next > closing ? next - 1 : next;
    if (!e->read.nonce.empty()) {
      if (e->readDocument == closing)
        e->read = PennIDE::ReadRequest();
      else if (e->readDocument > closing)
        --e->readDocument;
    }
    Sci(e, SCI_RELEASEDOCUMENT, 0, pointer);
    Select(e, e->active);
    break;
  }
  case LargerFont:
  case SmallerFont:
    e->fontSize =
        std::max(8, std::min(36, e->fontSize + (id == LargerFont ? 1 : -1)));
    Colors(e);
    Analyze(e);
    break;
  case SendCommands: {
    if (!SendPrepared || e->endpoint.empty()) {
      MessageBoxW(e->window,
                  L"The original connected destination is unavailable.",
                  L"Prepared commands", MB_ICONINFORMATION);
      break;
    }
    Sync(e);
    auto &document = e->docs[e->active];
    auto &draft = document.draft;
    auto object = Utf8(e->object), attribute = Utf8(e->attribute);
    if (draft.object != object || draft.attribute != attribute) {
      draft.object = object;
      draft.attribute = attribute;
      ++draft.revision;
    }
    std::vector<std::string> commands;
    std::string error;
    if (!PennIDE::PrepareCommands(draft, commands, error)) {
      MessageBoxW(e->window, Wide(error).c_str(), L"Prepared commands",
                  MB_ICONWARNING);
      break;
    }
    auto endpoint = draft.endpoint;
    auto documentID = document.id, revision = draft.revision;
    std::string preview = "Destination: " + endpoint +
                          "\nDocument: " + std::to_string(documentID) +
                          " revision " + std::to_string(revision) +
                          "\n\nExact commands:\n";
    for (auto &command : commands)
      preview += command + "\n";
    preview += "\nSend these commands once to this original destination?";
    if (!ReviewCommands(e->window, preview))
      break;
    Sync(e);
    if (e->endpoint != endpoint || e->docs[e->active].id != documentID ||
        draft.revision != revision || draft.endpoint != endpoint) {
      MessageBoxW(e->window, L"Draft or destination changed. Nothing was sent.",
                  L"Prepared commands", MB_ICONWARNING);
      break;
    }
    if (!SendPrepared(endpoint, documentID, revision, commands))
      MessageBoxW(e->window,
                  L"The host did not confirm dispatch. Check the world "
                  L"transcript before retrying.",
                  L"Prepared commands", MB_ICONWARNING);
    break;
  }
  case ReadAttribute:
  case Discover: {
    if (OwnedNonceCount >= 8192) {
      MessageBoxW(e->window,
                  L"The bounded private-read request registry is full. Further "
                  L"reads require an original endpoint disconnect to clear its "
                  L"ownership records.",
                  L"Read request", MB_ICONINFORMATION);
      break;
    }
    if (!Transport || e->endpoint.empty()) {
      MessageBoxW(
          e->window,
          L"An original connected destination and read transport are required.",
          L"Read request", MB_ICONINFORMATION);
      break;
    }
    unsigned char bytes[16];
    if (BCryptGenRandom(0, bytes, sizeof(bytes),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
      break;
    std::string nonce;
    const char *hex = "0123456789abcdef";
    for (auto b : bytes) {
      nonce += hex[b >> 4];
      nonce += hex[b & 15];
    }
    if (KnownReply(e->endpoint, nonce))
      break;
    Sync(e);
    auto &draft = e->docs[e->active].draft;
    auto object = Utf8(e->object), attribute = Utf8(e->attribute);
    if (draft.object != object || draft.attribute != attribute) {
      draft.object = object;
      draft.attribute = attribute;
      ++draft.revision;
    }
    PennIDE::ReadRequest request;
    bool valid = id == ReadAttribute
                     ? PennIDE::PrepareAttributeRead(draft, nonce, request)
                     : PennIDE::PrepareDiscovery(e->endpoint, nonce, request);
    if (!valid) {
      MessageBoxW(
          e->window,
          L"Use a numeric object reference and a simple attribute name.",
          L"Read request", MB_ICONWARNING);
      break;
    }
    std::string preview =
        "Read-only request to " + request.endpoint + "\n\n" + request.command +
        "\n\nSend this exact read request? Loading an attribute replaces the "
        "draft only if its source revision is unchanged.";
    if (MessageBoxW(e->window, Wide(preview).c_str(),
                    L"Approve explicit read request", MB_OKCANCEL) != IDOK)
      break;
    Sync(e);
    if (request.endpoint != e->endpoint ||
        (!request.discovery && request.revision != draft.revision))
      break;
    if (!OwnReply(request.endpoint, request.nonce))
      break;
    if (request.discovery)
      e->discovery = request;
    else {
      e->read = request;
      e->readDocument = e->active;
    }
    if (!Transport(request.endpoint, request.command)) {
      if (request.discovery)
        e->discovery = PennIDE::ReadRequest();
      else
        e->read = PennIDE::ReadRequest();
      MessageBoxW(e->window,
                  L"Original destination is unavailable. Nothing was loaded.",
                  L"Read request", MB_ICONWARNING);
    }
    break;
  }
  case New:
    Add(e, "");
    break;
  case Open: {
    auto p = File(e, false);
    if (!p.empty())
      Load(e, p);
    break;
  }
  case Save: {
    Sync(e);
    auto p = File(e, true);
    if (!p.empty() && !Write(p, Text(e)))
      MessageBoxW(e->window, L"Save failed.", L"PennMUSH IDE", MB_ICONERROR);
    break;
  }
  case Undo:
    Sci(e, SCI_UNDO);
    break;
  case Redo:
    Sci(e, SCI_REDO);
    break;
  case Theme:
    e->dark = !e->dark;
    Colors(e);
    Analyze(e);
    break;
  case Language:
    e->docs[e->active].lua = !e->docs[e->active].lua;
    Analyze(e);
    break;
  case Complete: {
    if (e->docs[e->active].lua) {
      const char *keywords =
          "and break do else elseif end false for function if in local nil not "
          "or repeat return then true until while";
      auto source = Text(e);
      size_t end = Sci(e, SCI_GETCURRENTPOS), begin = end;
      while (begin &&
             std::isalpha(static_cast<unsigned char>(source[begin - 1])))
        --begin;
      auto prefix = source.substr(begin, end - begin);
      std::istringstream words(keywords);
      std::string name, list;
      while (words >> name)
        if (name.find(prefix) == 0) {
          if (!list.empty())
            list += ' ';
          list += name;
        }
      if (!list.empty())
        Sci(e, SCI_AUTOCSHOW, end - begin,
            reinterpret_cast<sptr_t>(list.c_str()));
      break;
    }
    auto s = Text(e);
    size_t end = Sci(e, SCI_GETCURRENTPOS), begin = end;
    while (begin && ((s[begin - 1] >= 'a' && s[begin - 1] <= 'z') ||
                     (s[begin - 1] >= 'A' && s[begin - 1] <= 'Z') ||
                     (s[begin - 1] >= '0' && s[begin - 1] <= '9') ||
                     s[begin - 1] == '_' || s[begin - 1] == '@'))
      --begin;
    auto list =
        Catalog.empty()
            ? PennIDE::Completion(s.substr(begin, end - begin))
            : PennIDE::CatalogCompletion(Catalog, s.substr(begin, end - begin));
    auto prefix = s.substr(begin, end - begin);
    for (const auto &name : e->discovered)
      if (name.find(prefix) == 0) {
        if (!list.empty())
          list += ' ';
        list += name;
      }
    if (!list.empty())
      Sci(e, SCI_AUTOCSHOW, end - begin,
          reinterpret_cast<sptr_t>(list.c_str()));
    break;
  }
  case Reference: {
    auto source = Text(e);
    size_t caret = Sci(e, SCI_GETCURRENTPOS), begin = caret, end = caret;
    auto nameChar = [](char c) {
      return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') || c == '_' || c == '@' || c == '/';
    };
    while (begin && nameChar(source[begin - 1]))
      --begin;
    while (end < source.size() && nameChar(source[end]))
      ++end;
    auto name = source.substr(begin, end - begin);
    for (auto &c : name)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto found = Catalog.find(name);
    if (found == Catalog.end())
      found = Catalog.find(PennIDE::ActiveFunctionName(source, caret));
    if (found != Catalog.end()) {
      auto reference = OpenPennEditor(
          e->window, "", found->second.name + "\n\n" + found->second.text);
      if (reference)
        SetWindowTextW(reference, Wide("PennMUSH reference: " +
                                       found->second.name + " (read only)")
                                      .c_str());
    } else {
      std::string refs =
          "No local topic at the caret. Unknown game functions remain "
          "legal.\nUse Full offline reference or explicit game discovery.\n\n";
      for (auto &r : PennIDE::References())
        refs += std::string(r.signature) + "\n" + r.description + "\n\n";
      MessageBoxW(e->window, Wide(refs).c_str(), L"PennMUSH reference", MB_OK);
    }
    break;
  }
  case Review: {
    Sync(e);
    auto &d = e->docs[e->active].draft;
    auto object = Utf8(e->object), attribute = Utf8(e->attribute);
    if (d.object != object || d.attribute != attribute) {
      d.object = object;
      d.attribute = attribute;
      ++d.revision;
    }
    PennIDE::Review r;
    std::string error;
    if (!PennIDE::PrepareReview(d, r, error)) {
      MessageBoxW(e->window, Wide(error).c_str(), L"Attribute review",
                  MB_ICONWARNING);
      break;
    }
    std::string preview = "Destination: " + r.endpoint +
                          "\nRevision: " + std::to_string(r.revision) + "\n\n" +
                          r.command +
                          "\n\nCopy this reviewed command to the clipboard?";
    if (MessageBoxW(e->window, Wide(preview).c_str(),
                    L"Review exact command (unsent)", MB_OKCANCEL) == IDOK &&
        PennIDE::ReviewCurrent(d, r, e->endpoint)) {
      auto wide = Wide(r.command);
      if (OpenClipboard(e->window)) {
        EmptyClipboard();
        HGLOBAL h =
            GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t));
        if (h) {
          void *p = GlobalLock(h);
          memcpy(p, wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));
          GlobalUnlock(h);
          if (!SetClipboardData(CF_UNICODETEXT, h))
            GlobalFree(h);
        }
        CloseClipboard();
      }
    }
    break;
  }
  case Find: {
    auto query = Utf8(e->search);
    if (query.empty())
      break;
    Sci(e, SCI_SETTARGETSTART, Sci(e, SCI_GETSELECTIONEND));
    Sci(e, SCI_SETTARGETEND, Sci(e, SCI_GETLENGTH));
    Sci(e, SCI_SETSEARCHFLAGS, 0);
    auto found = Sci(e, SCI_SEARCHINTARGET, query.size(),
                     reinterpret_cast<sptr_t>(query.data()));
    if (found < 0) {
      Sci(e, SCI_SETTARGETSTART, 0);
      Sci(e, SCI_SETTARGETEND, Sci(e, SCI_GETLENGTH));
      found = Sci(e, SCI_SEARCHINTARGET, query.size(),
                  reinterpret_cast<sptr_t>(query.data()));
    }
    if (found >= 0)
      Sci(e, SCI_SETSEL, found, found + query.size());
    break;
  }
  case Replace: {
    auto query = Utf8(e->search), replacement = Utf8(e->replacement);
    auto s = Text(e);
    size_t from = Sci(e, SCI_GETSELECTIONSTART),
           to = Sci(e, SCI_GETSELECTIONEND);
    if (!query.empty() && s.substr(from, to - from) == query &&
        s.size() - (to - from) + replacement.size() <= MaxSource) {
      Sci(e, SCI_BEGINUNDOACTION);
      Sci(e, SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(replacement.c_str()));
      Sci(e, SCI_ENDUNDOACTION);
    }
    break;
  }
  case Recover:
    RestoreRecovery(e);
    break;
  case FullReference: {
    if (FullHelp.empty()) {
      MessageBoxW(e->window,
                  L"Copy the pinned pennmush-reference folder beside the "
                  L"executable to enable full offline help.",
                  L"PennMUSH reference", MB_ICONINFORMATION);
      break;
    }
    HWND ref = OpenPennEditor(e->window, "", FullHelp);
    if (ref)
      SetWindowTextW(ref, L"PennMUSH full upstream reference - read only");
    break;
  }
  }
}
LRESULT CALLBACK EditorKeys(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR,
                            DWORD_PTR data) {
  auto e = reinterpret_cast<Editor *>(data);
  if (msg == WM_KEYDOWN) {
    bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (w == VK_F1) {
      Command(e, Reference);
      return 0;
    }
    if (control && w == 'W') {
      Command(e, CloseDraft);
      return 0;
    }
    if (control && (w == VK_OEM_PLUS || w == VK_ADD)) {
      Command(e, LargerFont);
      return 0;
    }
    if (control && (w == VK_OEM_MINUS || w == VK_SUBTRACT)) {
      Command(e, SmallerFont);
      return 0;
    }
    if (control && w == VK_SPACE) {
      Command(e, Complete);
      return 0;
    }
    if (control && w == 'F') {
      SetFocus(e->search);
      return 0;
    }
    if (w == VK_F3) {
      Command(e, Find);
      return 0;
    }
    if (control && w == 'S') {
      Command(e, Save);
      return 0;
    }
    if (control && w == 'N') {
      Command(e, New);
      return 0;
    }
  }
  return DefSubclassProc(h, msg, w, l);
}
LRESULT CALLBACK Proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
  Editor *e = State(h);
  if (msg == WM_NCCREATE) {
    e = static_cast<Editor *>(
        reinterpret_cast<CREATESTRUCT *>(l)->lpCreateParams);
    e->window = h;
    SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(e));
  }
  if (!e)
    return DefWindowProcW(h, msg, w, l);
  switch (msg) {
  case WM_CREATE: {
    e->tabs = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0,
                            0, h, 0, 0, 0);
    e->text = CreateWindowExW(0, L"Scintilla", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                  WS_HSCROLL,
                              0, 0, 0, 0, h, 0, 0, 0);
    if (!e->text || !e->tabs)
      return -1;
    SetWindowSubclass(e->text, EditorKeys, 1, reinterpret_cast<DWORD_PTR>(e));
    e->status = CreateWindowW(L"STATIC", L"Unsent draft", WS_CHILD | WS_VISIBLE,
                              0, 0, 0, 0, h, 0, 0, 0);
    e->object =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                        0, 0, 0, h, 0, 0, 0);
    e->attribute =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                        0, 0, 0, h, 0, 0, 0);
    e->search =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                        0, 0, 0, h, 0, 0, 0);
    e->replacement =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0,
                        0, 0, 0, h, 0, 0, 0);
    SendMessageW(e->object, EM_SETCUEBANNER, 0,
                 reinterpret_cast<LPARAM>(L"Object #123"));
    SendMessageW(e->attribute, EM_SETCUEBANNER, 0,
                 reinterpret_cast<LPARAM>(L"Attribute"));
    SendMessageW(e->search, EM_SETCUEBANNER, 0,
                 reinterpret_cast<LPARAM>(L"Find (Ctrl+F)"));
    SendMessageW(e->replacement, EM_SETCUEBANNER, 0,
                 reinterpret_cast<LPARAM>(L"Replacement"));
    Sci(e, SCI_SETCODEPAGE, SC_CP_UTF8);
    Sci(e, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
    Sci(e, SCI_SETMARGINWIDTHN, 0, 58);
    Sci(e, SCI_SETWRAPMODE, SC_WRAP_NONE);
    Sci(e, SCI_SETINDENT, 2);
    Sci(e, SCI_INDICSETSTYLE, 0, INDIC_SQUIGGLE);
    Sci(e, SCI_INDICSETFORE, 0, RGB(230, 120, 70));
    Colors(e);
    SetTimer(h, 2, 30000, 0);
    return 0;
  }
  case WM_SIZE: {
    int width = LOWORD(l), height = HIWORD(l);
    MoveWindow(e->tabs, 0, 0, width, 28, TRUE);
    MoveWindow(e->object, 4, 30, 100, 24, TRUE);
    MoveWindow(e->attribute, 108, 30, 160, 24, TRUE);
    MoveWindow(e->search, 280, 30, 150, 24, TRUE);
    MoveWindow(e->replacement, 440, 30, 150, 24, TRUE);
    MoveWindow(e->text, 0, 58, width, std::max(1, height - 104), TRUE);
    MoveWindow(e->status, 4, std::max(58, height - 42), std::max(1, width - 8),
               40, TRUE);
    return 0;
  }
  case WM_COMMAND:
    Command(e, LOWORD(w));
    return 0;
  case WM_NOTIFY: {
    auto n = reinterpret_cast<NMHDR *>(l);
    if (n->hwndFrom == e->tabs && n->code == TCN_SELCHANGE)
      Select(e, TabCtrl_GetCurSel(e->tabs));
    if (n->hwndFrom == e->text) {
      auto sc = reinterpret_cast<SCNotification *>(l);
      if (sc->nmhdr.code == SCN_MODIFIED && !e->loading &&
          (sc->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)))
        SetTimer(h, 1, 180, 0);
      if (sc->nmhdr.code == SCN_UPDATEUI) {
        auto caret = Sci(e, SCI_GETCURRENTPOS);
        if (caret != e->lastCaret) {
          e->lastCaret = caret;
          if (e->recoverable)
            SetTimer(h, 1, 180, 0);
        }
        sptr_t p = Sci(e, SCI_GETCURRENTPOS), match = -1;
        auto c = Sci(e, SCI_GETCHARAT, p);
        if (c == '[' || c == ']' || c == '(' || c == ')' || c == '{' ||
            c == '}')
          match = Sci(e, SCI_BRACEMATCH, p);
        else if (p > 0) {
          --p;
          c = Sci(e, SCI_GETCHARAT, p);
          if (c == '[' || c == ']' || c == '(' || c == ')' || c == '{' ||
              c == '}')
            match = Sci(e, SCI_BRACEMATCH, p);
        }
        Sci(e, SCI_BRACEHIGHLIGHT, match >= 0 ? p : -1, match);
      }
    }
    return 0;
  }
  case WM_TIMER:
    if (w == 1) {
      KillTimer(h, 1);
      if (static_cast<size_t>(Sci(e, SCI_GETLENGTH)) > MaxSource) {
        Sci(e, SCI_UNDO);
        MessageBoxW(
            h, L"The last edit exceeded the 4 MiB source limit and was undone.",
            L"PennMUSH IDE", MB_ICONWARNING);
      }
      Analyze(e);
    } else if (w == 2 && e->recoverable) {
      Sync(e);
      if (!SaveRecovery())
        SetWindowTextW(e->status,
                       L"Draft recovery failed or exceeded 32 drafts / 8 MiB. "
                       L"Save source files explicitly.");
    }
    return 0;
  case WM_CLOSE:
    Sync(e);
    if (e->recoverable && !e->docs.empty()) {
      int answer = MessageBoxW(
          h,
          L"Keep the open unsent drafts in local recovery before closing? "
          L"Recovery stores source only, without destination bindings or "
          L"grants.",
          L"Unsent draft", MB_YESNOCANCEL);
      if (answer == IDCANCEL)
        return 0;
      if (answer == IDYES && !SaveRecovery()) {
        MessageBoxW(h, L"Recovery could not be saved.", L"PennMUSH IDE",
                    MB_ICONERROR);
        return 0;
      }
    }
    DestroyWindow(h);
    return 0;
  case WM_DESTROY:
    if (e->recoverable)
      Preferences(e, true);
    for (auto &d : e->docs)
      Sci(e, SCI_RELEASEDOCUMENT, 0, d.scintilla);
    return 0;
  case WM_NCDESTROY:
    Windows.erase(std::remove(Windows.begin(), Windows.end(), h),
                  Windows.end());
    SetWindowLongPtrW(h, GWLP_USERDATA, 0);
    delete e;
    return DefWindowProcW(h, msg, w, l);
  }
  return DefWindowProcW(h, msg, w, l);
}
} // namespace
bool PennEditorConfigureStorage(const std::wstring &folder) {
  if (!Windows.empty() || folder.empty())
    return false;
  StorageFolder = folder;
  return CreateDirectoryW(folder.c_str(), 0) ||
         GetLastError() == ERROR_ALREADY_EXISTS;
}
HWND OpenPennEditor(HWND owner, const std::string &endpoint,
                    const std::string &source) {
  if (source.size() > MaxSource || !PennIDE::ValidUtf8(source))
    return 0;
  static HMODULE module = 0;
  if (!module) {
    wchar_t path[32768] = {};
    GetModuleFileNameW(0, path, 32768);
    std::wstring p = path;
    auto slash = p.find_last_of(L"\\/");
    p = p.substr(0, slash + 1) + L"Scintilla.dll";
    module = LoadLibraryExW(p.c_str(), 0, LOAD_WITH_ALTERED_SEARCH_PATH);
  }
  if (!module) {
    MessageBoxW(owner,
                L"The locally built Scintilla.dll is required beside "
                L"MUSHclient.exe. See docs/PENN_IDE.md.",
                L"PennMUSH IDE unavailable", MB_ICONERROR);
    return 0;
  }
  INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_TAB_CLASSES};
  InitCommonControlsEx(&controls);
  WNDCLASSW c = {};
  c.lpfnWndProc = Proc;
  c.hInstance = GetModuleHandleW(0);
  c.lpszClassName = L"MUSHclientPennIDE";
  c.hCursor = LoadCursor(0, IDC_ARROW);
  c.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  RegisterClassW(&c);
  LoadCatalog();
  auto e = new Editor;
  e->endpoint = endpoint;
  e->recoverable = !endpoint.empty();
  Preferences(e, false);
  HMENU menu = CreateMenu(), file = CreatePopupMenu(), edit = CreatePopupMenu();
  AppendMenuW(file, MF_STRING, New, L"New unsent draft");
  AppendMenuW(file, MF_STRING, Open, L"Open UTF-8 source...");
  AppendMenuW(file, MF_STRING, Save, L"Save source...");
  AppendMenuW(file, MF_STRING, Recover, L"Recover last active draft");
  AppendMenuW(edit, MF_STRING, Undo, L"Undo");
  AppendMenuW(edit, MF_STRING, Redo, L"Redo");
  AppendMenuW(edit, MF_STRING, Complete, L"Complete at caret");
  AppendMenuW(edit, MF_STRING, Find, L"Find next (first search field)");
  AppendMenuW(edit, MF_STRING, Replace,
              L"Replace selected match (second search field)");
  AppendMenuW(edit, MF_STRING, Reference,
              L"Contextual command/function reference (F1)");
  AppendMenuW(edit, MF_STRING, FullReference,
              L"Full offline PennMUSH reference");
  AppendMenuW(edit, MF_STRING, Theme, L"Toggle dark/light");
  AppendMenuW(edit, MF_STRING, Language,
              L"Toggle PennMUSH / Lua language mode");
  AppendMenuW(edit, MF_STRING, Review, L"Review attribute command and copy...");
  AppendMenuW(edit, MF_STRING, ReadAttribute,
              L"Read attribute (explicit request)...");
  AppendMenuW(edit, MF_STRING, Discover,
              L"Discover game functions (explicit request)...");
  AppendMenuW(edit, MF_STRING, SendCommands,
              L"Review and send prepared commands...");
  AppendMenuW(file, MF_STRING, RenameDraft, L"Rename current draft...");
  AppendMenuW(file, MF_STRING, CloseDraft, L"Close current draft (Ctrl+W)");
  AppendMenuW(edit, MF_STRING, LargerFont, L"Larger source font (Ctrl++)");
  AppendMenuW(edit, MF_STRING, SmallerFont, L"Smaller source font (Ctrl+-)");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"File");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit),
              L"Edit / assistance");
  auto prefsPath = Storage() + L"\\preferences.ini";
  int width = std::max(
          640,
          std::min(2400, static_cast<int>(GetPrivateProfileIntW(
                             L"Editor", L"Width", 1000, prefsPath.c_str())))),
      height = std::max(
          420,
          std::min(1600, static_cast<int>(GetPrivateProfileIntW(
                             L"Editor", L"Height", 700, prefsPath.c_str()))));
  POINT position = {};
  GetCursorPos(&position);
  MONITORINFO monitor = {sizeof(monitor)};
  if (GetMonitorInfoW(MonitorFromPoint(position, MONITOR_DEFAULTTONEAREST),
                      &monitor)) {
    width = std::min(
        width, static_cast<int>(monitor.rcWork.right - monitor.rcWork.left));
    height = std::min(
        height, static_cast<int>(monitor.rcWork.bottom - monitor.rcWork.top));
  }
  HWND h =
      CreateWindowExW(0, c.lpszClassName, L"PennMUSH IDE - local unsent drafts",
                      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width,
                      height, owner, menu, c.hInstance, e);
  if (!h)
    return 0;
  Add(e, source);
  if (!e->recoverable) {
    for (int id :
         {New, Open, Undo, Redo, Complete, Language, Review, ReadAttribute,
          Discover, SendCommands, RenameDraft, Recover, Replace})
      EnableMenuItem(menu, id, MF_BYCOMMAND | MF_GRAYED);
    EnableWindow(e->object, FALSE);
    EnableWindow(e->attribute, FALSE);
    EnableWindow(e->replacement, FALSE);
  }
  Windows.push_back(h);
  ShowWindow(h, SW_SHOW);
  return h;
}
bool PennEditorSnapshot(HWND h, std::string &s, uint64_t &r,
                        std::string &endpoint) {
  auto e = State(h);
  if (!e || !e->recoverable ||
      static_cast<size_t>(Sci(e, SCI_GETLENGTH)) > MaxSource)
    return false;
  Sync(e);
  auto &d = e->docs[e->active].draft;
  s = d.source;
  r = d.revision;
  endpoint = d.endpoint;
  return true;
}
bool PennEditorSnapshotInfo(HWND h, PennEditorInfo &info) {
  auto e = State(h);
  if (!e)
    return false;
  if (!PennEditorSnapshot(h, info.source, info.revision, info.endpoint))
    return false;
  info.documentID = e->docs[e->active].id;
  info.window = h;
  return true;
}
bool PennEditorSaveRecoveryFile(const std::wstring &path) {
  return SaveRecovery(path);
}
bool PennEditorRestoreRecoveryFile(HWND h, const std::wstring &path) {
  auto e = State(h);
  return e && e->recoverable && RestoreRecovery(e, path);
}
std::vector<PennEditorInfo> PennEditorListSnapshots() {
  std::vector<PennEditorInfo> result;
  for (auto h : Windows) {
    auto e = State(h);
    if (!e || !e->recoverable)
      continue;
    Sync(e);
    for (const auto &d : e->docs) {
      PennEditorInfo info;
      info.window = h;
      info.documentID = d.id;
      info.revision = d.draft.revision;
      info.endpoint = d.draft.endpoint;
      info.source = d.draft.source;
      result.push_back(info);
    }
  }
  return result;
}
bool PennEditorApplyDocumentProposal(uint64_t id, uint64_t revision,
                                     const std::string &replacement) {
  if (!PennIDE::ValidUtf8(replacement) || replacement.size() > MaxSource)
    return false;
  for (auto h : Windows) {
    auto e = State(h);
    if (!e || !e->recoverable)
      continue;
    Sync(e);
    for (size_t i = 0; i < e->docs.size(); ++i) {
      auto &d = e->docs[i];
      if (d.id != id)
        continue;
      if (d.draft.revision != revision)
        return false;
      Select(e, i);
      return PennEditorApplyProposal(h, revision, 0, d.draft.source.size(),
                                     replacement);
    }
  }
  return false;
}
void PennEditorSetTransport(
    std::function<bool(const std::string &, const std::string &)> transport) {
  Transport = transport;
}
bool PennEditorApplyReviewedDocumentProposal(
    uint64_t id, uint64_t revision, const std::string &replacement,
    const std::function<bool()> &validator) {
  if (!validator || !PennIDE::ValidUtf8(replacement) ||
      replacement.size() > MaxSource)
    return false;
  for (auto h : Windows) {
    auto e = State(h);
    if (!e)
      continue;
    Sync(e);
    for (size_t i = 0; i < e->docs.size(); ++i) {
      auto &d = e->docs[i];
      if (d.id != id)
        continue;
      if (d.draft.revision != revision || d.draft.endpoint.empty() ||
          Sci(e, SCI_GETREADONLY))
        return false;
      Select(e, i);
      if (d.draft.revision != revision || !validator())
        return false;
      Sci(e, SCI_BEGINUNDOACTION);
      Sci(e, SCI_SETTARGETSTART, 0);
      Sci(e, SCI_SETTARGETEND, Sci(e, SCI_GETLENGTH));
      Sci(e, SCI_REPLACETARGET, replacement.size(),
          reinterpret_cast<sptr_t>(replacement.data()));
      Sci(e, SCI_ENDUNDOACTION);
      Sync(e);
      Analyze(e);
      return true;
    }
  }
  return false;
}
void PennEditorSetSendPrepared(
    std::function<bool(const std::string &, uint64_t, uint64_t,
                       const std::vector<std::string> &)>
        sender) {
  SendPrepared = sender;
}
void PennEditorInvalidateEndpoint(const std::string &endpoint) {
  auto ownership = ReplyOwnership.find(endpoint);
  if (ownership != ReplyOwnership.end()) {
    OwnedNonceCount -= ownership->second.size();
    ReplyOwnership.erase(ownership);
  }
  for (auto h : Windows) {
    auto e = State(h);
    if (e && e->endpoint == endpoint) {
      e->endpoint.clear();
      e->read = PennIDE::ReadRequest();
      e->discovery = PennIDE::ReadRequest();
      e->discovered.clear();
      for (auto &d : e->docs) {
        d.draft.endpoint.clear();
        ++d.draft.revision;
      }
      SetWindowTextW(e->status, L"Destination disconnected. Drafts remain "
                                L"unsent; game requests are disabled.");
    }
  }
}
bool PennEditorHandleOutput(const std::string &endpoint,
                            const std::string &line) {
  size_t separator = 0;
  while (separator < line.size() && separator <= 64 && line[separator] != ' ')
    ++separator;
  if (separator > 64)
    return false;
  const auto nonce = line.substr(0, separator);
  if (!KnownReply(endpoint, nonce))
    return false;
  if (line.size() > 100000)
    return true;
  for (auto h : Windows) {
    auto e = State(h);
    if (!e || e->endpoint != endpoint)
      continue;
    if (!e->discovery.nonce.empty() &&
        line.find(e->discovery.nonce + " ") == 0) {
      auto names = line.substr(e->discovery.nonce.size() + 1);
      if (PennIDE::AcceptDiscovery(e->discovery, endpoint, e->discovery.nonce,
                                   names, e->discovered))
        SetWindowTextW(e->status,
                       L"Game function names cached for this connection. "
                       L"Unknown functions remain legal.");
    }
    if (!e->read.consumed && !e->read.nonce.empty() &&
        line.find(e->read.nonce + " ") == 0 &&
        e->readDocument < e->docs.size()) {
      auto encoded = line.substr(e->read.nonce.size() + 1);
      DWORD n = 0;
      std::string source;
      if (!encoded.empty()) {
        if (!CryptStringToBinaryA(encoded.c_str(), encoded.size(),
                                  CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT, 0,
                                  &n, 0, 0) ||
            n > 65536)
          continue;
        source.resize(n);
        if (n && !CryptStringToBinaryA(
                     encoded.c_str(), encoded.size(),
                     CRYPT_STRING_BASE64 | CRYPT_STRING_STRICT,
                     reinterpret_cast<BYTE *>(&source[0]), &n, 0, 0))
          continue;
      }
      Sync(e);
      auto &draft = e->docs[e->readDocument].draft;
      if (PennIDE::AcceptAttributeRead(draft, e->read, endpoint, e->read.nonce,
                                       source)) {
        size_t selected = e->active;
        e->loading = true;
        Sci(e, SCI_SETDOCPOINTER, 0, e->docs[e->readDocument].scintilla);
        Sci(e, SCI_BEGINUNDOACTION);
        Sci(e, SCI_SETTARGETSTART, 0);
        Sci(e, SCI_SETTARGETEND, Sci(e, SCI_GETLENGTH));
        Sci(e, SCI_REPLACETARGET, source.size(),
            reinterpret_cast<sptr_t>(source.data()));
        Sci(e, SCI_ENDUNDOACTION);
        Sci(e, SCI_SETDOCPOINTER, 0, e->docs[selected].scintilla);
        e->loading = false;
        Analyze(e);
      } else
        SetWindowTextW(e->status, L"Read response rejected: draft revision, "
                                  L"destination or response changed.");
    }
  }
  return true;
}
bool PennEditorApplyProposal(HWND h, uint64_t revision, size_t from, size_t to,
                             const std::string &replacement) {
  auto e = State(h);
  if (!e || !e->recoverable || Sci(e, SCI_GETREADONLY) ||
      static_cast<size_t>(Sci(e, SCI_GETLENGTH)) > MaxSource)
    return false;
  Sync(e);
  auto &d = e->docs[e->active].draft;
  PennIDE::Draft candidate = d;
  if (!candidate.Apply(from, to, replacement, revision))
    return false;
  std::string preview = "Current selection:\n" +
                        d.source.substr(from, to - from) +
                        "\n\nProposed replacement:\n" + replacement;
  if (MessageBoxW(h, Wide(preview).c_str(), L"Review proposed edit",
                  MB_OKCANCEL) != IDOK)
    return false;
  Sync(e);
  if (d.revision != revision)
    return false;
  Sci(e, SCI_BEGINUNDOACTION);
  Sci(e, SCI_SETTARGETSTART, from);
  Sci(e, SCI_SETTARGETEND, to);
  Sci(e, SCI_REPLACETARGET, replacement.size(),
      reinterpret_cast<sptr_t>(replacement.data()));
  Sci(e, SCI_ENDUNDOACTION);
  Sync(e);
  Analyze(e);
  return true;
}
