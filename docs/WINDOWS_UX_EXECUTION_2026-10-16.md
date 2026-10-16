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
- Cold RichEdit scrollbars with an uninitialized zero page no longer falsely
  turn end-of-output carets into manual scrollback. The optimized full-client
  trace proved the first-packet race; all diagnostic instrumentation was removed.
- Plain-text viewport resizing now retains live follow or the scrolled reading
  anchor; both modes have an explicit regression.
- Labeled docking guides now use their painted target rectangles for hit tests;
  Join/split/separate-window wording is explicit. Nested split/tab/auto-hide
  restoration and light/dark/red/blue styling passed the native fixtures.

## Integrated checks

Release SHA256:
`D7670F9E64AEC3121EE2B50E2D8A075236506E44E67871D7C01CD72CABCA9912`.

Final Debug SHA256: `056B86F3CADD0962105413624F24CE7861598D2746DC3CE830B923DC6E69F53B`.

Release and Debug full builds passed. The optional Debug browse database was
disabled in temporary local build properties to avoid the existing BSCMAKE size
limit; symbols, assertions and runtime checks remained enabled. Existing OpenSSL
missing-PDB linker warnings remain unrelated to these changes.

All native application runs below used proven private desktops and fictional
loopback worlds. No installed profiles, actual game connections or physical input.

| Full client | Result | Evidence under Local/Temp |
|---|---|---|
| Release Main / tree | 13/13 | mush-modern-acceptance-vro_96ue |
| Release Main / character tabs | 13/13 | mush-modern-acceptance-uvxsaikr |
| Debug Main / tree | 13/13 | mush-modern-acceptance-51ia0lxs |
| Debug Main / character tabs | 13/13 | mush-modern-acceptance-9tyrk6y3 |
| Release unsent Main/channel close and Exit | 7/7 | mush-modern-acceptance-n433zgh1 |
| Debug unsent Main/channel close and Exit | 7/7 | mush-modern-acceptance-adyswq2t |
| Release shared-history canonical wire / alias / draft / isolation | 10/10 | mush-modern-acceptance-quood8su |

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

## Final performance and installation gate

The accepted Release executable above completed a quiet 180-second test with
four genuinely visible/following split panes, Fixedsys input/output, 80 offered
text lines/second, styled/Unicode traffic, public static images, mention routing
and changing unsent spelling drafts. All 1,790 batches completed (14,114 offered
lines), with no crash or backpressure. Completed-output p95 was 77.4 ms and
scripted Execute/echo p95 was 4.1 ms. These are neither per-character timing nor
physical key-to-paint measurements.

All four panes had zero unread/mention badges at startup and after the image
phase without selection or input commands. Warm-to-final private bytes rose from
12.3 to 26.0 MiB, with a 69.5 MiB image-phase peak; final drain stayed flat.
GDI returned to 274, USER was 312, and handles stabilized at 708. This bounded
resource check gives no clear runaway signal; it is not a long-duration leak
proof and does not interpret retained text/image/network caches as leaks.

Exact receipts, per-phase early/late comparisons, image-receipt limitations and
conditions are in WINDOWS_PERFORMANCE_UX_ACCEPTANCE_2026-10-16.md. Earlier runs
with incorrect hidden-frame behavior or lost follow are diagnostic baselines,
not final all-visible acceptance. No temporary diagnostic code remains.

## Installed personal test candidate

The clean portable package identifies source `939f881f3a990cc4789c60e8318c9f3b9368b743`
and contains 292 files. All 291 checksum entries and ZIP integrity were verified.
An additional seven close/Exit checks passed against the final full package on a
private desktop (`mush-modern-acceptance-kjbgomhe`).

Installed `C:/MUSH/MUSHClient-New/MUSHclient.exe` matches the accepted D7670F9E
SHA256 above. The installer verified 343 existing non-managed files unchanged by
hash, including settings, worlds/plugins, personal/game dictionaries, logs and
editor state. It wrote 41 runtime/guide files, retained missing-dependency
installation behavior from the clean package, and did not overwrite existing
custom scripts/plugins. No personal data was copied into the shareable package.

Rollback runtime files and the local preservation/installation receipts are at
`C:/MUSH/Build-Backups/before-workspace-ux-20261016-225012`. The installed client
was not launched, and the older running client was left untouched. BUILD-INFO.txt
in the test folder identifies the latest executable and the Size/keyboard controls.
