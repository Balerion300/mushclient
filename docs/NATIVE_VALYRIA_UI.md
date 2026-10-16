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
- Original Main composers now add a native owning-character destination strip and Send button while retaining their original edit HWND, fonts and command path. Each channel composer shows its character, game and stream destination. Find expands only when needed; Arrange is directly visible and also available with Ctrl+Shift+M.
- Arrange offers native detach, tab reorder, join and four edge placements. It moves original pane/input/RichEdit windows through MFC docking operations, preserving drafts, transcript text and selection. Existing split-tree persistence and labeled drag destinations remain in use.
- Script editor and Agent review are directly reachable in the navigator. Logging, routing, preferences, plugins and existing tools keep their original command paths and permissions.

A channel means a filtered output stream, including Main and Pages, rather than necessarily a server-native channel. This change does not alter routing rules or send commands merely by changing navigation or placement.

## Verification

- Native appearance fixture: 34 checks, including the actual derived tab class, enlarged tab geometry/hit rectangles, red default, blue accent with unchanged neutral surfaces, retained native window behavior and original transcript font/selection.
- Native shell fixture: 32 checks, including separate own-character identities, nested channel ownership, native selection/focus retention, reentrant activation rejection, light/dark appearance and narrow toolbar wrapping.
- Native docking fixture: existing nested forest/late arrival/persistence checks plus direct Arrange joins and all four edges. Original transcript HWND, text, selection, composer HWND and unsent draft survive every operation. Same-pane placement is rejected.
- The prior native checkpoint completed both production Release and Debug builds and isolated actual-client logging/detach acceptance. The current Main-composer slice has separate verification below; prior builds do not validate a later source change.

These are automated native API fixtures, not physical mixed-DPI, screen-reader or long-running live-game acceptance. The browser study is a design contract, not evidence that the Windows implementation has reached complete visual parity.

For actual-client acceptance, `tests/workspace/modern_client_fixture.py` prepares a fresh portable runtime with two fictional loopback worlds and routed Chat panes. It prints its exact candidate PID/path and retains plugin and socket receipts. It uses no installed profiles, credentials or personal preferences. The caller can stop only that fixture using its `fixture.control` file.

## Remaining Windows work

The original Main world transcript and command editor remain the original views and preserve their user font/color choices. Their Main composer now has a native destination strip and Send button around the original editor; details and acceptance limits are below. Main channels can already be routed into native workspace panes, but that does not replace the original world view by default. The standard menu and optional compact MDI document tabs retain Windows/MFC geometry. Rail game filtering, full Unicode MBCS tab captions, per-monitor device acceptance, keyboard splitter movement and complete prototype composition parity remain release work. Replacing these controls must retain original focus, selection, draft, plugin and routing behavior.

## Native acceptance cycle 2

The original focus-reacquisition failure was reproduced by the independent native fixture. `CWorkspaceFrame` now remembers its actual focused descendant before deactivation and restores it synchronously after MDI activation. The target must still be a visible, enabled descendant of the same frame and carry the same HWND-lifetime property. There is no posted focus change to overtake a later click or keyboard action. Both `CMainFrame` and the native fixture execute this shared path.

`tests/qa-cycle1/focus_reactivation_probe.cpp` now passes original composer restoration plus explicit focus changes, hidden/disabled targets, changed ownership, missing lifetime identity, mouse activation and immediate disconnect/reconnect status visibility. These are native API checks; the isolated real executable still needs the physical click-and-type regression.

The game rail now dispatches the exact first original character of the selected game. It does not change the title while keeping old-game actions selected. Native status painting retains owner-drawn MORE feedback and renders its size grip; the bottom-right native resize hit test passes. Main-frame large/small icons are loaded explicitly from the executable so a locale DLL cannot substitute historical MUSHclient artwork.

## Main-composer implementation contract

Keep each original `CSendView` HWND, its document, history, selection, IME behavior, hooks and font/color preferences. Add a native destination label as a sibling inside `CMySplitterWnd`, above the original input view. A splitter layout override can reserve its measured height while leaving `GetPane(COMMAND_PANE, 0)` bound to the original view. This avoids a replacement text buffer or a command-forwarding proxy.

