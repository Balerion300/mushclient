#define NOMINMAX
#include "LogStartSetup.h"
#include <algorithm>
#include <cstring>
#include <cwctype>

namespace {
enum { Normal = 101, Script, Target, Function, World };

struct SetupState {
  LogStartSetupOptions &options;
  std::vector<std::wstring> targets;
  int unavailable = -1;
};

template <typename T>
void Append(std::vector<unsigned char> &bytes, const T &value) {
  auto size = bytes.size();
  bytes.resize(size + sizeof(value));
  std::memcpy(bytes.data() + size, &value, sizeof(value));
}
void Text(std::vector<unsigned char> &bytes, const wchar_t *text) {
  do {
    Append(bytes, *text);
  } while (*text++);
}
void Control(std::vector<unsigned char> &bytes, DWORD style, short x, short y,
             short width, short height, WORD id, WORD kind,
             const wchar_t *text) {
  while (bytes.size() % 4)
    bytes.push_back(0);
  DLGITEMTEMPLATE control = {};
  control.style = WS_CHILD | WS_VISIBLE | style;
  control.x = x;
  control.y = y;
  control.cx = width;
  control.cy = height;
  control.id = id;
  Append(bytes, control);
  Append(bytes, WORD(0xffff));
  Append(bytes, kind);
  Text(bytes, text);
  Append(bytes, WORD(0));
}
std::vector<unsigned char> Template() {
  std::vector<unsigned char> bytes;
  DLGTEMPLATE dialog = {};
  dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT;
  dialog.cdit = 11;
  dialog.cx = 382;
  dialog.cy = 246;
  Append(bytes, dialog);
  Append(bytes, WORD(0)); // No menu or registered window class.
  Append(bytes, WORD(0));
  Text(bytes, L"Logging Setup");
  Append(bytes, WORD(9));
  Text(bytes, L"Segoe UI");
  Control(bytes, SS_LEFT, 12, 12, 358, 24, World, 0x82, L"");
  Control(bytes, BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 12, 41, 358, 14,
          Normal, 0x80, L"&Normal logging (choose a file when starting)");
  Control(bytes, BS_AUTORADIOBUTTON | WS_TABSTOP, 12, 63, 358, 14, Script,
          0x80, L"Run a start &script");
  Control(bytes, SS_LEFT, 24, 89, 70, 12, WORD(-1), 0x82, L"Script &source:");
  Control(bytes, CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
          97, 86, 273, 125, Target, 0x85, L"");
  Control(bytes, SS_LEFT, 24, 117, 70, 12, WORD(-1), 0x82, L"&Function name:");
  Control(bytes, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, 97, 114, 273, 16,
          Function, 0x81, L"");
  Control(bytes, SS_LEFT, 24, 139, 346, 19, WORD(-1), 0x82,
          L"Leave the function name empty to use normal logging.");
  Control(bytes, SS_LEFT, 12, 165, 358, 47, WORD(-1), 0x82,
          L"File > Log session runs the chosen function when starting a log. "
          L"Your script can gather server details with triggers, then call "
          L"OpenLogExclusive(filename) and WriteLog(text) to create the log "
          L"and write its header.");
  Control(bytes, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 244, 219, 60, 17,
          IDOK, 0x80, L"Save");
  Control(bytes, BS_PUSHBUTTON | WS_TABSTOP, 310, 219, 60, 17, IDCANCEL, 0x80,
          L"Cancel");
  return bytes;
}
std::wstring GetText(HWND control) {
  int length = GetWindowTextLengthW(control);
  std::wstring value(static_cast<size_t>(length) + 1, L'\0');
  GetWindowTextW(control, &value[0], length + 1);
  value.resize(length);
  auto first = value.find_first_not_of(L" \t\r\n");
  if (first == std::wstring::npos)
    return L"";
  return value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
}
void EnableScript(HWND dialog) {
  bool script = IsDlgButtonChecked(dialog, Script) == BST_CHECKED;
  EnableWindow(GetDlgItem(dialog, Target), script);
  EnableWindow(GetDlgItem(dialog, Function), script);
}
void Center(HWND dialog, HWND owner) {
  RECT bounds = {}, parent = {};
  GetWindowRect(dialog, &bounds);
  if (!owner || !GetWindowRect(owner, &parent))
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &parent, 0);
  MONITORINFO monitor = {sizeof(monitor)};
  if (!GetMonitorInfoW(MonitorFromRect(&parent, MONITOR_DEFAULTTONEAREST),
                       &monitor))
    monitor.rcWork = parent;
  int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
  int x = parent.left + (parent.right - parent.left - width) / 2;
  int y = parent.top + (parent.bottom - parent.top - height) / 2;
  x = std::max(static_cast<int>(monitor.rcWork.left),
               std::min(x, static_cast<int>(monitor.rcWork.right) - width));
  y = std::max(static_cast<int>(monitor.rcWork.top),
               std::min(y, static_cast<int>(monitor.rcWork.bottom) - height));
  SetWindowPos(dialog, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}
INT_PTR CALLBACK Proc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
  auto state = reinterpret_cast<SetupState *>(
      GetWindowLongPtrW(dialog, DWLP_USER));
  if (message == WM_INITDIALOG) {
    state = reinterpret_cast<SetupState *>(lp);
    SetWindowLongPtrW(dialog, DWLP_USER, lp);
    SetDlgItemTextW(dialog, World,
                    (L"Logging for: " + state->options.worldName).c_str());
    SendDlgItemMessageW(dialog, Function, EM_SETLIMITTEXT, 255, 0);
    SetDlgItemTextW(dialog, Function, state->options.script.c_str());
    auto target = GetDlgItem(dialog, Target);
    SendMessageW(target, CB_ADDSTRING, 0,
                  reinterpret_cast<LPARAM>(L"World script"));
    state->targets.push_back(L"");
    int selected = 0;
    for (const auto &plugin : state->options.plugins) {
      auto label = plugin.name + L" (" + plugin.id + L")";
      SendMessageW(target, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(label.c_str()));
      state->targets.push_back(plugin.id);
      if (_wcsicmp(plugin.id.c_str(), state->options.plugin.c_str()) == 0)
        selected = static_cast<int>(state->targets.size()) - 1;
    }
    if (!state->options.plugin.empty() && selected == 0) {
      auto label = L"Unavailable plugin (" + state->options.plugin + L")";
      SendMessageW(target, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(label.c_str()));
      selected = state->unavailable = static_cast<int>(state->targets.size());
      state->targets.push_back(state->options.plugin);
    }
    SendMessageW(target, CB_SETCURSEL, selected, 0);
    CheckRadioButton(dialog, Normal, Script,
                      state->options.script.empty() ? Normal : Script);
    EnableScript(dialog);
    Center(dialog, GetWindow(dialog, GW_OWNER));
    return TRUE;
  }
  if (!state)
    return FALSE;
  if (message == WM_CLOSE) {
    EndDialog(dialog, IDCANCEL);
    return TRUE;
  }
  if (message != WM_COMMAND)
    return FALSE;
  switch (LOWORD(wp)) {
  case Normal:
  case Script:
    EnableScript(dialog);
    return TRUE;
  case IDOK: {
    auto script = IsDlgButtonChecked(dialog, Script) == BST_CHECKED
                      ? GetText(GetDlgItem(dialog, Function))
                      : std::wstring();
    std::wstring plugin;
    if (!script.empty()) {
      int selected = static_cast<int>(
          SendDlgItemMessageW(dialog, Target, CB_GETCURSEL, 0, 0));
      if (selected < 0 || selected == state->unavailable ||
          static_cast<size_t>(selected) >= state->targets.size()) {
        MessageBoxW(dialog, L"Choose World script or an enabled plugin for this "
                              L"world before saving the start script.",
                    L"Logging Setup", MB_OK | MB_ICONINFORMATION);
        SetFocus(GetDlgItem(dialog, Target));
        return TRUE;
      }
      plugin = state->targets[selected];
    }
    state->options.script = script;
    state->options.plugin = plugin;
    EndDialog(dialog, IDOK);
    return TRUE;
  }
  case IDCANCEL:
    EndDialog(dialog, IDCANCEL);
    return TRUE;
  }
  return FALSE;
}
} // namespace

bool ShowLogStartSetup(HWND owner, LogStartSetupOptions &options) {
  auto bytes = Template();
  SetupState state{options};
  auto result = DialogBoxIndirectParamW(
      GetModuleHandleW(0), reinterpret_cast<const DLGTEMPLATE *>(bytes.data()),
      owner, Proc, reinterpret_cast<LPARAM>(&state));
  if (result == -1)
    MessageBoxW(owner, L"Logging Setup could not be opened.", L"Logging Setup",
                  MB_OK | MB_ICONERROR);
  return result == IDOK;
}
