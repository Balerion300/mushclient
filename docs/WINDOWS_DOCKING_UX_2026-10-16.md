# Windows docking and activity UX acceptance - October 16, 2026

This pass builds on the existing native MFC docking engine, modern light/dark chrome, world appearance inheritance, and portable nested layout forest. It does not replace the network, scripting, trigger, plugin, or world-management core.

## Changes

- Visible Place left/right/above/below and Join tabs labels now use the same rectangles for hit testing and painting. Previously a labeled side target near the center could select Join tabs because only broad outer-edge regions were tested.
- Small-target fallback accounts for display DPI, preserving legible native guides when the five labeled targets would be crowded.
- Placement menus say Open as separate window and identify the target submenus as Join tabs or split beside.
- Selecting a pane for input while reading earlier output preserves unread and mention badges. Returning to the live end clears them without waiting for another incoming message. Visible panes following live output remain read.
- The docking regression launcher now verifies a disposable desktop before launching its native frames. A hidden process alone does not prevent shown child windows from appearing on the user's desktop.

## Acceptance

Expanded native docking regression passed on an isolated ValyriaQA desktop, using fictional transcripts only. Evidence directory:
`C:/Users/Elio/AppData/Local/Temp/mushclient-dock-layout-256fc6d3bd3c462f848d13c73097fe1d`

Coverage includes all five labeled hit targets; disabled targets; projected native placement geometry; all four split directions and tab joining; nested sibling split restoration; active/ordered tabs; floating splits; auto-hide groups; out-of-order world arrival; close/reopen; deliberate manual rearrangement; hidden roots; monitor work-area clamping; retained transcript HWND, text, selection, reading anchors, and unfinished input; and live/scrolled-back activity badges.

Saved divider ratios restored at 600/600, 400/400, 550/549, and 450/449 (requested/observed in thousandths). Existing nested forest round trips therefore required no format change.

This is native API fixture acceptance. Physical pointer dragging across mixed-DPI monitors, screen-reader behavior, and subjective real-display light/dark polish still require interactive acceptance. No user world, credential, connection, or desktop was used.

## Final resize regression

A subsequent real-client benchmark exposed a startup/resize follow concern. The native fixture reproduced a defect: shrinking a plain transcript viewport at the live end left RichEdit at the old scroll offset, silently disabling follow. The original code restored the view only when an inline image was actually resized. The same path skipped height-only resize restoration.

The corrected code restores the saved live-end or reading anchor on every transcript viewport resize, retaining pending batch state and image-adjusted offsets. The expanded native regression now passes both bottom-follow and scrolled selection/reading-anchor checks, along with the entire preceding docking/layout suite.

Failing baseline: `C:/Users/Elio/AppData/Local/Temp/mushclient-dock-layout-03ea0e9002524c128f1dc5d5656180bb`.
Passing correction: `C:/Users/Elio/AppData/Local/Temp/mushclient-dock-layout-159de7f72692447f87fbe27f3893c671`.

Appearance acceptance also passed on private desktops: 34 native appearance checks and 27 shared style-component checks, covering light/dark, red/blue accent, high contrast, native keyboard/accessibility geometry, frame/system menus, content identity and selection, and status resize affordance.
Evidence: `C:/Users/Elio/AppData/Local/Temp/mushclient-appearance-84efd9b46eb54240a44330db91383e83` and `C:/Users/Elio/AppData/Local/Temp/mushclient-style-components-7bd50db2896a4b50b2348a20be15e459`.
