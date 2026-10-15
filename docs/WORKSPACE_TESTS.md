# Workspace preview validation

Windows 11 x64 host, VS2022 14.35 x86 C++/MFC, Release client. All tests use
synthetic data, isolated windows/directories or loopback fixtures. Installed
clients, saved credentials, player worlds and production games were not used.

## Verified so far, October 14, 2026

- Integrated Release solution builds with native panes, Scintilla editor,
  review interface, authenticated agent host and supervised scene director.
- Native workspace probe: Unicode, styles, original-world action callbacks,
  static OLE images, floating resize/redock, splits, pause/search/clear, prefix,
  unread acknowledgment, shortcut focus, bounded regex and exact prune records.
- SQLite contract: on-disk reopen, portable schemas, Unicode patterns/prefixes,
  world isolation and precise route replacement/deletion. This is separate from
  the native frame restoring its position on restart.
- Native pane soak: 24,000 appended records across three streams with synthetic
  decoded images and split/redock cycles. Late private-memory spread 8,192 bytes;
  GDI range eight handles tracks retained image count. Final plain streams retain
  1,994 records each; image stream 250. No sustained resource growth observed.
- Editor language and native Scintilla fixtures: Unicode offsets, syntax/help,
  full pinned catalog completion/F1, undo, stable tab identities, rename/close,
  recovery/preferences, read correlation, reference-viewer guards, stale edit
  rejection and an explicit scrollable prepared-send review.
- Agent review fixture: explicit source sharing, full source comparison, current
  revision acceptance, rejection, output-only snapshots, captures, cancellation,
  expiry and pane-to-original-world draft binding. No automatic game sends.
- Native agent core/host/22-tool companion plus sixteen Python tests: schema,
  grant/session isolation, revocation, scene/NPC/turn isolation, idempotency,
  cancellation, endpoint cleanup, duplicate JSON keys and excessive depth.
- Actual client fixture: plugin and trigger callbacks once, wrapped logical
  paragraphs, one reentrant note, omitted output absent. Native GUI grant shares
  only selected Main output; native companion reads it; draft preparation sends
  nothing. Clear resets cursor/history; disconnect removes old window access;
  GUI revoke and normal client shutdown remove endpoint files.
- Standalone optional runner: explicit mock provider subprocess, authenticated
  loopback context, task/heartbeat and exactly one unsent proposal. This verifies
  the packaged cooperation loop, not model quality or live game delivery.
- Existing inline-image backend: zero failures; includes decoding,
  transparency, first GIF frame, HTTP limits, bitmap budget and cache release.

Individual fixture evidence stays in local temporary/build directories; it is
not bundled player output. `AGENT_INTEGRATION_AUDIT.md` describes the actual-client
receipts and which executable they tested. Tests repeated on the final binary
are recorded below before packaging.

## Candidate acceptance still required

Physical dragging across mixed-DPI monitors, subjective keyboard/selection use,
long daily-use sessions and Mac/Wine are not covered by synthetic native API
fixtures. The editor diagnostics are conservative local hints, not a PennMUSH
server evaluator. The full pinned reference is bundled; game-specific discovery
requires an explicit read request. Existing legacy Notepad/miniwindow APIs remain
in place rather than redirecting plugins silently into the new editor.

The preview has the requested primary workflows. Mention rules, arbitrary
shortcut rebinding, priority ordering, automatic channel-to-following-image
association and a legacy Notepad-to-IDE opt-in adapter remain later refinement
checkpoints. Images can be routed by their own record or a broad route; they do
not silently inherit a previous paragraph's channel match. Scene direction and
private NPC briefs are runtime state; recovered source never restores grants or
send destinations. Agent access is disabled on startup and cannot send commands.

## Longer regression measurements

The isolated native image-loader soak ran 660 seconds: 329 cycles, 5,264 unique
requests, all image references expired, released GDI count remained one, and
first/last minute mean private bytes grew 396,766 bytes (under 0.4 MiB).

The actual Release client at SHA256 `a2be63174b4ce5b7541249bcb92c9d25c01a77b04e5e237e33ae3574bdfcd77d`
completed both 600-second MXP/Pueblo image transcript fixtures. Private growth
was -117,760 bytes; GDI stayed 118; user handles ranged 135-137. This binary
predates the final line-cookie/keyboard/proposal-schema fixes. The refreshed candidate receipt is recorded below rather than treating this
older executable as final proof.

