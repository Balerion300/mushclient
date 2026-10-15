#include "stdafx.h"
#include "WorkspaceStyle.h"
#include <commctrl.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#ifndef WM_DPICHANGED_AFTERPARENT
#define WM_DPICHANGED_AFTERPARENT 0x02E3
#endif

namespace workspace_style {
namespace {
Theme selectedTheme = Theme::System;
AppearanceChangedHandler appearanceChanged = NULL;
bool paletteValid = false;
Palette cachedPalette;
struct Chrome {
  bool surface = false, button = false, primary = false, hover = false;
  Icon icon = Icon::None;
  HWND tooltip = NULL;
  std::wstring tooltipText;
};
std::map<HWND, std::unique_ptr<Chrome> > chrome;
std::map<UINT, HFONT> fonts;
std::vector<HFONT> retiredFonts;
LOGFONTW messageFont = {};
bool messageFontKnown = false;
std::map<COLORREF, HBRUSH> brushes;
void RefreshFontSettings() {
  NONCLIENTMETRICSW metrics = {sizeof(metrics)};
  if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0)) return;
  if (messageFontKnown && memcmp(&messageFont,&metrics.lfMessageFont,sizeof(messageFont))) {
    // Enrolled controls and callers borrow these handles. Keep old generations
    // alive until process exit rather than deleting a font still selected in a DC.
    for (const auto& font : fonts) retiredFonts.push_back(font.second);
    fonts.clear();
  }
  messageFont = metrics.lfMessageFont; messageFontKnown = true;
}
HBRUSH Brush(COLORREF color) {
  auto found = brushes.find(color);
  if (found != brushes.end()) return found->second;
  HBRUSH brush = CreateSolidBrush(color);
  if (brush) brushes[color] = brush;
  return brush;
}
bool HighContrast() {
  HIGHCONTRAST info = {sizeof(info)};
  return SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(info), &info, 0) &&
         (info.dwFlags & HCF_HIGHCONTRASTON);
}
bool SystemDark() {
  HKEY key = NULL; DWORD value = 1, size = sizeof(value), type = 0;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
  const LONG status = RegQueryValueExW(key, L"AppsUseLightTheme", NULL, &type,
                                     reinterpret_cast<BYTE*>(&value), &size);
  RegCloseKey(key);
  return status == ERROR_SUCCESS && type == REG_DWORD && !value;
}
void NativeTheme(HWND window) {
  // Optional native control theme support; Windows 7 remains a valid fallback.
  typedef HRESULT (WINAPI *SetThemeFn)(HWND, LPCWSTR, LPCWSTR);
  static HMODULE module = LoadLibraryW(L"uxtheme.dll");
  static SetThemeFn setTheme = module ? reinterpret_cast<SetThemeFn>(GetProcAddress(module, "SetWindowTheme")) : NULL;
  if (setTheme) setTheme(window, Colors(window).highContrast ? L"" : L"Explorer", NULL);
}
bool PushButton(HWND window) {
  wchar_t kind[32] = {};
  GetClassNameW(window, kind, 32);
  const DWORD type = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)) & BS_TYPEMASK;
  return !_wcsicmp(kind, L"Button") &&
         (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON || type == BS_OWNERDRAW);
}
void PaintButton(HWND window, HDC dc, Chrome& state) {
  RECT bounds; GetClientRect(window, &bounds);
  const Palette p = Colors(window);
  const bool enabled = IsWindowEnabled(window) != FALSE;
  const LRESULT status = SendMessageW(window, BM_GETSTATE, 0, 0);
  const bool pressed = (status & BST_PUSHED) != 0;
  COLORREF background = !enabled ? p.surface : state.primary ? p.accent :
                        pressed ? p.pressed : state.hover ? p.hover : p.surface;
  if (state.primary && enabled && state.hover && !pressed && !p.highContrast)
    background = RGB((GetRValue(p.accent)*4+GetRValue(p.text))/5,
                     (GetGValue(p.accent)*4+GetGValue(p.text))/5,
                     (GetBValue(p.accent)*4+GetBValue(p.text))/5);
  if (state.primary && enabled && pressed) background = p.pressed;
  const COLORREF foreground = !enabled ? p.disabled : p.highContrast && pressed ? p.accentText :
      state.primary && !pressed ? p.accentText : p.text;
  FillRect(dc, &bounds, Brush(background));
  FrameRect(dc, &bounds, Brush(state.primary && enabled ? p.accent : p.border));
  const int padding = Scale(window, 10), gap = Scale(window, 7);
  const int iconSize = Scale(window, 16);
  const int length = GetWindowTextLengthW(window);
  std::wstring text(min(length, 1024) + 1, L'\0');
  GetWindowTextW(window, &text[0], static_cast<int>(text.size()));
  text.resize(wcslen(text.c_str()));
  HFONT oldFont = static_cast<HFONT>(SelectObject(dc, UiFont(window)));
  SetBkMode(dc, TRANSPARENT); SetTextColor(dc, foreground);
  UINT flags = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
  if (SendMessageW(window, WM_QUERYUISTATE, 0, 0) & UISF_HIDEACCEL) flags |= DT_HIDEPREFIX;
  RECT label = bounds; label.left += padding; label.right -= padding;
  if (state.icon != Icon::None) {
    RECT icon = {label.left, (bounds.bottom - iconSize) / 2,
                 label.left + iconSize, (bounds.bottom + iconSize) / 2};
    DrawIcon(dc, icon, state.icon, foreground); label.left += iconSize + gap;
  } else flags |= DT_CENTER;
  DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &label, flags);
  if (GetFocus() == window) {
    RECT focus = bounds; InflateRect(&focus, -Scale(window, 3), -Scale(window, 3));
    SetTextColor(dc, foreground); SetBkColor(dc, background); DrawFocusRect(dc, &focus);
  }
  SelectObject(dc, oldFont);
}
LRESULT CALLBACK ChromeProc(HWND window, UINT message, WPARAM wp, LPARAM lp,
                            UINT_PTR subclass, DWORD_PTR data) {
  Chrome* state = reinterpret_cast<Chrome*>(data);
  switch (message) {
  case WM_ERASEBKGND:
    if (state->surface || state->button) {
      RECT bounds; GetClientRect(window, &bounds);
      FillRect(reinterpret_cast<HDC>(wp), &bounds, Brush(Colors(window).background)); return 1;
    }
    break;
  case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
  case WM_CTLCOLORBTN: {
    HWND child = reinterpret_cast<HWND>(lp);
    if (chrome.find(child) == chrome.end()) break;
    const Palette p = Colors(window);
    const bool field = message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX;
    const COLORREF background = field ? p.surface : p.background;
    SetTextColor(reinterpret_cast<HDC>(wp), IsWindowEnabled(child) ? p.text : p.disabled);
    SetBkColor(reinterpret_cast<HDC>(wp), background);
    return reinterpret_cast<LRESULT>(Brush(background));
  }
  case WM_PAINT:
    if (state->button) {PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint); PaintButton(window, dc, *state); EndPaint(window, &paint); return 0;}
    break;
  case WM_PRINTCLIENT:
    if (state->button) {PaintButton(window, reinterpret_cast<HDC>(wp), *state); return 0;}
    break;
  case WM_MOUSEMOVE:
    if (state->button && !state->hover) {state->hover = true; TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, window, 0}; TrackMouseEvent(&track); InvalidateRect(window, NULL, FALSE);}
    break;
  case WM_MOUSELEAVE:
    state->hover = false; InvalidateRect(window, NULL, FALSE); break;
  case WM_THEMECHANGED: case WM_SYSCOLORCHANGE: case WM_SETTINGCHANGE:
  case WM_DPICHANGED_AFTERPARENT:
    paletteValid = false;
    if (message == WM_SETTINGCHANGE) RefreshFontSettings();
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(UiFont(window)), FALSE);
    if (appearanceChanged) appearanceChanged(window);
    InvalidateRect(window, NULL, TRUE); break;
  case WM_NCDESTROY: {
    HWND tooltip = state->tooltip;
    RemoveWindowSubclass(window, ChromeProc, subclass);
    const LRESULT result = DefSubclassProc(window, message, wp, lp);
    if (tooltip && IsWindow(tooltip)) DestroyWindow(tooltip);
    chrome.erase(window); return result;
  }
  }
  const bool button = state->button;
  const LRESULT result = DefSubclassProc(window, message, wp, lp);
  if (button && IsWindow(window) && (message == WM_SETFOCUS || message == WM_KILLFOCUS ||
      message == WM_ENABLE || message == BM_SETSTATE || message == WM_LBUTTONDOWN ||
      message == WM_LBUTTONUP || message == WM_KEYDOWN || message == WM_KEYUP ||
      message == WM_SETTEXT || message == WM_UPDATEUISTATE)) InvalidateRect(window, NULL, FALSE);
  return result;
}
Chrome* Enroll(HWND window) {
  if (!IsWindow(window)) return NULL;
  auto found = chrome.find(window);
  if (found != chrome.end()) return found->second.get();
  std::unique_ptr<Chrome> state(new Chrome);
  if (!SetWindowSubclass(window, ChromeProc, reinterpret_cast<UINT_PTR>(&ChromeProc),
                         reinterpret_cast<DWORD_PTR>(state.get()))) return NULL;
  Chrome* result = state.get(); chrome[window] = std::move(state); return result;
}
} // namespace

