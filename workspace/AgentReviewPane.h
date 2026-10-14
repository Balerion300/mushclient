#pragma once
#include <windows.h>
// Native, resizable human review surface. Call on UI thread. Agent access and
// output grants are established separately; this pane never establishes them.
HWND OpenAgentReviewPane(HWND parent);