The refreshed Release candidate passes the exact upstream issue 82 input,
20,000 conversions of that input, 20,000 conversions of all nonzero byte values,
printing and empty-input comparison. The converter remains unchanged; this is
regression coverage, not a reproduced heap-corruption fix.

The refreshed candidate at SHA256 `f2858477ae6c2810dc4473830e7a521bc974870321a403d665919069ce62184f`
passes both 600-second actual-client MXP/Pueblo transcript soaks. Private-memory
growth was 66,560 bytes; private range 8,728,576-10,440,704 bytes; GDI 122-126;
user handles 138-162. Its Release and Debug builds both pass the issue 82 exact
input/printing and 20,000 repetitions per conversion fixture. The later dock
geometry fix passes separate native and actual UI acceptance at 150% scaling;
it does not alter the image backend.

Actual refreshed-client checks verify the parent paragraph appears before its
reentrant note, callback-cleared source never reappears, threshold pruning keeps
pending omitted text absent, route Ctrl+A/paste stays in its own field, and real
Scintilla revision 4,294,967,296 can be shared/proposed/reviewed. Human full-diff
acceptance changes the source and revision while the synthetic server receives
no command. The standalone runner is rate-limited below the host's request cap.

Final native viewport refinements pass wide-image fit/reflow with preserved source,
selection and scroll anchor; scroll-up readers stay put and bottom readers follow.
Private IDE read/discovery replies are consumed before Main/routing publication,
including stale, malformed, retired and close-before-reply cases. Owned nonce
metadata persists until disconnect and is capped without eviction. Unknown nonces
and unrelated endpoints remain ordinary output. These focused fixtures pass with
no automatic game sends; the integrated private-reply hook is checked separately.

The packaged Release executable has SHA256
`54e0c1258d5b1182fbc585c35ac04514e52c8653b7f33bd42f1b0e044ce4fd05`.
Its final incremental Release and Debug builds pass. The final native workspace
probe and sixteen Python companion/runner tests also pass. The last receive-path
change preserves the bounded prefix of oversized logical paragraphs so routing
and owned private-reply markers cannot be lost through tail truncation. Longer
soak receipts above identify the earlier executable they actually measured;
they are not represented as a soak of this final hash.

The actual private-read acceptance attempt reached a correctly populated editor
but automation activated its owner and dismissed the modeless Read menu before
approval. No read request was sent. Consequently native private-reply fixtures
pass, while the integrated human-approved private-read UI replay remains
unverified. This is an acceptance gap rather than evidence of a successful read.

The clean portable package contains 261 files, including Lua 5.1, Scintilla,
the native MCP companion and the optional standalone runner. All 260 manifest
hashes and the ZIP integrity check pass. It contains official example plugins,
not personal worlds, saved credentials, recovery files or runtime grants.
An isolated copy of that package starts the final executable successfully with
Welcome output and Lua 5.1.4, opens only a fictional offline world, shows no
missing-DLL error, and closes normally. No installed client was used or replaced.

## Modern native docking migration, October 14

The following results belong to the newer docking migration, separately from the
older preview/hash above. The native compatibility test covers the enhanced frame
with classic toolbar/status/world-child bounds, tab grouping/order, float/reattach,
split metadata and hosted-tool teardown. Forty floating tool close/reopen cycles
reuse a stable host instead of exhausting the 32-host limit. Closing transcripts
retains normal hide/unread behavior; tool children receive their normal close event.

The native transcript regression probe passes Unicode/style runs, links and image
object sizing/reflow, selection and scroll anchors, bottom following, pruning,
clearing and human submit/history/failure/disconnect/reconnect behavior. The
24,000-record, three-pane synthetic-image soak lasted 87.157 seconds, pruned 62
times and cleared once. Late private-memory spread was 970,752 bytes and GDI
spread nine objects. This is a bounded component soak, not a ten-minute full-client
or production-session stability claim.

Portable SQL reopen checks pass v1 routes/prefixes/geometry and v2 group/order/
selection/split-neighbor metadata. The cold-route test compiles the exact production
restore/routing methods against isolated MFC/SQLite and passes first-output
restoration, recursive-restore suppression, duplicate sequence rejection and
closed-endpoint rejection. The rebuilt actual client also restored both fictional
named routes without opening the Workspace menu.

