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