The implementation must separate the presentation strip height from the existing saved command-view height, including automatic input growth, manual splitter drags and close/reopen persistence. The label must use the owning document's character/game metadata, never whichever world happens to be globally active. Any added Send button must dispatch through the original guarded human-input path and survive reentrant activation/closure checks before any command is sent. Acceptance should cover two characters, distinct unsent drafts, input hooks, aliases, IME/edit selection, auto-height, fullscreen, tabbed/tree modes and detach/rejoin of adjacent channels.

This contract is implemented in the next slice below; complete shell parity also needs game filtering and intentional menu geometry while preserving native Window/Notepad access and accelerator behavior.

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


## Main composer: original editor, native surrounding controls

`CWorkspaceComposerSplitter` adds a native destination label and Send button as siblings of the original `CSendView`. The view is still the direct splitter pane with the same HWND, parent and pane ID. Its native edit buffer, user-selected font/colors, selection, history, command hooks, wrapping, IME and original world connection remain in place. The label identifies the owning game, own character and Main stream; it shows Offline and disables the button when disconnected. Original Enter behavior, including local aliases/scripts, remains available. Every original child frame is updated, including multiple New Window views of one document.

The presentation strip currently reserves 48 logical pixels, with an 84-by-32 logical pixel Send affordance. The splitter paints a flat neutral surface and thin separator using the shared semantic palette. Its stored command height, script-facing `SetCommandWindowHeight` argument, manual splitter save/restore and automatic editor growth all describe the original input body, excluding that strip. Each original frame initializes its requested input-body height once from saved/default preferences, then keeps the requested value in memory across native restore/maximize/fullscreen. Scripted height changes, automatic growth and manual splitter drags update that value; normal close saves it in the existing units. This fixes an actual-client regression in which MDI resizing reloaded the old default (41 pixels in the fixture) before a newly requested 120-pixel body could be persisted. It adds no schema and no SQLite writes per layout or keystroke. No user input font preference is replaced by the shell font.

Enter and the button call `CSendView::SubmitMainInput`, extracted from the original Return handler. It retains spell checking, `OnPluginCommandEntered` transformation/decline/clear behavior, aliases, autosay, command history and the original execute/send path. It does not use the channel composer's single-line sender. Active IME composition is allowed to complete before submission. A document-wide scope rejects recursive submissions even if a callback activates another New Window view. View lifetime tokens reject a destroyed/reused HWND or changed document after callbacks. A snapshot of original document views permits restoring action-source/autosay/command-stack state when one view closes while the same document survives in another view.

`DoCommand("SubmitMainInput")` is the explicit scripting/accelerator name for submitting the owning document's original Main draft. This new command resolves that document synchronously and rejects reentrant submissions; it does not use the old general menu dispatcher's asynchronous active-window targeting. Existing command names retain their previous behavior. Plugins already have command execution capability; the agent bridge gains no raw-send tool or permission bypass.

### Verification and remaining acceptance

- `tests/workspace/run_composer_probe.ps1`: 17 Debug checks pass; Release passed 16 checks before the last startup-font regression was added. Coverage includes original GetPane/HWND identity, two-character drafts/selections, custom font retention, System/Light/Dark resizing, 24–240 pixel body-height roundtrips, owner-specific button dispatch, offline disablement, destroyed/replaced view rejection, document-wide recursion ownership and requested body-height retention across resizing. Debug assertions fail the fixture rather than being suppressed.
- Independent `tests/qa-cycle2/run_main_composer_qa.ps1`: 12 passing Debug checks, including narrow geometry, synchronous owner closure from Send, unique refresh of two Main frames sharing a document, document-wide recursion and document/view lifetime disposal.
- Six changed native translation units compiled successfully as isolated Debug objects. This is a source integration check, not a production executable test.
- Final production Debug and Release builds pass. The actual-client fixture passes 12 functional checks through body-height persistence; the additional close/reopen acceptance is blocked by a real Debug assertion during ordinary closure, MFC `wincore.cpp:1078`, `ASSERT(pMap->LookupPermanent(hWndOrig)==NULL)`. The document remains mid-teardown with no original Main views. No assertion was suppressed and neither generic Close nor script Activate was changed to conceal the failure. See `VALYRIA_CHECKPOINT_2026-10-15.md` for the current blocker and evidence.
- Actual computer use verified the final Release startup editor is readable, a keyboard command plus native Main Send reaches only its original Alpha connection through the original plugin hooks, and neighboring channel Arrange / Detach / Join retains the Main input and inline image. This is one isolated local Windows session, not physical mixed-DPI or real IME proof.