`run_input_tests.ps1` passes policy/history and lossless world-encoding suites.
The actual two-world Release UI sent `+alpha unicode` with Greek/astral characters
only to Alpha while Beta was the active main world. An existing alias changed the
command to `+alpha ALIAS_test`; a plugin refusal kept its draft and transmitted
nothing. The plugin receipt confirms the human command-entry hook ran. Native tab
reordering and the separate floating-pane menu action were observed physically.
The tests use fresh temporary profiles, fictional worlds and loopback servers;
no personal game commands or installed-client changes are involved.

The delivered modern Release candidate has SHA256
`7c663d029cc7fa009b7bf11a480aaf9c24b4705732efbe2a1c84e8e1b8143581`.
Its actual client renders the ready image in the restored floating Alpha pane,
without displaying the long URL before it. The pixel regression reproduces the
old URL-plus-picture layout with only 148 colored pixels in view, then measures
14,454 after the fix while retaining the same model record and original runs.
MFC resource compilation and the native original-locale restoration check pass.
Both integrated Release and Debug Win32 builds pass. The final Debug client
starts both fictional worlds and native tab groups with close controls, without
the missing-resource assertion reproduced before the repair.
Its human send path also passes the plugin transformation case, with a receipt
of `+alpha HOOK_TRANSFORMED` and both expected command hook events.

The modern full portable package contains 263 files. All 262 manifest hashes,
ZIP integrity and the final executable hash pass; there are no world files or
runtime preference databases. An isolated full-package copy opens a synthetic
localhost-only world and displays Lua 5.1.4 with no missing-DLL error. The runtime
database filename `mushclient_prefs.sqlite` and its sidecars are now ignored and
rejected by packaging preflight; a fictional database was rejected before any
output folder was created. No such database is present in Git history.

Reproduce native checks with `run_modern_probe.ps1`, `run_native_probe.ps1`,
`run_cold_route_probe.ps1`, `run_image_pixel_probe.ps1`, `run_input_tests.ps1`
and `persistence_probe.py` in `tests/workspace`. The visible pixel test downloads
the public synthetic PNG and checks colored pixels, separately from OLE object
presence. Temporary evidence, binaries, SQLite databases and worlds remain ignored.

Exact arbitrary nested splitter-tree restoration and auto-hide restoration are
not implemented. Mixed-monitor DPI moves, prolonged daily use and full physical
drag/detach/join acceptance remain broader release gates. The initial-login Pueblo
negotiation ordering issue was repaired in the subsequent checkpoint below.

See [PERFORMANCE_TESTS.md](PERFORMANCE_TESTS.md) for the fictional traffic fixture,
font comparison, process-local sampling and administrator-only Windows CPU tracing.
Performance stress results are separate from component correctness passes.

## First-login Pueblo checkpoint

The pre-change Release executable (`39e0523cb20427d7bd5230dff9a7bd121c5f21d6c33e0bb1cd41f0effb1e6c82`)
reproduced login-before-Pueblo-reply with immediate and delayed Telnet greetings
and a delayed TLS greeting. The initial room was plain in all three cases.

`pueblo_login_probe.py` uses fresh temporary profiles, fictional credentials,
loopback servers and an ephemeral localhost TLS certificate. The repaired
Release client passes seven cases: immediate Telnet, 200 ms Telnet/TLS greetings,
800 ms Telnet greeting, silent-server fallback, disabled detection and disconnect
before login. Server receipts verify exactly one login following the Pueblo
reply; client receipts verify the initial image record, command-link action and
exactly one connect callback. Silent-server login proceeds at approximately one
second, and an early disconnect sends no login or connect callback.

The final repaired Release executable has SHA256
`ca99ed37362762fb99af0f3670933374cedc8286f92f7b4fa4c89750cb87cb59`.
All seven cases pass against that binary; integrated Release and Debug builds
pass after the final reentrant-timer guard.

The one-second wait is asynchronous and applies only when Pueblo detection and
MXP/Pueblo are enabled and automatic login/connect text is configured. Normal
startup callback order is retained; timer callbacks pause during negotiation.
Disconnect cancels pending startup. The existing TLS transport and login text
are retained. Servers announcing Pueblo after the fallback can still generate
a plain initial room; this does not attempt indefinite negotiation or change
manual/script-managed login. The image check verifies the actual transcript
record, not a screenshot or fresh pixel/decode acceptance run.

