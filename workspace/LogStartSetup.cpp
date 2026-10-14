#define NOMINMAX
#include "LogStartSetup.h"
#include "LogNameConfig.h"
#include <algorithm>
#include <cstring>
#include <cwctype>

namespace {
enum { Normal = 101, Script, Target, Function, World, ConfigureNames };
const wchar_t *NamingPlugin = L"7c1e480cf1d24cb28f9a1062";

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
  dialog.cdit = 12;
  dialog.cx = 382;
  dialog.cy = 268;
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
          0x80, L"Advanced: run a start &script");
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
  Control(bytes, BS_PUSHBUTTON | WS_TABSTOP | WS_GROUP, 12, 219, 222, 17,
          ConfigureNames, 0x80, L"Set up automatic file &names...");
  Control(bytes, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 244, 241, 60, 17,
          IDOK, 0x80, L"Save");
  Control(bytes, BS_PUSHBUTTON | WS_TABSTOP, 310, 241, 60, 17, IDCANCEL, 0x80,
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
enum { Folder = 201, Filename, Request, StartReply, EndReply, Header,
       Fields, FieldName, FieldRegex, Capture, Transform, Required,
       AddField, RemoveField };
bool LocalEncodingFailed = false;
std::string LocalText(const std::wstring &text) {
  if (text.empty()) return "";
  BOOL substituted = FALSE;
  int size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(),
                                 static_cast<int>(text.size()), 0, 0, 0, &substituted);
  if (!size || substituted) { LocalEncodingFailed = true; return ""; }
  std::string result(size, '\0');
  if (size)
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(), static_cast<int>(text.size()),
                         &result[0], size, 0, &substituted);
  if (substituted) LocalEncodingFailed = true;
  return result;
}
std::wstring Wide(const std::string &text) {
  int size = MultiByteToWideChar(CP_ACP, 0, text.data(),
                                 static_cast<int>(text.size()), 0, 0);
  std::wstring result(size, L'\0');
  if (size)
    MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()),
                         &result[0], size);
  return result;
}
std::vector<unsigned char> NamesTemplate() {
  std::vector<unsigned char> bytes;
  DLGTEMPLATE dialog = {};
  dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT;
  dialog.cx = 440; dialog.cy = 350;
  Append(bytes, dialog); Append(bytes, WORD(0)); Append(bytes, WORD(0));
  Text(bytes, L"Automatic log file names"); Append(bytes, WORD(9)); Text(bytes, L"Segoe UI");
  WORD count = 0;
  auto control = [&](DWORD style, short x, short y, short width, short height,
                     WORD id, WORD kind, const wchar_t *text) {
    Control(bytes, style, x, y, width, height, id, kind, text); ++count;
  };
  auto label = [&](short x, short y, short width, const wchar_t *text) {
    control(SS_LEFT, x, y, width, 12, WORD(-1), 0x82, text);
  };
  auto edit = [&](short x, short y, short width, WORD id) {
    control(WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, x, y, width, 16, id, 0x81, L"");
  };
  control(SS_LEFT, 12, 10, 416, 20, WORD(-1), 0x82,
          L"Choose file names for this world. Optional reply rules can use server "
          L"details. Nothing is sent or logged while you configure these settings.");
  label(12, 38, 70, L"Existing &folder:"); edit(88, 35, 340, Folder);
  label(12, 60, 70, L"File &name:"); edit(88, 57, 340, Filename);
  control(SS_LEFT, 88, 78, 340, 20, WORD(-1), 0x82,
          L"Use {world}, {real_date}, {real_time}, {number}, or a captured field "
          L"such as {game_year}. Existing files are never overwritten.");
  label(12, 106, 100, L"Optional &request:"); edit(118, 103, 310, Request);
  label(12, 128, 100, L"Reply &starts (regex):"); edit(118, 125, 310, StartReply);
  label(12, 150, 100, L"Reply &ends (regex):"); edit(118, 147, 310, EndReply);
  control(BS_AUTOCHECKBOX | WS_TABSTOP, 118, 169, 310, 13, Header, 0x80,
          L"Keep the entire reply block at the start of the log");
  label(12, 193, 100, L"Captured &fields:");
  control(CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
          118, 190, 310, 110, Fields, 0x85, L"");
  label(12, 215, 70, L"Field name:"); edit(88, 212, 150, FieldName);
  label(250, 215, 62, L"Capture:"); edit(315, 212, 113, Capture);
  label(12, 237, 70, L"Match (regex):"); edit(88, 234, 340, FieldRegex);
  label(12, 259, 70, L"Value format:");
  control(CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
          88, 256, 150, 100, Transform, 0x85, L"");
  control(BS_AUTOCHECKBOX | WS_TABSTOP, 250, 259, 178, 13, Required, 0x80,
          L"Required before creating the log");
  control(BS_PUSHBUTTON | WS_TABSTOP, 88, 281, 150, 17, AddField, 0x80, L"Add / update field");
  control(BS_PUSHBUTTON | WS_TABSTOP, 250, 281, 178, 17, RemoveField, 0x80, L"Remove selected field");
  control(SS_LEFT, 12, 303, 416, 20, WORD(-1), 0x82,
          L"Capture is a group number (1, 2, ...) or a named group. Example: "
          L"field game_year, match ^Year: (\\d+)$, capture 1. Request and rules "
          L"run only when you later choose Start Logging. Reply collection is bounded to 30 seconds.");
  control(BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 302, 327, 60, 17, IDOK, 0x80, L"Use settings");
  control(BS_PUSHBUTTON | WS_TABSTOP, 368, 327, 60, 17, IDCANCEL, 0x80, L"Cancel");
  reinterpret_cast<DLGTEMPLATE *>(bytes.data())->cdit = count;
  return bytes;
}
struct NamesState { LogNameConfiguration config; int selected = -1; };
bool Name(const std::string &name) {
  return !name.empty() && name.size() <= 64 &&
         ((name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z') || name[0] == '_') &&
         name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
}
bool TemplateFields(const LogNameConfiguration &config) {
  const auto &text = config.filename;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '}') return false;
    if (text[i] != '{') continue;
    size_t end = text.find('}', i + 1);
    if (end == std::string::npos) return false;
    auto key = text.substr(i + 1, end - i - 1);
    bool known = key == "world" || key == "real_date" || key == "real_time" || key == "number";
    for (auto &field : config.fields) if (key == field.name) known = true;
    if (!known) return false;
    i = end;
  }
  return true;
}
void FillFields(HWND dialog, NamesState &state, int selected = -1) {
  SendDlgItemMessageW(dialog, Fields, CB_RESETCONTENT, 0, 0);
  for (auto &field : state.config.fields)
    SendDlgItemMessageW(dialog, Fields, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(Wide(field.name).c_str()));
  SendDlgItemMessageW(dialog, Fields, CB_SETCURSEL, selected, 0);
  state.selected = selected;
  LogNameField field;
  if (selected >= 0) field = state.config.fields[selected];
  SetDlgItemTextW(dialog, FieldName, Wide(field.name).c_str());
  SetDlgItemTextW(dialog, FieldRegex, Wide(field.regex).c_str());
  SetDlgItemTextW(dialog, Capture, Wide(field.capture).c_str());
  int format = field.transform == "number" ? 1 : field.transform == "pad2" ? 2 : field.transform == "month_number" ? 3 : 0;
  SendDlgItemMessageW(dialog, Transform, CB_SETCURSEL, format, 0);
  CheckDlgButton(dialog, Required, field.required ? BST_CHECKED : BST_UNCHECKED);
}
bool CaptureField(HWND dialog, NamesState &state) {
  LogNameField field;
  LocalEncodingFailed = false;
  field.name = LocalText(GetText(GetDlgItem(dialog, FieldName)));
  field.regex = LocalText(GetText(GetDlgItem(dialog, FieldRegex)));
  if (LocalEncodingFailed) {
    MessageBoxW(dialog, L"A field contains characters this client's script "
                         L"interface cannot represent. Settings remain unchanged.",
                L"Captured field", MB_ICONWARNING); return false;
  }
  if (field.name.empty() && field.regex.empty()) return true;
  field.capture = LocalText(GetText(GetDlgItem(dialog, Capture)));
  int format = static_cast<int>(SendDlgItemMessageW(dialog, Transform, CB_GETCURSEL, 0, 0));
  const char *formats[] = {"", "number", "pad2", "month_number"};
  field.transform = formats[format < 0 || format > 3 ? 0 : format];
  field.required = IsDlgButtonChecked(dialog, Required) == BST_CHECKED;
  bool numeric = !field.capture.empty() && field.capture.size() <= 2 &&
                 field.capture.find_first_not_of("0123456789") == std::string::npos &&
                 field.capture != "0" && field.capture != "00";
  bool duplicate = false;
  for (size_t i = 0; i < state.config.fields.size(); ++i)
    if (static_cast<int>(i) != state.selected && state.config.fields[i].name == field.name)
      duplicate = true;
  if (LocalEncodingFailed || !Name(field.name) || field.name == "number" || duplicate ||
      field.regex.empty() || (!numeric && !Name(field.capture)) ||
      (state.selected < 0 && state.config.fields.size() >= 16)) {
    MessageBoxW(dialog, L"Use a unique field name (letters, numbers, underscore; "
                         L"starting with a letter), a match regex, and a capture "
                         L"number from 1 to 99 or a named group. 'number' is "
                         L"reserved. At most 16 fields can be configured.",
                L"Captured field", MB_ICONWARNING);
    return false;
  }
  if (state.selected < 0) {
    state.config.fields.push_back(field);
    state.selected = static_cast<int>(state.config.fields.size()) - 1;
  }
  else state.config.fields[state.selected] = field;
  return true;
}
INT_PTR CALLBACK NamesProc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
  auto state = reinterpret_cast<NamesState *>(GetWindowLongPtrW(dialog, DWLP_USER));
  if (message == WM_INITDIALOG) {
    state = reinterpret_cast<NamesState *>(lp); SetWindowLongPtrW(dialog, DWLP_USER, lp);
    auto &cfg = state->config;
    for (int id : {Folder, Filename, Request, StartReply, EndReply, FieldName, FieldRegex, Capture})
      SendDlgItemMessageW(dialog, id, EM_SETLIMITTEXT, 4096, 0);
    SetDlgItemTextW(dialog, Folder, Wide(cfg.folder).c_str());
    SetDlgItemTextW(dialog, Filename, Wide(cfg.filename).c_str());
    SetDlgItemTextW(dialog, Request, Wide(cfg.request).c_str());
    SetDlgItemTextW(dialog, StartReply, Wide(cfg.start).c_str());
    SetDlgItemTextW(dialog, EndReply, Wide(cfg.end).c_str());
    CheckDlgButton(dialog, Header, cfg.header ? BST_CHECKED : BST_UNCHECKED);
    for (auto name : {L"As captured", L"Unsigned number", L"Two or more digits", L"Month name to 01-12"})
      SendDlgItemMessageW(dialog, Transform, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    FillFields(dialog, *state); Center(dialog, GetWindow(dialog, GW_OWNER)); return TRUE;
  }
  if (!state) return FALSE;
  if (message == WM_CLOSE) { EndDialog(dialog, IDCANCEL); return TRUE; }
  if (message != WM_COMMAND) return FALSE;
  switch (LOWORD(wp)) {
  case Fields:
    if (HIWORD(wp) == CBN_SELCHANGE) {
      int selected = static_cast<int>(SendDlgItemMessageW(dialog, Fields, CB_GETCURSEL, 0, 0));
      if (CaptureField(dialog, *state)) FillFields(dialog, *state, selected);
      else SendDlgItemMessageW(dialog, Fields, CB_SETCURSEL, state->selected, 0);
    }
    return TRUE;
  case AddField:
    if (CaptureField(dialog, *state)) FillFields(dialog, *state);
    return TRUE;
  case RemoveField:
    if (state->selected >= 0) state->config.fields.erase(state->config.fields.begin() + state->selected);
    FillFields(dialog, *state); return TRUE;
  case IDOK: {
    if (!CaptureField(dialog, *state)) return TRUE;
    auto &cfg = state->config;
    LocalEncodingFailed = false;
    cfg.folder = LocalText(GetText(GetDlgItem(dialog, Folder)));
    cfg.filename = LocalText(GetText(GetDlgItem(dialog, Filename)));
    cfg.request = LocalText(GetText(GetDlgItem(dialog, Request)));
    cfg.start = LocalText(GetText(GetDlgItem(dialog, StartReply)));
    cfg.end = LocalText(GetText(GetDlgItem(dialog, EndReply)));
    cfg.header = IsDlgButtonChecked(dialog, Header) == BST_CHECKED;
    if (!TemplateFields(cfg)) {
      MessageBoxW(dialog, L"Every {field} in the file name must be world, "
                           L"real_date, real_time, number, or a captured field "
                           L"you added. Check the names and braces.",
                  L"Automatic log file names", MB_ICONWARNING); return TRUE;
    }
    auto folder = Wide(cfg.folder);
    auto attributes = GetFileAttributesW(folder.c_str());
    bool absolute = folder.size() > 2 && ((folder[1] == ':' && (folder[2] == '\\' || folder[2] == '/')) || folder.substr(0, 2) == L"\\\\");
    if (LocalEncodingFailed || !absolute || attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        cfg.filename.empty() || cfg.filename.size() > 220 ||
        cfg.filename.find_first_of("\\/:\r\n") != std::string::npos ||
        cfg.request.find_first_of("\r\n") != std::string::npos ||
        (!cfg.request.empty() && (cfg.start.empty() || cfg.end.empty())) ||
        (cfg.request.empty() && (!cfg.fields.empty() || !cfg.start.empty() || !cfg.end.empty())) ||
        EncodeLogNames(cfg).size() > 65536) {
      MessageBoxW(dialog, L"Choose an existing absolute folder and a file name "
                           L"without path separators. A metadata request must "
                           L"be one line and needs both reply delimiters. "
                           L"Captured fields require a metadata request.",
                  L"Automatic log file names", MB_ICONWARNING); return TRUE;
    }
    EndDialog(dialog, IDOK); return TRUE;
  }
  case IDCANCEL: EndDialog(dialog, IDCANCEL); return TRUE;
  }
  return FALSE;
}
bool ConfigureFileNames(HWND owner, SetupState &setup) {
  auto &options = setup.options;
  auto plugin = std::find_if(options.plugins.begin(), options.plugins.end(),
                             [](const LogStartSetupPlugin &item) {
                               return _wcsicmp(item.id.c_str(), NamingPlugin) == 0;
                             });
  if (plugin == options.plugins.end()) {
    MessageBoxW(owner, L"Install the bundled Scripted_Log_Names.xml plugin in "
                      L"this world and enable it, then return here. Installing "
                      L"it sends no command and opens no log.",
                L"Automatic log file names", MB_ICONINFORMATION); return false;
  }
  NamesState state;
  state.config.folder = LocalText(options.defaultFolder);
  const auto &data = options.saveNamingConfiguration ? options.namingConfiguration : plugin->namingConfiguration;
  if (!data.empty() && !DecodeLogNames(data, state.config)) {
    MessageBoxW(owner, L"Saved filename settings are malformed. They have been "
                       L"left unchanged; use the advanced script configuration "
                       L"or restore a valid plugin state.",
                L"Automatic log file names", MB_ICONWARNING); return false;
  }
  auto bytes = NamesTemplate();
  if (DialogBoxIndirectParamW(GetModuleHandleW(0), reinterpret_cast<const DLGTEMPLATE *>(bytes.data()),
                               owner, NamesProc, reinterpret_cast<LPARAM>(&state)) != IDOK)
    return false;
  options.namingConfiguration = EncodeLogNames(state.config);
  options.saveNamingConfiguration = true;
  return true;
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
      auto label = plugin.name;
      SendMessageW(target, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(label.c_str()));
      state->targets.push_back(plugin.id);
      if (_wcsicmp(plugin.id.c_str(), state->options.plugin.c_str()) == 0)
        selected = static_cast<int>(state->targets.size()) - 1;
    }
    if (!state->options.plugin.empty() && selected == 0) {
      std::wstring label = L"Unavailable plugin (saved selection)";
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
  case ConfigureNames:
    if (ConfigureFileNames(dialog, *state)) {
      CheckRadioButton(dialog, Normal, Script, Script);
      for (size_t i = 0; i < state->targets.size(); ++i)
        if (_wcsicmp(state->targets[i].c_str(), NamingPlugin) == 0)
          SendDlgItemMessageW(dialog, Target, CB_SETCURSEL, i, 0);
      SetDlgItemTextW(dialog, Function, L"StartLog");
      EnableScript(dialog);
    }
    return TRUE;
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
  auto working = options;
  SetupState state{working};
  auto result = DialogBoxIndirectParamW(
      GetModuleHandleW(0), reinterpret_cast<const DLGTEMPLATE *>(bytes.data()),
      owner, Proc, reinterpret_cast<LPARAM>(&state));
  if (result == -1)
    MessageBoxW(owner, L"Logging Setup could not be opened.", L"Logging Setup",
                  MB_OK | MB_ICONERROR);
  if (result == IDOK) options = working;
  return result == IDOK;
}
