# Valyria modern workspace direction

Status: substantial interactive browser prototype, October 15, 2026. The user
approved the modern direction and clarified the navigation and channel model.
This document describes that direction; it does not claim the native client has
already acquired this interface. Native icon integration is documented separately
in [VALYRIA_BRANDING.md](VALYRIA_BRANDING.md).

## Visual and interaction direction

Visual thesis: a calm, space-efficient desktop workspace, with neutral surfaces,
readable output, and a restrained red accent that carries Valyria's identity.
Light and dark are equally considered; blue is an optional accent alternative.
The authorized Fourteen Flames mark retains its exact source geometry and colors.
An ivory tile keeps the original gradient legible on both surface modes.

Content plan: a narrow game rail; an adjacent tree of the user's own characters
and their channels; the main output and freely arranged channel panes; an explicit
destination-aware composer. Editor, review, logging, automation, search, and
appearance stay reachable through labeled tools and the command palette.

Interaction thesis: a dragged tab previews the actual pane-relative destination
with shading and a plain label. Center joins tabs; edges create nested splits.
The Arrange dialog exposes identical choices without dragging. Short hover and
dialog transitions sharpen affordances; reduced-motion preferences remove them.

## Product model: games, your characters, their channels

Characters are the user's own logins, never a roster of other players. A game
groups separately configured connections as display metadata. Expanding a
character reveals that character's channels directly beneath it. Several
characters can remain expanded at once. Choosing Rowan's Public channel changes
the composer to Rowan's connection, even if Mira's Public channel was previously
active. An identical channel label does not imply an identical destination.

A **channel is a user-arranged filtered output stream**. Public, roleplay, pages,
staff output, and Main all fit this model. A channel does not require a
server-native MUSH channel entity. It can remain included in Main, also have its
own tab, join other tabs, or occupy its own docked or detached pane. The prototype
demonstrates “Separate channel only” and “Show in Main as well” with a labeled
sample echo in Main. Native routing must continue to place an image with its
matching text; the browser sample does not implement that routing engine.

The prototype also includes Notes and Activity as local views. They cannot send
commands. Connection identity is explicit at the composer and in channel
settings, independent of labels, selected game, tab position, and layout.

## Reviewable prototype

Open [prototypes/modern-workspace.html](prototypes/modern-workspace.html), keeping
the adjacent CSS, JavaScript and illustration plus `../../res/valyria-desktop.svg`
in their relative locations. The package is dependency-free and uses classic
scripts. It can be opened directly or served locally; local-file storage behavior
depends on the browser. A repo-root local server preserves the relative brand
asset path. See [the prototype readme](prototypes/README.md).

The browser prototype implements:

- Two fictional games and four fictional character identities, with expandable
  per-character channel trees and example unread counts that clear on selection.
- Main, Public, Roleplay, Pages, Staff, Notes and Activity views. Character and
  channel drafts, notes, editor buffers, routing examples and layouts remain
  distinct; choosing a different owner restores that owner's state.
- An arbitrary binary tree of nested horizontal/vertical splits across the
  sample channels. Tabs reorder, join groups, or split at any pane edge. Empty
  groups collapse after moves. Layout validity checks reject missing, duplicate,
  or cross-owner channel identities when restoring stored data.
- Shaded, labeled center/edge drop previews, tab-insertion indicators, Escape
  cancellation, keyboard tab navigation/reordering, an accessible placement
  dialog, pointer resize handles, and arrow-key separator resizing.
- In-browser detached panes with movable headers, keyboard positioning, browser
  resize corners, and Rejoin. Detaching every channel is supported. These panes
  are explicitly labeled previews; they are not operating-system windows.
- Persistent layout, theme, drafts, reading offsets, private notes, example
  configuration, and editor buffers in this browser's local storage. Reset layout
  retains the other state. Storage failures fall back to the current session.
- A local Lua-text editor with line numbers, per-character save, indentation,
  and a small insertion-based completion list. “Check example” only checks a
  fixed list of review keywords; it is not a parser or a safety guarantee.
- A fixed agent proposal whose changes can be reviewed, individually selected,
  rejected, or applied to the local example alias/trigger list and notes. No
  agent runs, and no permissions or native configuration are changed.
- Logging preferences, automatic filename expansion, format and stream choices,
  and a download containing fictional sample output. An optional pre-start flow
  previews a game-agnostic command or startup-script buffer, then matches an
  editable sample reply against a named-group regular expression. Gathered
  `game_year`, `game_month`, `game_day`, and arbitrary named captures are
  available alongside real-date tokens. Waiting, matched, missing-field,
  invalid-expression and timeout states control whether a filename is ready.
  The fictional date and illustrative BoD `+loginfo` presets are configuration
  examples, not hard-coded game behavior. No command/script runs, no logger
  starts, and no selected filesystem folder is accessed. The live native reply
  format and regex engine must be verified independently.
- Local sample-output search, a searchable keyboard command palette, editable
  private notes, example alias/trigger enable switches, and a shortcut reference.
- System/Light/Dark surfaces, **red as the default accent**, blue as the optional
  alternative, interface text scaling, independent transcript text size, visible
  focus, reduced-motion handling, and forced-colors CSS.
- An original minimal courtyard illustration retained from the preceding study,
  presented as an inline room image with a local enlarged preview. This is not
  recovered artwork or a demonstration of live MXP/Pueblo decoding.