void SetTheme(Theme theme) {
  if (theme != Theme::System && theme != Theme::Light && theme != Theme::Dark) theme = Theme::System;
  selectedTheme = theme;
  paletteValid = false;
  std::vector<HWND> windows; for (const auto& entry : chrome) windows.push_back(entry.first);
  for (HWND window : windows) if (IsWindow(window)) {NativeTheme(window); SendMessageW(window, WM_THEMECHANGED, 0, 0);}
  if (appearanceChanged) appearanceChanged(NULL);
}
void SetAppearanceChangedHandler(AppearanceChangedHandler handler) {appearanceChanged = handler;}
Theme GetTheme() {return selectedTheme;}
Palette Colors(HWND) {
  if (paletteValid) return cachedPalette;
  paletteValid = true;
  if (HighContrast()) return cachedPalette = {GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOW),
    GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOWTEXT),
    GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_HIGHLIGHTTEXT), GetSysColor(COLOR_WINDOW),
    GetSysColor(COLOR_HIGHLIGHT), GetSysColor(COLOR_GRAYTEXT), true};
  const bool dark = selectedTheme == Theme::Dark || (selectedTheme == Theme::System && SystemDark());
  if (dark) return cachedPalette = {RGB(27,29,33),RGB(35,38,43),RGB(237,239,242),RGB(176,183,193),RGB(83,91,103),RGB(114,180,255),RGB(15,25,38),RGB(47,53,62),RGB(65,76,91),RGB(145,153,165),false};
  return cachedPalette = {RGB(245,246,248),RGB(255,255,255),RGB(29,35,44),RGB(86,96,110),RGB(163,173,187),RGB(20,93,178),RGB(255,255,255),RGB(231,238,247),RGB(207,222,243),RGB(113,123,137),false};
}
UINT Dpi(HWND window) {
  typedef UINT (WINAPI *GetDpiFn)(HWND);
  static GetDpiFn getDpi = reinterpret_cast<GetDpiFn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
  if (window && getDpi) {UINT value = getDpi(window); if (value) return value;}
  HDC dc = GetDC(window); const int value = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
  if (dc) ReleaseDC(window, dc); return value > 0 ? value : 96;
}
int Scale(HWND window, int value) {return MulDiv(value, Dpi(window), 96);}
HFONT UiFont(HWND window) {
  if (!messageFontKnown) RefreshFontSettings();
  const UINT dpi = Dpi(window); auto found = fonts.find(dpi); if (found != fonts.end()) return found->second;
  NONCLIENTMETRICSW metrics = {sizeof(metrics)};
  typedef BOOL (WINAPI *MetricsFn)(UINT,UINT,PVOID,UINT,UINT);
  static MetricsFn getMetrics = reinterpret_cast<MetricsFn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi"));
  bool perDpi = getMetrics && getMetrics(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,dpi);
  if (!perDpi && !SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0)) return static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  if (!perDpi) metrics.lfMessageFont.lfHeight = MulDiv(metrics.lfMessageFont.lfHeight,dpi,Dpi(NULL));
  HFONT font = CreateFontIndirectW(&metrics.lfMessageFont);
  if (font) fonts[dpi] = font; return font ? font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
}
bool ReducedMotion() {BOOL enabled = TRUE; return !SystemParametersInfo(SPI_GETCLIENTAREAANIMATION,0,&enabled,0) || !enabled;}
void ApplySurface(HWND window) {Chrome* state = Enroll(window); if (!state) return; state->surface = true; NativeTheme(window); if (appearanceChanged) appearanceChanged(window); InvalidateRect(window,NULL,TRUE);}
void StyleControl(HWND window) {if (!Enroll(window)) return; SendMessageW(window,WM_SETFONT,reinterpret_cast<WPARAM>(UiFont(window)),TRUE); NativeTheme(window);}
void StyleButton(HWND window, Icon icon, bool primary, const wchar_t* tooltip) {
  StyleControl(window); Chrome* state = Enroll(window); if (!state || !PushButton(window)) return;
  state->button = true; state->icon = icon; state->primary = primary;
  if (state->tooltip) {DestroyWindow(state->tooltip); state->tooltip = NULL;}
  state->tooltipText = tooltip ? tooltip : L"";
  if (!state->tooltipText.empty()) {
    state->tooltip = CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,NULL,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,window,NULL,GetModuleHandleW(NULL),NULL);
    if (state->tooltip) {TOOLINFOW info = {sizeof(info)};info.uFlags=TTF_IDISHWND|TTF_SUBCLASS;info.hwnd=window;info.uId=reinterpret_cast<UINT_PTR>(window);info.lpszText=&state->tooltipText[0];SendMessageW(state->tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&info));SendMessageW(state->tooltip,TTM_SETMAXTIPWIDTH,0,Scale(window,320));}
  }
  InvalidateRect(window,NULL,TRUE);
}
void FitToWorkArea(HWND window,int minimumWidth,int minimumHeight) {
  if (!IsWindow(window)) return; MONITORINFO monitor={sizeof(monitor)};
  if (!GetMonitorInfo(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) return;
  RECT bounds;GetWindowRect(window,&bounds);const RECT& work=monitor.rcWork;
  const int width=min(work.right-work.left,max(Scale(window,minimumWidth),bounds.right-bounds.left));
  const int height=min(work.bottom-work.top,max(Scale(window,minimumHeight),bounds.bottom-bounds.top));
  const int x=max(work.left,min(bounds.left,work.right-width)),y=max(work.top,min(bounds.top,work.bottom-height));
  SetWindowPos(window,NULL,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
}

void DrawIcon(HDC dc,const RECT& bounds,Icon icon,COLORREF color) {
  if (icon == Icon::None) return;
  const int size=min(bounds.right-bounds.left,bounds.bottom-bounds.top);
  auto x=[&](int value){return bounds.left+MulDiv(value,size,20);};
  auto y=[&](int value){return bounds.top+MulDiv(value,size,20);};
  HPEN pen=CreatePen(PS_SOLID,max(1,MulDiv(15,size,200)),color);HPEN oldPen=static_cast<HPEN>(SelectObject(dc,pen));HGDIOBJ oldBrush=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
  auto line=[&](int a,int b,int c,int d){MoveToEx(dc,x(a),y(b),NULL);LineTo(dc,x(c),y(d));};
  auto box=[&](int a,int b,int c,int d){Rectangle(dc,x(a),y(b),x(c),y(d));};
  switch(icon) {
  case Icon::Worlds: Ellipse(dc,x(2),y(2),x(18),y(18));Ellipse(dc,x(7),y(2),x(13),y(18));line(2,10,18,10);break;
  case Icon::Connect: line(4,3,4,9);line(10,3,10,9);box(2,8,12,13);line(7,13,7,18);line(13,5,18,5);line(16,3,18,5);line(16,7,18,5);break;
  case Icon::Disconnect: line(4,3,4,8);line(10,3,10,8);box(2,7,12,12);line(7,12,7,17);line(14,12,19,17);line(19,12,14,17);break;
  case Icon::Find: Ellipse(dc,x(2),y(2),x(13),y(13));line(12,12,18,18);break;
  case Icon::Logging: case Icon::Editor: box(4,2,16,18);line(7,6,13,6);line(7,10,13,10);line(7,14,12,14);break;
  case Icon::Channels: box(2,3,18,14);line(5,14,5,18);line(5,18,10,14);line(6,7,14,7);line(6,10,12,10);break;
  case Icon::Tools: line(3,16,13,6);Ellipse(dc,x(11),y(2),x(18),y(9));box(2,14,6,18);break;
  case Icon::Agent: box(3,5,17,16);line(10,2,10,5);line(7,9,7,11);line(13,9,13,11);line(7,13,13,13);break;
  case Icon::Scene: box(2,5,18,17);line(2,9,18,9);line(4,2,16,2);line(5,2,8,5);line(11,2,14,5);break;
  case Icon::Settings: Ellipse(dc,x(5),y(5),x(15),y(15));Ellipse(dc,x(8),y(8),x(12),y(12));line(10,1,10,5);line(10,15,10,19);line(1,10,5,10);line(15,10,19,10);line(3,3,6,6);line(14,14,17,17);line(3,17,6,14);line(14,6,17,3);break;
  case Icon::Add:line(3,10,17,10);line(10,3,10,17);break;
  case Icon::Remove:line(3,10,17,10);break;
  case Icon::Save:box(3,2,17,18);box(6,2,13,7);box(6,11,14,18);break;
  case Icon::Close:line(4,4,16,16);line(16,4,4,16);break;
  case Icon::ChevronDown:line(4,7,10,13);line(10,13,16,7);break;
  case Icon::ChevronRight:line(7,4,13,10);line(13,10,7,16);break;
  case Icon::Send:line(2,3,18,10);line(18,10,2,17);line(2,17,6,10);line(6,10,2,3);line(6,10,18,10);break;
  case Icon::Refresh:Arc(dc,x(3),y(3),x(17),y(17),x(17),y(8),x(14),y(15));line(17,3,17,8);line(12,8,17,8);break;
  case Icon::Pause:box(4,3,8,17);box(12,3,16,17);break;
  default:break;
  }
  SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);
}
} // namespace workspace_style
