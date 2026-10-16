#pragma once
#include <windows.h>

// Original native Windows chrome. Does not replace nonclient hit testing,
// docking, window commands, tab notification/keyboard handling, or content.
namespace workspace_appearance {
// Call once before constructing enhanced MFC docking windows.
void Install();
// Repaint native docking chrome and update the supported DWM title attributes.
// A NULL root refreshes the current UI thread's top-level windows.
void Refresh(HWND root = NULL);
// TRUE when a documented DWM color attribute is accepted. Unsupported OS/DWM
// versions retain their native title bars; there are no private ordinal calls.
bool ApplyNativeFrame(HWND window);
// Explicit opt-in for ordinary single-row horizontal native tabs only. Returns
// false for custom/owner-drawn, vertical, button or multiline tab controls.
// Keeps the native item rectangles, messages, focus and accessibility provider.
bool StyleTabs(HWND tabs);
bool StyleStatusBar(HWND status);
}
