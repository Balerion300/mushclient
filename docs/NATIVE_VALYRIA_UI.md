# Native Valyria presentation implementation

This is the Windows implementation checkpoint following the approved browser study. It runs in the existing Windows client. It does not introduce a browser engine, transport bridge or replacement game renderer.

## Architecture

The retained engine owns world documents, Telnet/TLS, scripting, triggers, plugin windows, command history and transcript records. The presentation layer owns native chrome, navigation and placement. The original Windows frame retains its system menu, maximize, snap and accessibility behavior.

`workspace/WorkspaceNativeTabs.h` supplies a real `CMFCTabCtrl` subclass through the supported `CTabbedPane::m_pTabWndRTC` factory. Its layout override establishes a minimum 40 logical pixel tab strip, enlarged when the system UI font requires more space. MFC computes the corresponding tab hit rectangles and content area. This is actual geometry, not a font-only attempt to enlarge the old tabs.

The hardcoded MFC character-document tab factory is not patched or cast to an unrelated type. A clean installation defaults to the character tree and original maximized world view. An explicit saved document-tab preference is retained; **Tools > Show character document tabs** toggles it. The native Window menu and original Notepad/editor commands remain available.

## Implemented behavior

- A native game rail and expandable game / own character / channel tree. Character/channel actions retain the exact original runtime connection identity. Selecting a game activates its first original character and scrolls to that game in the complete tree; filtering to one game is still pending.
- Persistent System/Light/Dark base appearance, neutral surfaces, red default accent and a blue alternative. High contrast defers to Windows colors. Approved original Valyria icon resources remain unchanged.
- Larger real channel tabs, pane captions, toolbar controls and status bar. The status bar retains original native parts and owner-drawn MORE feedback.
- Each channel composer shows its character, game and stream destination. Find expands only when needed; Arrange is directly visible and also available with Ctrl+Shift+M.
- Arrange offers native detach, tab reorder, join and four edge placements. It moves original pane/input/RichEdit windows through MFC docking operations, preserving drafts, transcript text and selection. Existing split-tree persistence and labeled drag destinations remain in use.
- Script editor and Agent review are directly reachable in the navigator. Logging, routing, preferences, plugins and existing tools keep their original command paths and permissions.

A channel means a filtered output stream, including Main and Pages, rather than necessarily a server-native channel. This change does not alter routing rules or send commands merely by changing navigation or placement.

## Verification

- Native appearance fixture: 34 checks, including the actual derived tab class, enlarged tab geometry/hit rectangles, red default, blue accent with unchanged neutral surfaces, retained native window behavior and original transcript font/selection.
- Native shell fixture: 31 checks, including separate own-character identities, nested channel ownership, native selection/focus retention, reentrant activation rejection, light/dark appearance and narrow toolbar wrapping.
- Native docking fixture: existing nested forest/late arrival/persistence checks plus direct Arrange joins and all four edges. Original transcript HWND, text, selection, composer HWND and unsent draft survive every operation. Same-pane placement is rejected.
- Root integration completed a production Release build before the final tree-first and status owner-draw adjustments. The final source must be rebuilt and exercised in the isolated actual client before release.

These are automated native API fixtures, not physical mixed-DPI, screen-reader or long-running live-game acceptance. The browser study is a design contract, not evidence that the Windows implementation has reached complete visual parity.

For actual-client acceptance, `tests/workspace/modern_client_fixture.py` prepares a fresh portable runtime with two fictional loopback worlds and routed Chat panes. It prints its exact candidate PID/path and retains plugin and socket receipts. It uses no installed profiles, credentials or personal preferences. The caller can stop only that fixture using its `fixture.control` file.

## Remaining Windows work

The original Main world transcript and command editor remain the original views and preserve their user font/color choices. Their composer has not yet gained the new channel composer's local destination header and surrounding layout. Main channels can already be routed into native workspace panes, but that does not replace the original world view by default. The standard menu and optional compact MDI document tabs retain Windows/MFC geometry. Rail game filtering, full Unicode MBCS tab captions, per-monitor device acceptance, keyboard splitter movement and complete prototype composition parity remain release work. Replacing these controls must retain original focus, selection, draft, plugin and routing behavior.

## Native acceptance cycle 2

The original focus-reacquisition failure was reproduced by the independent native fixture. `CWorkspaceFrame` now remembers its actual focused descendant before deactivation and restores it synchronously after MDI activation. The target must still be a visible, enabled descendant of the same frame and carry the same HWND-lifetime property. There is no posted focus change to overtake a later click or keyboard action. Both `CMainFrame` and the native fixture execute this shared path.

`tests/qa-cycle1/focus_reactivation_probe.cpp` now passes original composer restoration plus explicit focus changes, hidden/disabled targets, changed ownership, missing lifetime identity, mouse activation and immediate disconnect/reconnect status visibility. These are native API checks; the isolated real executable still needs the physical click-and-type regression.

The game rail now dispatches the exact first original character of the selected game. It does not change the title while keeping old-game actions selected. Native status painting retains owner-drawn MORE feedback and renders its size grip; the bottom-right native resize hit test passes. Main-frame large/small icons are loaded explicitly from the executable so a locale DLL cannot substitute historical MUSHclient artwork.

## Next Main-composer vertical slice

Keep each original `CSendView` HWND, its document, history, selection, IME behavior, hooks and font/color preferences. Add a native destination label as a sibling inside `CMySplitterWnd`, above the original input view. A splitter layout override can reserve its measured height while leaving `GetPane(COMMAND_PANE, 0)` bound to the original view. This avoids a replacement text buffer or a command-forwarding proxy.

