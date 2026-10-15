#pragma once
#include <windows.h>

// Chrome only. Game transcripts, Scintilla and plugin miniwindows keep their
// own fonts and palettes. Returned fonts/brushes remain owned by this module.
namespace workspace_style {
enum class Theme { System, Light, Dark };
struct Palette {
  COLORREF background, surface, text, muted, border, accent, accentText;
  COLORREF hover, pressed, disabled;
  bool highContrast;
};
enum class Icon {
  None, Worlds, Connect, Disconnect, Find, Logging, Channels, Tools, Editor,
  Agent, Scene, Settings, Add, Remove, Save, Close, ChevronDown, ChevronRight,
  Send, Refresh, Pause
};
void SetTheme(Theme theme);
// Optional native-frame integration. Standalone style users need no MFC link.
// NULL denotes a palette change; a window denotes a newly enrolled surface.
typedef void (*AppearanceChangedHandler)(HWND);
void SetAppearanceChangedHandler(AppearanceChangedHandler handler);
Theme GetTheme();
Palette Colors(HWND window = NULL);
UINT Dpi(HWND window);
int Scale(HWND window, int pixelsAt96Dpi);
HFONT UiFont(HWND window);
bool ReducedMotion();
void ApplySurface(HWND window);
void StyleControl(HWND window);
void StyleButton(HWND window, Icon icon = Icon::None, bool primary = false,
                 const wchar_t* tooltip = NULL);
void DrawIcon(HDC dc, const RECT& bounds, Icon icon, COLORREF color);
// Fits a tool's outer bounds into its current monitor work area, preserving
// position where possible. Call after calculating the tool's desired DPI size.
void FitToWorkArea(HWND window, int minimumWidthAt96 = 280,
                   int minimumHeightAt96 = 180);
} // namespace workspace_style
