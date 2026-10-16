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