Reproduce with `python tests/pueblo_login_probe.py`; TLS fixtures require Python
`cryptography`. Raw receipts, synthetic worlds and certificates stay in Temp.

## Transcript backgrounds, docking latency and automatic completion

The next Release candidate has SHA256
`dc4aeecc319dfa62eaeb43a1014ca40fece72594f6edcefb6a5dd59c944f908b`.
Integrated Release and Debug builds, native transcript correctness, modern
tab/tool lifecycle and decoded visible-image pixel checks pass.

Transcript records now carry the original world's surface colors, including
empty paragraphs. The native surface and transparent image matte use that
background. Paragraph breaks reset accidental formatting/background inheritance
while retaining explicit colored spans and the configured background bleed.
The isolated background fixture verifies white, dark and custom surfaces,
blank/trailing/unused viewport pixels, replay, colored spans and intentional
bleed. All sampled 4-by-4 pixel patches matched their expected colors.

Resizing no longer reconstructs every retained paragraph merely because the
pane contains images. It replaces the ready image records for the changed
effective width, preserving text, links and view state. A native fixture with
1,800 styled records and eight decoded images exercises 24 genuine resize,
dock/undock and group transitions. Median operation time changed from 1,971 ms
to 129 ms; the maximum changed from 7,250 ms to 527 ms. Exact text, record
boundaries, link/action dispatch, formatting, selection, scroll anchor, HWND,
OLE count and image dimensions remain correct. Builds ran concurrently, so
these timings are evidence from this host, not a guarantee for every machine
or a physical mouse-drag acceptance result.

A separate 120-second actual-client regression at 80 offered lines/second and
two routed panes completed all 1,192 batches (8,620 offered lines), with no
backpressure. Output p95 was 13.35 ms and maximum 361.19 ms; scripted command/
echo p95 was 7.18 ms and maximum 63.58 ms. Nineteen images were confirmed ready,
none failed, and eleven left scrollback before readiness was recorded. This is
a short responsiveness regression; the prior longer test retains its own binary
identity, and physical typing/daily-use acceptance remains separate.

Automatic editor completion uses the local command/function catalog and
explicitly discovered names. Native Scintilla tests type real characters and
verify four `regedit` alternatives, arrows/Tab/Enter acceptance, Escape,
backspace, command/Lua/custom candidates and read-only protection. A visual
fixture confirms the dropdown. Per-keystroke name scanning is capped at 128
bytes; completion does not copy an entire large draft.

Reproduce the new transcript fixtures with `run_background_probe.ps1` and
`run_docking_latency_probe.ps1` in `tests/workspace`. Their fixtures use synthetic
output and independently built native windows, not personal profiles or games.

## Optional game-independent log-start scripts

Release SHA256
`da9a36e9462962b2e6a3d33f88dff212d64876026770d734d6b4b1b2104c2c2b`
passes all nine actual-client cases in `tests/scripted_logging_fixture.py`.
Release and Debug integrated builds pass. The native setup dialog separately
passes normal/world/plugin selection, blank fallback, Cancel and unavailable
plugin checks; Save revalidates the original world and marks it modified.

The native client stores only the selected world/plugin function. It has no
game command, date parser or calendar assumptions. Unset start actions retain
the original logging dialogs; automatic connection logging is unchanged.
`OpenLogExclusive` atomically creates a new file without overwriting. All 418
previous automation-map entries retain their order; the new API uses explicit
DISPID 419. Existing `OpenLog` and `CloseLog` implementations are unchanged.

The real-client fixtures verify world/plugin execution context and arguments,
recursive start rejection (including asynchronous `DoCommand`), existing-file/
missing-folder/already-open error paths, split ANSI replies, full header and
buffered tail exactly once, missing metadata, timeout and cancellation, stale
reply after disconnect/reconnect, same-date collision preservation and two
worlds independently logging while active focus changes. Every case checks that
installing/connecting alone sends no metadata command and creates no log.

