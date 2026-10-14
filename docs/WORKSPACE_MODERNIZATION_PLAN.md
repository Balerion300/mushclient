# MUSHclient workspace modernization

Owner: this chat. Started 2026-10-14. Branch: codex/workspace-modernization.
Baseline: 0fb5738, including inline static Pueblo/MXP images, tab preservation,
toolbar scaling correction and MXP restart recovery. The existing draft PR and
installed client remain usable while this separate branch develops.

## Intended deliverable

A complete portable Windows build with a PennMUSH IDE, native resizable/dockable
spawned transcripts, an agent conversation workspace, explicit local MCP sharing,
reviewed editor proposals, debug captures, and supervised NPC scene workflows.
It preserves existing worlds, Lua plugins, triggers, aliases, timers, links,
images, logging and keyboard operation. Model Context Protocol is distinct from
the game's MUD Client Protocol; the plan includes evaluating the latter's editor
handshake separately. Native Mac/Linux and a browser-renderer rewrite are outside
this Windows milestone, rather than silently promised by a Windows build.

## Provenance and implementation policy

Use the permissively licensed MUSHclient baseline and independently implement
the user's requested behavior. Valyria user-facing guides are behavioral
references; do not copy Atlantis/Lemuria code, recovered pseudocode, icons, or
Valyria implementation into this repository. Do not describe this as a certified
clean-room process. Record origin/license/version of new dependencies. Scintilla
is the preferred editor control subject to a working native integration; a
temporary plain editor cannot be labelled the completed IDE. PennMUSH references
retain their own notices. Existing distribution dependencies retain theirs.

## Architecture

* World document remains the connection/scripting owner. No socket per spawn.
* Transcript store owns completed, styled logical records, generation/revision,
  timestamps and immutable image/action metadata. Views own layout/selection,
  scroll and search state. Docking changes view ownership, not record ownership.
* Native workspace hosts independently bounded main/spawn/editor/agent panes.
  Prototype MFC docking before committing to frame migration. Preserve existing
  toolbar behavior and portable preference storage; registry-only layout is not
  acceptable for the portable distribution.
* Receive parsing, triggers and plugins run once. Routing creates copies/views
  after processing and before final presentation, with explicit omit semantics.
  Plugin cancellation, multiline triggers, local echoes and logging are tested.
* Editor document carries destination identity, revision and unsent source.
  Analysis is bounded and local; accepting completion is one undo step.
* Agent host uses the same stable pane/editor identities through grants. Network
  worker requests marshal bounded actions to the UI thread; no blocking waits
  on the GUI thread. MCP stdio companion is separately packaged.

## Feature inventory and acceptance

### Native workspace and spawned output

Named channel/tell/staff spawns; literal/regex routing; copy or hide from main;
color/link/image preservation; split and tabbed docking; floating windows with
real resize handles; optional per-spawn input and command prefixes; focus and
unread/mention indicators; priority ordering; configurable shortcuts; search,
copy, pause, stable scrollback; per-world/character layouts. Switching active
world never redirects an existing spawn input. Closing a floating pane redocks
or preserves it according to an explicit preference. Destroying a world closes
all its views safely. Layout restoration clamps missing monitors and bad data.
Old miniwindow drawing plugins continue unchanged alongside native spawns.

### PennMUSH IDE

Tabbed unsent documents, line numbers, highlighting, bracket match, completion,
command/function reference, active argument hints, conservative diagnostics,
Unicode-safe offsets, find/replace, undo/redo, dark/light colors, large-source
typing limits, save/open and draft recovery. Attribute loading/saving is explicit
and destination-bound; show exact commands and review before transmission.
Diagnostics never evaluate code. Unknown game functions remain legal; discovery
is an explicit read-only request with nonce, bounded names and endpoint cache.
Supported Lua/plugin editing is a separate language mode. Preserve old Notepad
automation APIs through adapters; no silent changes to scripts using them.

### Agent workspace / Model Context Protocol

Match the behavioral scope documented in Valyria-MCPs 0.5.0 / client 0.54.x:
explicit named agents, per-pane grants, bounded incremental output/search/wait,
unsent idempotent drafts, conversations, selected-text snapshots, explicit editor
sharing, revision-checked edit proposals, human diff acceptance, task/progress/
heartbeat and cooperative cancellation, and bounded debug captures. Inventory
and freeze all 22 current tools before calling parity complete. A bridge reachable
does not prove a model is running. The pane reports runner status honestly.

Sharing defaults off on launch, expires on disconnect/replacement, is revocable
per agent or globally, and is excluded from exports. Bind only loopback with a
random token and account-restricted endpoint file; never log/commit tokens.
No agent credential access, unsent game-input reads, settings mutation, arbitrary
scripts, connection control or game-send tools. External runner configuration is
explicit. Game text and attached context are untrusted data. These constraints
preserve the actual requested Valyria agent interface, not an autonomous player.

### Supervised NPC scenes and external runner

Scene director has shared notes/direction, agent-specific private NPC briefs,
explicit destination and speaker prefixes, paused start, human-selected turns,
revision-bound prose proposals, review-to-editor and normal human send. Pause,
revocation, disconnect, new direction or turn changes invalidate proposals.
Queued locally is distinct from executed by the game. No auto turn advance,
NPC creation or send-on-proposal. Optional external runner integrates tasks and
heartbeats; provider credentials stay outside MUSHclient grants. Include offline
rehearsal so testing requires neither a real account nor a paid model.

## Ordered checkpoints

