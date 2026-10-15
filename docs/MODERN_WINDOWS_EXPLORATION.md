# Modern Windows alternative

Branch `codex/modern-windows-exploration` starts at UI preview `0adf5ac`.
This is a separate exploratory direction; the earlier preview remains intact.

Visual thesis: a quiet Windows workspace with flat integrated pane headers,
clear active tabs, native window behavior and a single restrained accent.

Content plan: game navigation contains the user's own characters/logins, each
containing only its original connection's spawns. The transcript/editor remains
the working surface. Frequent actions stay labeled; advanced commands remain in
Tools and existing menus. Character identity is visible in the action bar.

Interaction thesis: character switching updates the destination explicitly;
tab/pane selection keeps reading positions and drafts; hover/focus states and
on-demand tools provide feedback without ornamental layout animation.

The user clarified that characters mean their own logins, not a player roster.
Navigation groups are display metadata. They never merge connections, infer
credentials, redirect commands, grant agent access or change WDL3 transcript
identities. Default game grouping uses the configured server/port; local game
labels can group separately configured Telnet/TLS profiles deliberately. Character
labels fall back to the profile's configured player name, then its world title.
No credentials or personal profiles belong in source, tests or packages.

The native appearance agent owns bounded MFC docking/tab painting and supported
DWM frame styling, preserving native resize, system menus, docking and hit tests.
The orchestrator owns character grouping/label editing, shell composition,
project wiring, builds and integration acceptance. No external UI framework,
copied client implementation or new dependency is introduced.

Acceptance: distinct characters with identical spawn names retain exact runtime
targets; changing display labels cannot change send destination; game groups do
not issue world commands; callbacks that change/close a world abort actions;
light/dark/high contrast and narrow controls remain legible; tab/float/redock
retains pane identity and selection. Native tests and screenshots must be
distinguished from physical mixed-DPI and daily-use acceptance.

## Preview behavior and limits

The default character workspace uses native MFC document tab groups. Dragging
and splitting tabs retains the original world views and input controls. The
Workspace menu can return to classic child windows. Notepad and Activity use
the same enhanced child-frame base so they remain visible among character tabs.
Deferred activation and title updates explicitly load framework strings from
the executable while retaining the application's existing locale resources.

Use Tools > Game and character labels to organize profiles without changing
their connection or login. Labels are stored in local preferences; the navigator
and action bar use them. Native document tabs use the configured player name.
Identical spawn names remain separate beneath their original characters.

This is an exploratory preview, not a completed visual redesign. The legacy
menu, status bar, dialogs and transcript/input surfaces retain their original
appearance. MFC document tabs have wider measured label padding but retain the
framework's compact native height. Larger tabs and a consistent menu/status
surface need another focused design pass. Arbitrary character document-tab
split restoration is not added here; the existing spawn-layout persistence
remains separate. Physical mixed-DPI dragging and sustained personal use still
require acceptance.

## Verification

- Release and Debug integrated builds succeeded; actual client startup and Lua
  plugin/logging smoke checks passed after fixing deferred tab resources.
- All 13 actual-client scripted logging cases passed.
- Navigator fixture: 30 checks, including two characters with identical Chat
  names, exact destination dispatch, rename retention and collapsed groups.
- Native appearance fixture: 26 checks, including nonblank floating capture,
  unused MDI palette fill and high-contrast fallback.
- Native grouped-document fixture: 24 checks, including Notepad visibility,
  activation, close/return, split/rejoin, classic/grouped mode and content retention.
- Production display-label form: 5 checks for cancel, trimming and Unicode data.
- Actual Windows desktop inspection of the final Release executable confirmed
  three fictional character tabs, switching Mira to Rowan, navigator/action-bar
  synchronization, native window resizing, the label editor, and light/dark
  switching. Screenshots were captured from these isolated profiles.
- A two-minute fictional-traffic run offered 80 lines/second in active phases
  with Fixedsys, four visible split spawns, styling and images. All 1,191 batches
  completed without backpressure or client exit. Output receipt p95 was 40.27 ms;
  scripted Execute/echo p95 was 2.39 ms. These measure batch/script receipts,
  not per-character physical typing. Fifteen image receipts were ready, none
  failed, and fifteen had already been pruned from history before inspection.
  This run preceded the final horizontal tab-padding change; background build
  activity and a separate fictional startup check overlapped parts of the run.
  It is a responsiveness smoke test, not a controlled comparative benchmark.

Tests use fictional local profiles and native software fixtures. No credentials,
personal worlds, live game transcript or agent grants are included.