Standalone native Lua 5.1 and PCRE tests pass 168 assertions, including filename
safety, regex captures, parsed-field overrides, bounds, relative default folders,
text-mode guards and both optional plugin wrappers. The ready-to-load Blood of
Dragons example parses both dates from the supplied reply and preserves the full
header; it is not installed or selected automatically. Its live server format
still needs user acceptance; fixtures use only synthetic loopback replies.

See `SCRIPTED_LOGGING.md` for configuration, templates, optional preset, callback
bounds, collision behavior and the limitation of uncorrelated late replies.

## Review fixes and usability checks

The independent review reproduced three lifecycle defects. Focused native
regressions now verify that explicitly kept recovery sources survive another
window's autosave, opening a blank editor, and a separate process restart;
draft object/attribute targets survive New, tab navigation and neighboring tab
closure; and surviving selections/reading anchors remain attached to retained
records after inactive or paused transcript pruning.

`workspace/PennRecoverySmoke.cpp` tests native close/keep/discard, legacy
migration, stable local recovery identities, duplicate restoration, capacity,
malformed input and failed writes. The original review reproduction was also
rebuilt against the fixes: two drafts remain two after the second window's
timer, including the closed draft, and a blank autosave preserves both.
Recovery remains source-only; old clients cannot read new PENNIDE2 bundles.
Concurrent separate client processes sharing one recovery directory have not
been validated.

`workspace/PennEditorTargetTests.cpp` exercises real edit controls and owned
approval dialogs, including target edit/revert during outstanding replies and
modal reviews. Targets participate in revision checks without altering source
dirty/undo state. Friendly world names and session labels replace runtime IDs
in editor captions/previews; original destination routing remains separate.

`tests/workspace/run_deferred_view_probe.ps1` builds a fresh Temp-only native
fixture. All 13 focused cases pass: exact RECORD-1200 reproduction, repeated
pruning, partial/all eviction, UTF-16/styles, image placeholder changes,
reading pixel anchor, follow-bottom, paused selection changes, pause without
new output, record boundaries, mid-batch deactivation and clear/reset.
The existing native workspace and 24-transition docking/image-reflow fixtures
also pass. Single-run latency comparisons were mixed; this selection repair
does not establish a new docking speed improvement. Physical mouse and daily
use acceptance remain separate.

Logging Setup adds a native automatic-filenames form for the optional generic
helper: folder/template, an optional explicit request, reply delimiters,
captured fields, transforms and required/header switches. The native form
fixture verifies field editing, opt-in, data round-trip and outer Cancel;
standalone native Lua/PCRE checks pass 185 assertions. Configuration alone
neither sends traffic nor opens a log. Advanced script hooks remain available.

The final review-fix Release executable has SHA256
`d08f3b83eecf79bbe5a42019c2f9c275476df18654cf916598ed3c2d8152e94b`.
Release and Debug integrated Win32 builds pass. All 13 actual-client logging
cases pass on that Release binary, including saved-form configuration,
invalid-PCRE preservation, refusal to reconfigure a pending start, and plugin
save/reload. Native XML variables restore structural line endings as CRLF;
both configuration decoders accept that persistence round-trip without
changing hex-encoded setting values. A failed configuration save reopens setup
with the entered values intact. The native form is 696 pixels tall at the
fixture's 96-DPI setting; mixed-DPI physical acceptance remains untested.
Explicit source saves check flush/close success before discarding recovery.
An owned-Temp-file write-denial regression preserves the prior file, recovery
and open source; it does not simulate a full-disk failure during late flush.
Debug SHA256
`a582f7c79226c49fbdbd7c95e3bfcb149acd628a39407c14e0e267e8373b8b27`
also passes isolated actual-client plugin-hook and configured-reload startup
cases without resource assertions.

## Spawn usability checks, October 15

`tests/workspace/run_spawn_features_probe.ps1` builds a fresh Temp-only hidden
native fixture against extracted production routing, preference and shortcut
methods plus the real transcript pane. It covers passive Unicode mention badges,
priority traversal and wrap, saved settings reload/rejection, native key-message
dispatch, reserved and conflicting chords, AltGr/Windows/IME refusal, deduplicated
multi-route image delivery, runtime world isolation, four-image/two-second limits,
sequence gaps, explicit clear, private editor replies and omitted/plugin-cancelled
records. The transport, clock, native accelerator-conflict and IME-state inputs
are fictional fixture adapters. It uses no personal worlds or server connection.
Physical keyboard layouts, screen readers and mixed-DPI interaction require
separate acceptance; this fixture does not establish those results.

