# Windows workspace UX execution — October 16, 2026

Requested deliverable: a complete portable build installed in
`C:\MUSH\MUSHClient-New`, with personal worlds, plugins, settings and dictionaries
preserved. Current branch: `codex/native-valyria-ui`; baseline `68d42b9`.

## Design and behavior

Visual thesis: quiet native light/dark surfaces, restrained red accents, clear
selection and readable dense text, following the accepted Valyria workspace.
Content hierarchy: game/world → own character/login → local output channels;
the original document remains the connection and scripting owner.
Interaction thesis: choose a pane and type immediately; show exactly where input
will go; make docking previews agree with the actual drop result.

## Workstreams and release gates

| Work | Owner | Acceptance |
|---|---|---|
| Shared input, character-wide history, pane drafts, keyboard switching, flexible input height | Input agent + integration | Exact owner/prefix, recall without double prefix, draft isolation, normal editing and spelling |
| Hierarchy, Add custom channel, consolidated channel actions, clickable Separate/+Main, activity indicators | Channels agent + integration | No connection changes or commands sent by navigation; atomic routing changes; visible/live unread policy |
| Dock target previews, tab joining/splits, nested restore, theme and appearance consistency | Docking agent | Painted target matches hit test; reopen preserves layout/drafts; no UI-thread stalls introduced |
| Cross-platform and portable profile architecture | Orchestrator | Document verified framework choices and subset mapping; preserve desktop-only scripting/plugins |
| Integrated build and private-desktop regression | Orchestrator | Release + Debug; fictional profiles only; no interactive-desktop takeover |
| Performance, stability and resource check | Independent agent after implementation | Quiet matched traffic, several panes, input/output latency, bounded resource observations; distinguish history growth from leaks |
| Personal installation | Orchestrator | Back up replaced files; preserve settings/world/plugin hashes; one obvious executable; install receipt |

## Scope and safeguards

Audit existing features before adding replacements. Keep network, triggers,
aliases, Lua/plugins, spelling, editor, logging and agent permissions intact.
Tests use fictional loopback servers and hidden controls or explicitly verified
`ValyriaQA-...` private desktops. No foreground automation, player messaging or
personal-world test connections. Component evidence does not establish physical
mixed-DPI dragging, IME, screen-reader behavior or subjective visual acceptance.

Cross-platform research and profile conversion are follow-up architecture work;
this milestone does not replace MFC or upload any desktop profile automatically.
Only an explicit safe settings subset could be shared with the web; passwords,
tokens, private grants, executable scripts and plugins must never be implicitly
copied into a web account.

Implementation and acceptance receipts will be appended as the work completes.

## Implemented checkpoint

- Shared input retains its original Main editor and borrows channel editors;
  click selection and keyboard switching target the exact original connection.
- Arrow/Alt-arrow history now shares actual sent commands per character, with
  independent drafts/scratch and full-command recall to avoid duplicate prefix
  or alias expansion. Authentication/no-echo input is excluded; history is bounded.
- Ctrl+Tab/Ctrl+Shift+Tab cycles Main and available channel panes while editing
  game input/transcripts; Alt+PageUp/Down is equivalent. Outside those contexts,
  existing world shortcuts remain. Modal dialogs, IME and search are guarded.
- The Size selector offers Auto, fixed line counts and manual sizing, saved per
  world locally. Existing world height preferences are retained. Auto is an
  explicit choice for worlds previously configured with fixed/custom input.
- The navigator retains game → own character → channels, with visible Add
  channel, channel-specific context actions, and clickable Separate/+Main/Mixed.
  Routing changes commit together without changing input target or existing text.
- Sidebar selection now follows the actual selected transcript instead of the
  last transcript in its group. Main appearance inheritance and explicit channel
  overrides remain available. Hidden/scrolled activity counts are not cleared
  merely by selecting input; returning to live output clears them.
- Labeled docking guides now use their painted target rectangles for hit tests;
  Join/split/separate-window wording is explicit. Nested split/tab/auto-hide
  restoration and light/dark/red/blue styling passed the native fixtures.

## Integrated checks

Release SHA256:
`EF6B7AAC5AD59CB98084506C2583D595FD5344128047DBAB49A05C0DC39803A7`.

Release and Debug full builds passed. The optional Debug browse database was
disabled in temporary local build properties to avoid the existing BSCMAKE size
limit; symbols, assertions and runtime checks remained enabled. Existing OpenSSL
missing-PDB linker warnings remain unrelated to these changes.

All native application runs below used proven private desktops and fictional
loopback worlds. No installed profiles, actual game connections or physical input.

| Full client | Result | Evidence under Local/Temp |
|---|---|---|
| Release Main / tree | 13/13 | mush-modern-acceptance-845ulgcr |
| Release Main / character tabs | 13/13 | mush-modern-acceptance-spdxqdqm |
| Debug Main / tree | 13/13 | mush-modern-acceptance-5q_kfr7c |
| Debug Main / character tabs | 13/13 | mush-modern-acceptance-uuhuk5a9 |
| Release unsent Main/channel close and Exit | 7/7 | mush-modern-acceptance-1t7acoo8 |
| Debug unsent Main/channel close and Exit | 7/7 | mush-modern-acceptance-_lkw88_6 |
| Release shared-history canonical wire / alias / draft / isolation | 10/10 | mush-modern-acceptance-mwlw_hi9 |

There are also 15 extracted production persistence checks (including rejected
Main-toggle saves), 13 native production keyboard-routing checks and separate
input/pane/composer, shell, docking and appearance fixtures. Individual guides
record their exact evidence and conditions.

## Limits to keep visible

The legacy Main command-history dialog remains the original source-history API.
The new shared arrow history captures synchronous outgoing human commands;
delayed/queued script output remains outside it. Channel input soft-wraps but
rejects explicit pasted newlines with its draft intact; Main retains multiline
script behavior. Personal drafts are not newly uploaded or serialized by this work.

Arbitrary RGB Main-line/sidebar highlighting and notification sound selection
were not added in this pass. Existing channel font/color overrides and activity
badges remain supported. Physical dragging/mixed-DPI, real IME and screen-reader
acceptance, all classic preference dialogs, and sustained everyday use remain
separate from these synthetic regressions. This is a test candidate, not a claim
of complete cross-platform or full visual-prototype parity.