Custom sample regex evaluation runs in a disposable local worker with a one-second
timeout. Direct-file mode supports the fixed presets; arbitrary regex preview
requires a local HTTP server to permit the worker. This is explicit in the UI.

The command action is always **Preview send**, shows its stable connection
identity, and retains the draft. Content Security Policy disables network
connections (`connect-src 'none'`). No transport, credentials, actual server
profiles, network requests, script execution, or background agent is included.
All resource loads are local prototype/branding files.

## Presentation and native window contract

- Use consistent semantic tokens for base, navigation, surface, elevated surface,
  primary text, muted text, divider, accent, selected state, focus, success,
  warning and error. Red identity does not make red the only error indicator.
  States carry labels, shapes or focus outlines in addition to color.
- Aim for approximately 40 logical-pixel pane headers/tabs, independently
  measurable hit regions and readable labels. Transcript/editor typography is
  independent from chrome; existing output font preferences, including Fixedsys,
  remain an engine-side compatibility requirement.
- Keep density deliberate. No dashboard-card mosaic, decorative title-bar
  substitutes, or red-tinted application background. Tool dialogs use the same
  surfaces, spacing, text hierarchy and control language as the workspace.
- Preserve the real Windows frame, resizing borders, native maximize/snap
  behavior, system menu, taskbar identity, focus, and accessibility. Do not
  imitate working minimize/maximize/close controls in HTML. Native detached
  frames must use the same real operating-system contract.
- Narrow windows collapse the tree into a toggleable drawer. Very small dock
  viewports scroll inside the dock area rather than overflowing the application.
  This study is desktop-first, not a complete touch/mobile client.
- Themes, layout and routing are separate preferences. Switching one must not
  silently change connection identity, input drafts, output selection, or
  transcript content. Browser reading offsets are demonstrated; preserving an
  arbitrary native text selection requires native acceptance work.

## Keep the engine; replace obstructing presentation

The connection, Telnet/TLS, scripting, triggers, aliases, world configuration,
transcript storage, channel routing, inline images, editor engine, logging and
reviewed agent-action code remain valuable. Adapt them behind a presentation
boundary. Do not copy another client's implementation, ship personal profiles,
or conflate this browser mock with production connection logic.

1. **Confirm the composition and terminology.** Review light/dark/red/blue,
   narrow widths, expanded character trees, selected destination, nested splits,
   detached frames, and advanced tool reachability with fictional profiles.
2. **Define a stable presentation model.** Give each connection, stream and pane
   a stable identifier. Views bind to stream IDs; commands resolve the owning
   connection just before dispatch and abort if it changed or closed. Treat
   topology as a tree independent from transcript data. Preserve image/text
   routing and existing authorization boundaries.
3. **Prove a native vertical slice.** Implement two fictional characters with
   identically named channels, main output, a destination-aware composer,
   three nested pane groups, a detached native frame, and live theme switching.
   Test actual mouse center-join and all four edge-split actions, keyboard
   placement, cancellation, restoration, and retained drafts.
4. **Own the geometry where MFC cannot.** MFC's existing tab sizing hooks do not
   deliver the desired per-instance height/font/hit regions. Introduce controls
   that own measurement, drawing, hit testing, focus and accessibility, rather
   than repainting a legacy rectangle. Retain existing docking only if it can
   meet the model and physical interaction contract; replace the docking host
   too if it blocks it. Choose a toolkit after this focused feasibility proof.
   This prototype is not a decision to embed a browser into the native client.
5. **Apply the same components to remaining tools.** Editor/completion, logging,
   aliases/triggers, reviewed agent operations, profiles, menus and status
   surfaces should share the system. Production agent review must show actual
   proposed changes and existing permissions, not the study's fixed examples.
   Preserve the existing game-agnostic logging preflight: run the optional
   configured command/script under its existing authorization, wait for the
   configured reply trigger, collect fields, resolve the filename, then start
   logging. Display missing-field and timeout states instead of silently using
   an incomplete name. BoD `+loginfo` remains a preset rather than engine logic.
6. **Accept behavior and performance.** Check real mouse/keyboard operation,
   assistive technologies, high contrast, mixed DPI, snap, floating/rejoining,
   nested restoration, reading positions and selections, routing correctness,
   long histories, image output, typing latency and sustained use. Avoid rebuilding
   the layout or replaying transcript history on each output update.

The separated browser model may inform a future web interface. It does not
implement a webclient, server transport, authentication, credential protection,
session recovery, or secure remote agent review. Those are separate design and
engineering work.

## Verification boundary

`node docs/prototypes/workspace-model.test.cjs` passes 81 focused assertions for
edge moves, joining, reordering, nested trees, all-detached recovery, persistence,
resize clamps and identity rejection. Independent seeded stress testing passed
9,000 move/join/split/detach/rejoin/resize operations across three fictional
profiles, validating every resulting layout and JSON roundtrip. JavaScript
syntax checks pass. The worker parser has separate named-field, invalid-pattern,
nonmatch and input-bound checks. These checks concern the original browser model
and local sample parsing only.

Browser interaction and screenshot acceptance is recorded separately with the
review run. Neither this model evidence nor browser screenshots establish native
MFC interaction, physical mixed-DPI behavior, screen-reader acceptance,
production performance or release readiness.
