# Modern docking and everyday spawned-window use

Authorized October 14, 2026. Baseline: `078c4b9`, workspace preview in
`codex/workspace-modernization`. This work continues draft fork PR #2.

The user prioritizes tabs that can be moved, detached and joined into groups,
and ordinary channel/tell use that makes migration from SimpleMU practical.
Replace the workspace docking layer with modern MFC panes; retain the existing
connection, scripting, triggers, aliases, world documents and private settings.
No Qt port, copied Valyria implementation or new connection per spawn.

## Ownership

- Docking agent: modern frame/panes, native tab groups, detach/join/reorder,
  splits, portable placement and membership/order, native regression fixtures.
- Polish agent: bounded human-input policy/history, route setup and validation,
  responsive controls, keyboard behavior and user-facing quickstart.
- Orchestrator: bridge to the original world's command processing, shared project
  wiring, serialized integrated builds, acceptance, packaging and draft PR updates.

## Acceptance checkpoints

1. A genuine `CMDIFrameWndEx` docking manager coexists with legacy world windows,
   toolbars/status and background preferences. All pane bounds fit the frame.
2. Each transcript retains its own state and identity when its native tab moves,
   reorders, joins another group, splits or floats. Do not replay received text,
   triggers, captures or agent grants during a layout change.
3. Persist group membership/order, split/floating placements and selected tabs
   through portable SQLite state, with missing-monitor clamping. No registry-only
   restoration, passwords, runtime grants or endpoint tokens.
4. Human Enter/Send submits through the original world's normal command handling,
   with the configured prefix and local per-transcript history. Failed submissions
   retain the draft. Changing focused worlds never changes the destination.
5. Ordinary human input is distinct from editor, scene and agent proposals: those
   retain their existing human review and revision/destination checks. Agents gain
   no game-send capability from this change.
6. Route setup and edit/delete operations explain matching and copy/hide behavior,
   reject invalid names/patterns and remain usable at larger fonts/display scaling.
7. Regressions cover Unicode, colors, links/images, selection and scroll anchors,
   bounded history/pruning, disconnect/close, aliases/plugins, layout restart,
   multiworld input and packaged startup. Physical drag acceptance is recorded
   separately from programmatic docking API tests.

Build/test with fictional loopback worlds and fresh temporary profiles. Personal
worlds and credentials stay outside Git. Do not replace a running installed client.
The prior preview is retained as a rollback while the candidate is verified.

## Status

The two-agent implementation uses `CMDIFrameWndEx`, `CDockablePane` leaves and
native `CTabbedPane` groups. Legacy world documents, connection handling,
scripting, aliases and triggers remain in place. Native regression fixtures cover
grouping, reorder, float/reattach, split metadata, tool close/reopen, transcript
state and bounded history. The actual Release client confirms first-output saved
route restoration, native tab reordering, separate floating panes and human input
going to the original world even when another world is active.

Portable persistence stores group membership/order/selection, floating geometry
and bounded split-neighbor metadata. Exact arbitrary nested splitter trees and
auto-hide layouts are not restored. The installed preview is left intact while
this separate candidate passes integrated acceptance. See `WORKSPACE_TESTS.md`
for the measured build and any remaining gates; component tests alone are not
proof of all physical dragging, display scaling or daily-use behavior.

The initial-login Pueblo regression is repaired in the subsequent connection
checkpoint. Pueblo-enabled automatic login waits asynchronously for the greeting
reply or a one-second fallback. Actual-client Telnet/TLS regression receipts
verify that the initial room already contains its image record and command link;
silent servers and disconnects are covered. See `WORKSPACE_TESTS.md` for scope.
