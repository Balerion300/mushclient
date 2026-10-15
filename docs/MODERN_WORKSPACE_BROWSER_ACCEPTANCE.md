# Valyria design-study browser acceptance

October 15, 2026. Original offline prototype, fictional profiles only. These
checks validate the browser study, not the native client's docking or transport.

Passed with browser mouse/keyboard interaction:

- Expand Mira and Rowan together; select each owner's identically named Pages
  view; retain distinct drafts across switching and reload. The command palette
  also opens the correct character/channel destination.
- Join Public into Main's tab group, create a further nested split, detach into
  an in-browser preview, and rejoin with the draft retained. Layout restores on
  reload. Reset restores topology while retaining local buffers.
- Browser mouse drops at center and all four edges display the corresponding
  label and complete the join/split. Escape cancels the drag. Keyboard placement,
  tab reordering, and split-separator resizing work.
- System follows emulated OS light/dark changes. Light/dark with red/blue accents
  all render. A fresh browser context defaults to System with red accent.
- The character drawer works at 760 by 900 without page-level horizontal
  overflow. The initial close-button obstruction was fixed; the visible close
  control and Escape both dismiss the drawer.
- Inline illustration loads and opens its local image preview. Find returns
  sample text; routing a channel into Main adds its labeled sample, and reverting
  the preference removes it.
- Editor completion displays four fixed API examples and inserts the chosen
  entry. Arrow/Enter selection works, Escape dismisses completion while keeping
  the editor open, and Tab leaves the editor. The initial keyboard trap was fixed.
- Explicit review Apply/Reject updates only fictional local configuration.
  Reviewed alias changes appear in the example settings. Sidebar and Activity
  pending counts reflect resolved review decisions.
- Logging waits for gathered fields before completing the filename. Named
  captures produce a game-date plus real-date name. A missing match blocks the
  sample download. The illustrative +loginfo preset works without executing a
  command. A deliberately expensive regex times out after one second in a worker
  while the interface remains responsive.
- Direct file opening loads local scripts and branding. Fixed-preset logging
  extraction also works directly from the file URL. Arbitrary regex preview
  requires a local server; the interface explains this boundary.

Final model checks passed: 81 layout assertions, eight sample-parser assertions,
and 9,000 seeded layout operations across three fictional profile identities.
JavaScript syntax and Git whitespace checks passed. Screenshots cover red/blue
in light/dark, join preview, narrow navigation, logging, and editor completion.

No live server commands, real credentials, installed profiles, native agent
permissions, or personal transcripts were used. Native snap/window behavior,
mixed DPI, assistive-technology operation, output speed, and long-session
stability still need their own native implementation and acceptance.
