#define NOMINMAX
#include "../../workspace/LogStartSetup.cpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <thread>
// Synthetic native form only: no client document, script engine or transport.
HWND WaitDialog(const wchar_t *title, HWND owner) {
  for (int i = 0; i < 1000; ++i) {
    HWND dialog = FindWindowW(L"#32770", title);
    if (dialog && GetWindow(dialog, GW_OWNER) == owner && GetDlgItem(dialog, IDOK)) {
      Sleep(30); return dialog;
    }
    Sleep(5);
  }
  assert(!"Fixture dialog not found"); return 0;
}
void Click(HWND dialog, int id) {
  HWND button = GetDlgItem(dialog, id);
  assert(button); PostMessageW(button, BM_CLICK, 0, 0);
}
void CaptionFits(HWND button) {
  wchar_t text[256]={};GetWindowTextW(button,text,256);std::wstring label=text;
  label.erase(std::remove(label.begin(),label.end(),L'&'),label.end());
  HDC dc=GetDC(button);auto old=SelectObject(dc,(HFONT)SendMessageW(button,WM_GETFONT,0,0));
  SIZE extent={};GetTextExtentPoint32W(dc,label.data(),(int)label.size(),&extent);
  RECT bounds;GetClientRect(button,&bounds);
  assert(extent.cx+workspace_style::Scale(button,44)<=bounds.right);
  SelectObject(dc,old);ReleaseDC(button,dc);
}
bool InViewport(HWND dialog,HWND child) {
  RECT r,client;GetWindowRect(child,&r);MapWindowPoints(0,dialog,(POINT*)&r,2);GetClientRect(dialog,&client);
  return r.left>=0 && r.top>=0 && r.right<=client.right && r.bottom<=client.bottom;
}
void Snapshot(HWND dialog, const std::wstring &path) {
  RECT rect; GetWindowRect(dialog, &rect);
  int width = rect.right - rect.left, height = rect.bottom - rect.top;
  HDC screen = GetDC(dialog), memory = CreateCompatibleDC(screen);
  HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
  auto old = SelectObject(memory, bitmap);
  PrintWindow(dialog, memory, 0);
  SelectObject(memory, old);
  BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
  std::vector<unsigned char> pixels(width * height * 4);
  assert(GetDIBits(memory, bitmap, 0, height, pixels.data(), &info, DIB_RGB_COLORS));
  BITMAPFILEHEADER header = {}; header.bfType = 0x4d42;
  header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
  header.bfSize = header.bfOffBits + static_cast<DWORD>(pixels.size());
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<char *>(&header), sizeof(header));
  output.write(reinterpret_cast<char *>(&info.bmiHeader), sizeof(info.bmiHeader));
  output.write(reinterpret_cast<char *>(pixels.data()), pixels.size());
  assert(output.good());
  DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(dialog, screen);
}
int wmain(int argc, wchar_t **argv) {
  _set_error_mode(_OUT_TO_STDERR); _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  assert(argc == 2);
  HWND owner = CreateWindowW(L"STATIC", L"Fictional logging fixture", WS_OVERLAPPED,
                              0, 0, 100, 100, 0, 0, GetModuleHandleW(0), 0);
  assert(owner);
  LogStartSetupOptions options;
  options.worldName = L"Fictional World"; options.defaultFolder = argv[1];
  options.plugins.push_back({L"Scripted Log Names", NamingPlugin, ""});
  auto edit = std::thread([&] {
    auto setup = WaitDialog(L"Logging Setup", owner);
    assert(!IsWindowVisible(GetDlgItem(setup,Function)));
    CaptionFits(GetDlgItem(setup,IDOK));CaptionFits(GetDlgItem(setup,IDCANCEL));
    Snapshot(setup,std::wstring(argv[1])+L"\\logging-basic.bmp");
    Click(setup, ConfigureNames);
    auto form = WaitDialog(L"Automatic log file names", setup);
    assert(!IsWindowVisible(GetDlgItem(form,Request)));
    Click(form,AdvancedNames);
    for(int i=0;i<1000 && !IsWindowVisible(GetDlgItem(form,Request));++i)Sleep(1);
    assert(IsWindowVisible(GetDlgItem(form,Request)));
    CaptionFits(GetDlgItem(form,IDOK));CaptionFits(GetDlgItem(form,IDCANCEL));
    RECT full;GetWindowRect(form,&full);
    SetWindowPos(form,0,0,0,480,360,SWP_NOMOVE|SWP_NOZORDER);
    SendMessageW(form,WM_NEXTDLGCTL,(WPARAM)GetDlgItem(form,IDOK),TRUE);
    for(int i=0;i<1000 && !InViewport(form,GetDlgItem(form,IDOK));++i)Sleep(1);
    assert(InViewport(form,GetDlgItem(form,IDOK)));
    Snapshot(form,std::wstring(argv[1])+L"\\logging-narrow.bmp");
    SetWindowPos(form,0,0,0,full.right-full.left,full.bottom-full.top,SWP_NOMOVE|SWP_NOZORDER);
    SetDlgItemTextW(form, Filename, L"{game_year}-{number}.txt");
    SetDlgItemTextW(form, Request, L"fictional-date");
    SetDlgItemTextW(form, StartReply, L"^BEGIN$");
    SetDlgItemTextW(form, EndReply, L"^END$");
    SetDlgItemTextW(form, FieldName, L"game_year");
    SetDlgItemTextW(form, FieldRegex, L"^Year: (\\d+)$");
    SetDlgItemTextW(form, Capture, L"1");
    SendDlgItemMessageW(form, Transform, CB_SETCURSEL, 1, 0);
    CheckDlgButton(form, Header, BST_CHECKED);
    SendMessageW(form, WM_COMMAND, AddField, 0);
    SetDlgItemTextW(form, FieldName, L"scene");
    SetDlgItemTextW(form, FieldRegex, L"^Scene: (.*)$");
    CheckDlgButton(form, Required, BST_UNCHECKED);
    SendMessageW(form, WM_COMMAND, AddField, 0);
    SendDlgItemMessageW(form, Fields, CB_SETCURSEL, 0, 0);
    SendMessageW(form, WM_COMMAND, MAKEWPARAM(Fields, CBN_SELCHANGE), 0);
    SetDlgItemTextW(form, FieldRegex, L"^Year:\\s*(\\d+)$");
    SendDlgItemMessageW(form, Fields, CB_SETCURSEL, 1, 0);
    SendMessageW(form, WM_COMMAND, MAKEWPARAM(Fields, CBN_SELCHANGE), 0);
    assert(GetText(GetDlgItem(form, FieldName)) == L"scene");
    SendDlgItemMessageW(form, Fields, CB_SETCURSEL, 0, 0);
    SendMessageW(form, WM_COMMAND, MAKEWPARAM(Fields, CBN_SELCHANGE), 0);
    assert(GetText(GetDlgItem(form, FieldRegex)) == L"^Year:\\s*(\\d+)$");
    UpdateWindow(form);
    Snapshot(form, std::wstring(argv[1]) + L"\\names-ui.bmp");
    Click(form, IDOK);
    for (int i = 0; i < 1000 && IsWindow(form); ++i) Sleep(5);
    assert(!IsWindow(form));
    assert(IsDlgButtonChecked(setup, Script) == BST_CHECKED);
    assert(GetText(GetDlgItem(setup, Function)) == L"StartLog");
    Click(setup, IDOK);
  });
  assert(ShowLogStartSetup(owner, options)); edit.join();
  assert(options.saveNamingConfiguration && options.script == L"StartLog" && options.plugin == NamingPlugin);
  LogNameConfiguration config;
  assert(DecodeLogNames(options.namingConfiguration, config));
  assert(config.folder == LocalText(argv[1]) && config.filename == "{game_year}-{number}.txt" &&
         config.request == "fictional-date" && config.fields.size() == 2 &&
         config.fields[0].name == "game_year" && config.fields[0].transform == "number" &&
         config.fields[0].regex == "^Year:\\s*(\\d+)$" && config.fields[0].required &&
         config.fields[1].name == "scene" && !config.fields[1].required && config.header);
  auto data = options.namingConfiguration;
  assert(EncodeLogNames(config) == data);
  std::string persisted;
  for (char ch : data) { if (ch == '\n') persisted += '\r'; persisted += ch; }
  assert(DecodeLogNames(persisted, config) && EncodeLogNames(config) == data);
  assert(!DecodeLogNames(data + "trailing", config));
  assert(!DecodeLogNames("SLN1\nzz\n", config));
  // A rejected host save must be able to reopen the submitted options rather
  // than reverting to the plugin's previous state and losing entered rules.
  options.plugins[0].namingConfiguration = data;
  edit = std::thread([&] {
    auto setup = WaitDialog(L"Logging Setup", owner);
    Click(setup, ConfigureNames);
    auto form = WaitDialog(L"Automatic log file names", setup);
    SendDlgItemMessageW(form, Fields, CB_SETCURSEL, 0, 0);
    SendMessageW(form, WM_COMMAND, MAKEWPARAM(Fields, CBN_SELCHANGE), 0);
    SetDlgItemTextW(form, FieldRegex, L"[");
    Click(form, IDOK);
    for (int i = 0; i < 1000 && IsWindow(form); ++i) Sleep(5);
    assert(!IsWindow(form)); Click(setup, IDOK);
  });
  assert(ShowLogStartSetup(owner, options)); edit.join();
  assert(DecodeLogNames(options.namingConfiguration, config) && config.fields[0].regex == "[");
  edit = std::thread([&] {
    auto setup = WaitDialog(L"Logging Setup", owner);
    Click(setup, ConfigureNames);
    auto form = WaitDialog(L"Automatic log file names", setup);
    SendDlgItemMessageW(form, Fields, CB_SETCURSEL, 0, 0);
    SendMessageW(form, WM_COMMAND, MAKEWPARAM(Fields, CBN_SELCHANGE), 0);
    assert(GetText(GetDlgItem(form, FieldRegex)) == L"[");
    assert(GetText(GetDlgItem(form, Request)) == L"fictional-date");
    SetDlgItemTextW(form, FieldRegex, L"^Year:\\s*(\\d+)$");
    Click(form, IDOK);
    for (int i = 0; i < 1000 && IsWindow(form); ++i) Sleep(5);
    assert(!IsWindow(form)); Click(setup, IDOK);
  });
  assert(ShowLogStartSetup(owner, options)); edit.join();
  assert(options.namingConfiguration == data);
  edit = std::thread([&] {
    auto setup = WaitDialog(L"Logging Setup", owner);
    Click(setup, ConfigureNames);
    auto form = WaitDialog(L"Automatic log file names", setup);
    SetDlgItemTextW(form, Filename, L"fictional-modified-but-cancelled-{number}.txt");
    Click(form, IDOK);
    for (int i = 0; i < 1000 && IsWindow(form); ++i) Sleep(5);
    assert(!IsWindow(form));
    Click(setup, IDCANCEL);
  });
  assert(!ShowLogStartSetup(owner, options)); edit.join();
  assert(options.script == L"StartLog" && options.namingConfiguration == data);
  DestroyWindow(owner);
  std::cout << "PASS native automatic-names form, captured field, explicit opt-in, LF/CRLF round-trip, malformed data, rejected edits retained, outer Cancel\n";
  return 0;
}