## Combined completion candidate, October 15

Release SHA256
`b4bf0d61e7a9f9a04fd30afea8cb3d18ca5c352e24ecf92808a286c106928a52`
and Debug SHA256
`54d423bda9c40a8bfc499fb93569b47d61c0dd00dfccefb4373115945437809f`
pass integrated Win32 builds. Release passes all 13 actual-client logging cases,
all seven Pueblo first-login Telnet/TLS/fallback cases, and replay of the actual
live `+loginfo` reply through the bundled preset. Debug passes isolated plugin
hook and configuration-reload startup cases.

The orchestrator independently reran 74 native spawn checks, 30 native dock-tree
checks, exact production private-publication and scene-send/read fixtures, and
the existing native transcript and 24-transition docking/image-reflow probes.
The latter retained 1,800 styled records and eight decoded images; its timing
was collected during the Debug build and is not a clean speed comparison.
See `WORKSPACE_DOCK_LAYOUT.md` for portable layout bounds and fallback behavior.

Scene-send callbacks may remove a binding, change or disconnect the original
world, expire the scene, or destroy the editor. The native tests retain the real
scene Store and editor review controls with mocked transport. Approved dispatch
occurs once; cancellation, changed source and stale scenes cannot bypass review.
Late owned private replies remain absent from routes, shared output and active
captures. Actual packaged-client DeleteLines acceptance retains its earlier
candidate identity in `AGENT_INTEGRATION_AUDIT.md`. Desktop private-read and
final scene-send acceptance remain unverified: supported automation cannot
reliably address controls in the separately owned editor window. No unsupported
desktop input was substituted for that missing acceptance.

A clean 120-second actual-client responsiveness run configured four split
spawns at 80 offered text lines/second, enabled mentions/following-image routes,
and used visible startup and Fixedsys world input/output fonts. All 1,191 batches
completed (8,611 offered lines), with no TCP backpressure or client exit. Output
p95 was 63.04 ms and maximum 173.03 ms; scripted command/echo p95 was 28.90 ms and
maximum 62.55 ms. Seventeen images were confirmed ready, none failed, and
thirteen left retained history before readiness was recorded. No builds ran
during this measurement. This is a bounded synthetic regression, not physical
key-to-pixel latency, daily-use acceptance, or an equal-workload comparison to
the earlier two-pane runs. The memory investigation remains closed.

## Separate native UI preview, October 15

The `codex/modern-native-ui` branch builds on the preceding functional checkpoint.
Release SHA256 is
`48573ebca09f3cdd4cdfdb7475bb1d4a3582d730314e7a40da3c4fa877c8b432`;
Debug is `fdc9cb007e6af5c8f65419d08ea80c1875b2a6c4d6993c4cffcaa485a1987f2f`.
Both builds pass. The focused native fixtures pass 27 style, 20 navigation,
65 workflow-form, 30 dock-tree and 74 spawn checks. Existing native
private-publication/read/scene/recovery tests and the logging form pass. All
13 actual-client scripted-logging cases passed the initial UI candidate
`1de31631b5922613d75e88fd98af81d566464fd6ed9457037a86ca8df062f835`;
the captured live reply replay passed the final Release candidate again.

The final quiet 120-second actual-client run completed all 1,190 batches at
80 offered lines/second with four configured split spawns and Fixedsys input/
output. Output p95 was 62.97 ms, scripted command/echo p95 9.49 ms, with no
backpressure or client exit. See `PERFORMANCE_TESTS.md` for limits and image
readiness receipts. Native fixture screenshots and actual desktop inspection
found and fixed narrow action/status overlap, clipped logging captions,
constrained scene overlap, stale grant-state copy and idle disabling of the
Open a world action. The new world action guard rejects focus-script redirection
or closure before dispatch.

`MODERN_NATIVE_UI.md` records the first-pass UI scope and desktop evidence.
The actual owned-editor private-read and scene-send gap remains; the desktop
tool's owned-control click still returned an out-of-bounds error. Navigator
custom docking placement is not yet serialized, and physical mixed-DPI,
screen-reader and daily-use acceptance remain open. Existing personal profiles
and installed clients were not modified.