The implementation must separate the presentation strip height from the existing saved command-view height, including automatic input growth, manual splitter drags and close/reopen persistence. The label must use the owning document's character/game metadata, never whichever world happens to be globally active. Any added Send button must dispatch through the original guarded human-input path and survive reentrant activation/closure checks before any command is sent. Acceptance should cover two characters, distinct unsent drafts, input hooks, aliases, IME/edit selection, auto-height, fullscreen, tabbed/tree modes and detach/rejoin of adjacent channels.

This follow-on is prepared, not implemented in this checkpoint. Complete shell parity also needs game filtering and intentional menu geometry while preserving native Window/Notepad access and accelerator behavior.

## Native acceptance cycle 3: direct frame focus

Physical testing of the cycle-2 executable still placed typed text in Main after clicking a channel. The earlier activation-only test did not cover direct `SetFocus(frame)` while the frame was already active. Adding that exact native operation reproduced the remaining failure before this fix.

`workspace/WorkspaceFocus.h` now handles both deactivation/reactivation and direct `WM_SETFOCUS` in the main frame and each `WorkspaceNativeFloat`. The latter remembers the prior actual control from `WM_SETFOCUS.wParam`, before MDI redirects focus. Each top-level host has its own bookmark; an owned detached mini-frame does not masquerade as a child of the main frame. Lifetime, ownership, visibility and enabled-state checks remain in force. Restoration stays synchronous so subsequent explicit click/Tab focus wins.

The focus fixture passes 16 checks including direct already-active frame focus, detached frame reactivation, direct detached-frame focus, explicit Main focus and redocking. This is not a substitute for the full-client physical test that revealed the gap.

For the real executable, launch `tests/workspace/modern_client_fixture.py --focus-trace --duration 3600`. This enables bounded own-process diagnostics (at most 2,048 records) in the fresh fixture directory's `valyria-focus.txt`. Entries contain only message codes, HWNDs, class names and control IDs, never typed text, character names or credentials. No external window interrogation or input injection is required. The option is disabled in ordinary launches.

Physical regression: click Beta channel input, type a distinct unsent marker, verify it stays in that field and Main remains unchanged; repeat after another application was foreground, after clicking the native frame title, with Alpha's different draft, and after detach/rejoin. Then click Main explicitly and verify typing moves there. Read the fixture trace if any step fails; it distinguishes frame activation, direct frame focus and channel edit focus notifications. Socket receipts should remain unchanged until a deliberate Send/Enter test is separately performed.

## Native acceptance cycle 4: visible detach

Root's actual-client test distinguished a UI-tool text-setting anomaly from native typing: the own-app trace retained channel Edit focus, a physical key appeared in the channel and Enter sent only the expected Beta-prefixed command to the Beta loopback receipt. The tool's bulk text-setting operation had modified Main directly. No additional focus changes were made on that basis.

The actual Arrange / Detach test exposed a separate real defect. MFC's `DetachPane(..., TRUE)` already creates a hidden floating host; a following `FloatPane(..., DM_SHOW)` returns TRUE immediately for that already-floating pane without showing or positioning it. The old regression asserted native floating identity but did not assert visibility. A new test against `CWorkspacePane::DetachToWindow`, the exact operation used by Arrange, failed before this fix.

`workspace/WorkspaceDockActions.h` now completes detachment by positioning the resulting native host within its current monitor work area, showing both pane and host, recalculating the original dock frame and invalidating the newly exposed client area. The main frame's float helper and Arrange share this operation. It keeps the original pane/RichEdit/composer HWNDs and recent native docking information.

The full docking regression now passes, including command-path visible host/pane, visible monitor bounds, exact output/selection/draft preservation and rejoining the original tab target. All 16 focus/status checks still pass. With `--focus-trace`, own-app diagnostics additionally record float return, actual host visibility and rectangle; they retain the same bounded, text-free logging policy.

Root exercised Arrange / Detach in the isolated actual Release client, verified the floating Beta channel and exposed Main-area repaint, then used Arrange / Join tabs to restore Alpha and Beta to the same native tab group. The original transcript and inline image remained visible. Mixed-DPI device testing remains outstanding.

## Native acceptance cycle 5: Debug startup assertion

Root's rebuilt Release executable passed actual visible Detach and Main-area repaint. The Debug executable failed before opening its fictional world. A small own-process Windows debugger launched a copied fictional runtime and captured the actual diagnostic: MFC `winctrl1.cpp:187`, the default `CListBox::MeasureItem` assertion. The owner-drawn game rail had supplied drawing but omitted measurement. Release did not report this programming error; the Debug assertion path subsequently faulted during callback handling.

Navigator now handles `WM_MEASUREITEM` for the game rail and supplies its 52 logical pixel row height. No production assertion was disabled or ignored. The shell probe explicitly verifies this native measurement message. Its runner accepts `-Configuration Debug`, links the real Debug CRT/MFC libraries and embeds required MFC resources. In that fixture only, an assertion writes `shell-debug-assert.txt` and exits with failure code 91 instead of waiting for a modal interaction.

Both Debug and Release shell fixtures pass all 32 checks. Root rebuilt the production Debug executable and reran the original scripted-logging `plugin-hook` case successfully. Its isolated fixture confirmed installation, connection, guarded recursive start, exclusive-file checks and log closure. The final Release executable also rebuilt successfully.

## Integration performance checkpoint

The native long-transcript docking API fixture retained 1,800 records and eight images across 24 operations: median 124 ms, 95th percentile 424 ms, maximum 435 ms on this test machine. Selection, links, image objects and viewport anchors passed. These are native API operations, not a claim about physical dragging on every device. The separate output component fixture passed plain, styled, image and batch cases; image-update 95th percentile was approximately 3.8 ms. Real-device sustained-use, mixed-DPI, screen-reader and complete prototype composition acceptance remain open.