| Checkpoint | Deliverable | Exit gate |
|---|---|---|
| C0 Baseline/spec | Build receipt, dependency notices, interfaces, feature matrix | Existing Debug/Release and isolated protocol tests pass; requirements assigned |
| C1 Dock prototype | Main frame plus one named native transcript, dock/float/resize | Text survives transitions; toolbar and world switching still work |
| C2 Editor prototype | Native editor control and local PennMUSH assistance | Unicode, undo, syntax, bounded typing and no accidental sends |
| C3 Agent core | Independent grant/history/draft model and MCP contract | Isolation, reset cursors, revoke, bounds and idempotency pass |
| C4 Routed workspace | Real completed output to multiple independent spawns | One trigger/plugin pass; copy/omit, colors, links, images and per-spawn inputs |
| C5 IDE workflows | Attribute editing, preview/review, references, recovery | Wrong endpoint/stale draft blocked; ordinary scripts compatible |
| C6 Agent UI/bridge | Sharing dialog, conversation pane, stdio companion, proposals/captures/tasks | Complete tool contract through packaged client; no secret or input exposure |
| C7 Scene workflows | Director, NPC isolation, external runner and rehearsal | Stale/private/wrong-turn rejection and explicit send review |
| C8 Integrated candidate | Saved layouts, settings/migration, help, full clean ZIP | Multiworld/UI/regression/soak and package checks all pass |

C1/C2/C3 start in parallel. C4 depends on C1 plus transcript interfaces; C5 on
C2; C6 on C3 plus C1/C2; C7 on C6; C8 integrates all. After each checkpoint,
rotate the three agents to implementation, integration and independent regression
work as needed. Never rotate by leaving a failing dependency behind.

## Three-agent execution and merge discipline

Agent Workspace owns docking/spawn files and main frame integration. Agent Editor
owns editor/language/reference files. Agent Access owns independent agent models,
schemas, companion and tests. Orchestrator owns shared document/view changes,
project/resource wiring, plan/status, integration builds and package acceptance.
Agents do not edit shared project files or each other's files without reassignment.
Use one shared branch with disjoint ownership initially; create isolated worktrees
when conflicting integration work makes them necessary. Each report includes
changed files, tests, unimplemented portions and next dependency. No blanket
staging of the surrounding project, personal worlds or recovered material.

## Validation strategy

Automated focused tests cover record identities/generations, prune/reset cursors,
route semantics, Unicode editor offsets, parser bounds, permissions, draft and
proposal revisions, scene isolation and cancellation. Local synthetic MUD fixture
covers fragmented Telnet/Pueblo/MXP, images, links, multiline triggers and reconnect.
Native UI acceptance exercises drag docking, tab groups, floating resize, keyboard
focus, large fonts, mixed DPI, removed monitors, selection, search, split output,
multiple same-game characters and world close. Existing image/heap82/MXP restart
checks remain regression gates. Longer multi-pane output/memory test establishes
a measured plateau; synthetic-only evidence is labelled honestly. Live games are
read-only acceptance unless the user expressly authorizes particular commands.

Full ZIP ships exe, runtimes, notices, help, companion and clean preferences;
no private worlds, saved passwords, grants, endpoints or player output. Verify
manifest hashes, offline startup, original plugin compatibility and recovering
existing copied settings. Install preview separately, preserving working clients.
Attach a new draft fork PR for modernization, separate from inline-image PR.
No merge into the fork's default branch or upstream submission is implied.

## Current status - October 14, 2026

The three-agent rotation has delivered a compiling integrated Windows preview.
The orchestrator owns receive/lifecycle integration, builds, packaging and review.
The fork default branch and installed clients have not been replaced.

| Checkpoint | Delivered | Remaining acceptance/refinement |
|---|---|---|
| C0 | Baseline retained; Release/Debug builds and reproducible build script | Final candidate receipts collected in WORKSPACE_TESTS.md |
| C1 | Native classic-MFC dock/float hosts, styled Unicode/static-image transcripts, separate panes | Physical dragging and mixed-DPI daily use |
| C2 | Scintilla IDE, pinned full help, completion/F1/hints, Lua, tabs/search/undo/font | Diagnostics remain advisory; no server evaluation |
| C3 | Explicit per-agent/window grants, bounded stores, loopback authentication, UI-thread dispatch; actual-client grant/editor flow verified | Daily-use acceptance |
| C4 | Route create/edit/delete, copy/hide, split, saved prefix/layout, unread counts, focus shortcuts; actual native restart placement verified | Optional mention/ordering/rebinding refinements |
| C5 | Explicit correlated attribute/function reads, human reviewed sends, source recovery/preferences | Existing Notepad APIs retained; optional IDE adapter remains a later checkpoint |
| C6 | Native conversation/review/capture/task UI and independent 22-tool stdio companion; actual native editor proposals and human acceptance verified | Human approved private-read UI replay |
| C7 | Supervised scene director, private NPC assignment, unsent proposals, stale/turn binding, optional runner | Real model configuration/quality and game-specific rehearsal |
| C8 | Release/Debug candidate builds, component/native resource tests, full portable ZIP with verified hashes, actual clean package startup, separate draft fork PR #2 | Extended daily-use acceptance |

Implemented privacy fixes include original-line identities across deletion,
canonical history trimming/clear, no capture replay on projection, late trigger
omission decisions, reentrant note ordering, scene binding through every draft
review path, and disconnect/revoke/shutdown expiration. Agent history is bounded
to 2 MiB per pane and 16 MiB total including estimated line overhead. The agent
bridge offers no game-send tool; editor/scene transmissions require human review.

This preview is a Windows milestone. Later refinements include mention rules,
arbitrary shortcut rebinding, transcript priority ordering, explicit association
of a channel match with following image records, and an opt-in Notepad adapter.
They do not block testing the requested primary editor, docking and agent flows.
Do not mark the preview release-ready until its final acceptance evidence and
real-device/daily-use limitations have been reviewed. See WORKSPACE_TESTS.md.