Physical manual dragging, real IME composition, mixed-DPI transitions, screen-reader announcements and neighboring channel detach/rejoin with the new Main strip still require actual-client acceptance. Existing native channel docking checks remain relevant but do not prove those combined interactions. The original transcript, standard menu, compact optional MDI document tabs and several legacy dialogs retain their older presentation. This slice implements a real original-editor Main composer; it does not claim complete prototype parity or a release-ready client.

### Close/reopen lifetime correction: source and compile checkpoint

The captured Debug close stack was resolved offline against the matching PDB, without launching or attaching to a client: `CWnd::DestroyWindow` (`wincore.cpp:1078`) → `CWorkspacePane::CloseConnection` (`WorkspacePane.cpp:394`) → `CMainFrame::CloseWorkspaceConnection` → `WorkspaceCloseDocument` → `CMUSHclientDoc::~CMUSHclientDoc` → `CDocument::OnCloseDocument`. The failing object is the channel transcript RichEdit. Its native `RICHEDIT50W` HWND had been registered with `CWnd::Attach`, which adds an MFC permanent-map entry but installs no MFC window procedure to remove that entry at destruction. Consequently `DestroyWindow` destroyed the native control but found its permanent-map entry still present. The assertion interrupted document destruction after the original Main views had closed. The raw Unicode input and search controls used the same incomplete lifetime pattern.

`workspace/WorkspaceNativeControl.h` now attaches a narrowly scoped native destruction observer to all three controls. On `WM_NCDESTROY`, it checks the exact HWND/wrapper identity, removes its observer, detaches the permanent-map entry, and forwards native teardown exactly once without subsequently accessing the wrapper. Observer installation failure rolls back the attachment before the caller destroys the raw window. All text, painting and keyboard processing remains with the original native Unicode controls; the MBCS application's `AfxWndProc` is not substituted for their Unicode procedures. No generic Close/Activate targeting, document retention, focus policy or assertion handling was changed.

The existing evidence is `Temp/mush-modern-acceptance-bwyrztoi/close-debug-trace.txt`; offline symbol resolution is `Temp/valyria-offline-pdb/close-stack-symbols.txt`. Independent source review confirmed the lifetime defect and reviewed the observer's ordering and rollback. `WorkspacePane.cpp` compiles as an isolated Debug object. The new native lifetime fixture compiles and links in both Debug and Release via `tests/workspace/build_native_control_probe.ps1`; that script only builds and has no execution option. The prepared fixture covers Unicode supplementary text, exact permanent-map identity, explicit RichEdit destruction, 64 close/reopen cycles and parent-driven destruction of all three controls.

**Runtime verification remains pending.** Neither the new native-window fixture nor the full-client close/reopen suite has been run against this correction while the user is using the desktop. Background compilation is evidence of build compatibility, not proof that close/reopen acceptance now passes. Root integration builds, the nine-check native lifetime fixture and the actual Debug/Release two-world close/reopen regression are the next gates when native-window testing is authorized again.

Background-only build verification (2026-10-15): full Debug Win32 and Release
Win32 production builds pass with the native-control lifetime patch. Runtime
close/reopen confirmation remains pending; no client, debugger, or GUI fixture
was launched while the user was using the computer.


### Shared input for Main and channels (2026-10-15)

Click a channel transcript without selecting text, or choose its tab/sidebar row,
to use the single input area below the original Main transcript. The destination
shows the character, game and channel, plus the exact sending prefix. Clicking
Main restores its original input. Each channel retains its own draft and history;
Up/Down and Alt+Up/Down recall channel commands without including the automatic
prefix. Main retains its existing history settings and original command engine.
Selecting text, clicking links, searching and dragging panes retain their normal
behavior. Per-pane input boxes are hidden; tool/editor panes are unchanged.

