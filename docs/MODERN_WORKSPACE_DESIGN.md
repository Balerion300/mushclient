# Modern workspace direction

Status: interactive design study, not an installed native client change.
Based on `9b4d6fa` with the existing docking fixes retained underneath.

Visual thesis: a calm desktop workspace organized around games, your characters,
and their channels. Broad flat surfaces, readable tabs, and clear destinations
replace compact legacy chrome. One accent marks the current context and drop
destination. Discord is a reference for navigation clarity, not a skin to copy.

Content plan: a narrow game rail; an adjacent character/channel navigator; the
main transcript; optional split or tabbed spawns; a persistent destination-aware
composer. Connection state belongs beside the character. Editor, logging, search,
automation, and settings remain reachable by labeled actions or a tools menu.

Interaction thesis: dragging a tab exposes a labeled drop target and a shaded
preview of the actual destination. Center joins tabs, edges create splits.
An equivalent placement menu supports keyboard users and makes the behavior
discoverable before dragging. Rearrangement preserves the original transcript,
draft, selection, connection identity, and reading position.

## Reviewable prototype

Open `prototypes/modern-workspace.html` in a browser. It is self-contained, uses
fictional local data, makes no network requests, and sends no game commands.
It demonstrates game/character/channel navigation, draft retention per
connection/channel, tab joining and splitting, contextual placement controls,
and System/Light/Dark appearance with three accent presets. Settings persist
locally in this prototype only. Browser chrome is not a proposed client frame.
The Editor and Tools panels are explanatory placeholders, not working editors
or agent permissions. Dragging in this study is a design demonstration, not
evidence of native MFC docking acceptance. The study models one primary pane
group and one secondary group; it does not model arbitrary nested layouts.

## Presentation contract

- 40 logical-pixel tabs and pane headers, generous click targets, quiet dividers.
- A consistent font and spacing scale for chrome; transcript and editor fonts
  remain independently configurable, including existing Fixedsys preferences.
- Dark and light share identical structure and semantic color roles. System
  follows Windows appearance. Accent is separate from background mode.
- Theme roles cover base, navigation, surface, elevated surface, text, muted
  text, divider, accent, focus, selection, error, and connection state. Future
  named themes use these roles rather than hard-coded colors in each control.
- Active/connected/unread states use labels or shapes as well as color.
- Native window resize, snap, system menu, focus, accessibility, and high
  contrast remain acceptance requirements. Avoid gratuitous layout animation.
- A narrow window can collapse secondary navigation without removing access
  to destinations or advanced tools. Persist layout separately from themes.

## Implementation boundary and checkpoints

Keep the connection, scripting, trigger, world, transcript storage, routing,
editor engine, and reviewed agent-action code. Do not copy another client's
implementation or ship personal profiles. Treat presentation as replaceable.

1. Review this composition in light/dark, normal/narrow sizes, and joined/split
   states. Confirm hierarchy and controls before implementing new native chrome.
2. Establish shared native color/font/spacing tokens and accessible controls.
   Replace the small tab/header presentation with a component that owns its
   measurement, drawing, hit testing, focus, and accessibility. MFC's supported
   tab sizing API does not provide the per-instance height/font control this
   design needs; painting over its old geometry is insufficient.
3. Prove a native vertical slice: two fictional connections, identical channel
   names, joined and split panes, detached frame, and System/Light/Dark. Adapt
   existing routing and pane hosts rather than duplicating transcript state.
   Existing MFC docking may remain behind the presentation only if it supports
   the interaction contract. If it prevents this, replace the docking host too.
   Select a new toolkit only after that focused feasibility check; this HTML
   study is not a decision to embed a browser in the product.
4. Apply the same components to navigation, action bars, floating frames,
   composer, menus, and tool dialogs. Keep advanced commands discoverable.
5. Accept real mouse and keyboard joining/splitting, cancellation, mixed DPI,
   nested restoration, auto-hide, theme changes, assistive access, and retained
   drafts/selections. Measure long-history output/input and drag latency against
   the current build. Avoid per-output layout rebuilding or transcript replay.

No toolkit migration, native performance claim, release readiness, or personal
installation is implied by this design study.

## Design-study verification, October 15, 2026

Browser checks confirmed separate character/channel drafts, switching games
with an explicit destination, joining and restoring a right/bottom split,
keyboard tab navigation, placement-menu navigation and Escape cancellation.
A browser mouse drag showed the shaded "Join tabs" preview and completed the
join. System appearance followed emulated light/dark changes; accent switching
and saved appearance survived reload. The 760-pixel-wide layout exposed its
navigation toggle without horizontal page overflow. Dark, light, narrow, and
join-preview screenshots were captured using fictional profiles. Dark/light
and join-preview captures were visually inspected. This verifies the browser
study only; native dragging, screen-reader operation, and performance remain
separate acceptance work.