The selected channel's existing Unicode EDIT is temporarily hosted in the Main
splitter, preserving its draft and native Unicode procedure. It returns to its
own pane before the host or channel closes or moves. Submit/history callbacks
capture and revalidate the original channel and connection across plugin focus
callbacks. Common input Edit commands follow the displayed editor. Legacy
Main-only completion and text-transformation commands are disabled while a
channel is selected so they cannot change Main's hidden draft. Explicit scripted
Main submission retains its original meaning. Failed submissions preserve the
draft and show feedback in the channel's status row.

Background verification: Release and Debug composer probes run with `-Hidden`,
covering exact Unicode/prefixes, original Main preservation, shared Send/history,
menu editing, teardown and geometry. The hidden real-pane fixture runs with
`tests/workspace/run_native_probe.ps1 -Source shared_pane_probe.cpp`; it verifies
prefix application, separate drafts/history, rejection feedback and callback
closure using the actual native channel controls. Input-policy and encoding tests
pass. The Debug native-control lifetime fixture also passes its 64 close/reopen
cycles. These fixtures never show a top-level window or take input focus.

Actual-client clicking/caret acceptance, docking while typing, real IME input and
mixed-DPI testing remain pending. The full client is built in both configurations;
background fixtures do not replace those desktop acceptance checks.

### October 16 background acceptance update

This update supersedes the earlier close/reopen blocker and runtime-pending
entries above. Current Debug and Release clients each pass all 13 full-client
Main-composer checks in both tree and tabbed-world modes (52 checks). Final runs
use native CreateProcessW on a private Windows desktop, independently verified
before candidate launch; the desktop is never displayed or switched to.

The investigation fixed two additional defects: early initialization cached a
default input height before the named world finished loading, and restoring a
reused channel pane docked it without detaching its surviving tab-group ownership.
Reopening now preserves the requested input height and avoids duplicate MFC
docking registration. The original native-control teardown assertion did not
recur. Existing native dock-layout checks also pass on a verified private desktop.

Separately, 100 hidden native shared-input checks and both console policy/encoding
suites pass. These cover drafts/history, Unicode/prefixes, owner-specific dispatch,
rejection feedback, editor borrowing, themes/geometry and lifetime invalidation.
See `CLOSE_REOPEN_ACCEPTANCE_2026-10-16.md` and
`SHARED_INPUT_ACCEPTANCE_2026-10-16.md` for candidate identities, evidence and limits.
Physical click/caret, IME, dragging and mixed-DPI acceptance remain open. No
personal installation or world profile was replaced by these tests.

### Inline spelling and channel appearance, October 16

Main and shared channel input now use asynchronous Windows spelling with native
underlines and right-click suggestions. Personal words apply across this client;
game words and spelling/pack preferences follow the navigator's game grouping.
An independently authored Westeros word pack is included. Dictionary management
offers review, Add/Remove, keyboard access and read-only pack inspection, using
the same light/dark tool styling. See `INLINE_SPELLING.md` for scope and storage.

Channels inherit their owning world's Main font and colors before first output,
including the empty transcript canvas. Live world changes update inherited
appearance. Explicit channel overrides are available in Transcript options with
reset-to-Main behavior; routing, ANSI styles, links and image metadata remain
independent of the default appearance.

Release and Debug builds pass. Independent fixtures pass 56 spelling checks per
configuration, 27 appearance checks and dialog layout/control checks. The final
integrated clients pass 52 original command/close/reopen checks across tree and
tabbed modes. A short isolated four-channel responsiveness sample completed all
299 offered batches while fictional unsent Main input changed to exercise
spelling. Physical visual/IME/mixed-DPI and accessibility acceptance remain open;
hidden control snapshots do not establish complete visual painting. Evidence is
in `SPELLING_ACCEPTANCE_2026-10-16.md`, `CHANNEL_APPEARANCE_ACCEPTANCE_2026-10-16.md`,
`SPELLING_DIALOG_UX_ACCEPTANCE_2026-10-16.md` and `PERFORMANCE_TESTS.md`.
