# Independent integration audit, 2026-10-14

No installed clients, user worlds, credentials or real game connections were used.
The actual-client fixture copies only the chosen executable, Lua runtime, locale
resource and independently authored plugins into a fresh temporary directory.
Its TCP server binds 127.0.0.1 with a random port; all output is fictional.

`tests/workspace/integration_fixture.py` and the two XML plugins exercise real
completed-line receive processing with a long fragmented/wrapped line, a trigger
that writes a reentrant local Note, and plugin output cancellation. Plugin IDs are
24 hexadecimal characters. Receipts capture the loaded plugin path before callbacks;
script errors are caught and written to files rather than opening modal dialogs.

Two integrated Release copies passed ten actual-client pipeline checks:

* Every WS_NORMAL, WS_WRAP, WS_REENTRANT and WS_OMIT line reached received-line
  callbacks and triggers exactly once.
* The plugin-cancelled line was absent from the original output buffer.
* The local Note emitted by a trigger appeared once.

Binary hashes and retained evidence:

* `50f932c9f87d90994f988a291748c24e99610fe45f5b8bbdd0b6eff1587b9e9f`,
  temporary fixture `mushclient-workspace-integration-uakc6dfz`.
* `8b160826d1f7845d39e7059935c2afb30bfd74581346a27f562f145fa9be00db`,
  temporary fixture `mushclient-workspace-integration-axsbt9pm`.

These checks do not yet prove that the same data and omissions reach an enabled
agent grant. Both tested builds lacked a Workspace entry in the native world menu
observed through UI accessibility; root subsequently added menu lifecycle hooks.
The later build then failed linking an unfinished `OnWorkspaceRoute` handler.
Actual enabled-grant checks must resume on the next successful integrated build.

Nested Simulate comparison: the older portable baseline
`8550c1ac345f39fa85d82def5d23c866b7c47886f6257730aa7af9aed05b40aa`
and the integrated copy both displayed one parent row and one child row, with one
parent callback and no child callback. An independent second cancellation plugin
also did not receive the child callback. Existing hard-return reentrancy handling
therefore prevents this fixture from testing nested child cancellation. No new
private-output leak was demonstrated by this comparison. The active baseline
fixture evidence is `mushclient-workspace-integration-zrd3vw0e`.

Source findings reported to the integrating owner:

1. Output clear originally left the independently retained agent history intact.
   Root added `WorkspaceClearOutput` to the canonical `ClearOutput` path, resetting
   the grant's history generation and clearing the native Output mirror. Source
   change observed; actual enabled-grant cursor reset is still pending.
2. `DeleteLines` removes tail screen rows without notifying the agent store.
   Explicit deletion must withdraw that text or conservatively reset its history.
3. Native transcript retention prunes at 2000 records, then keeps 1000 (and has
   byte limits). The agent store independently retains 10000 lines. A canonical
   retained-range mapping/prune callback is needed to prevent reads beyond the
   pane's currently retained history. Original main-view RemoveChunk retention
   likewise lacks a store mapping.
4. Original publication code manufactured receipt timestamps at delivery. Root
   now carries the original display CLine time into WorkspaceRecord; collection
   timestamps remain distinct from arrival timestamps.
5. Sender callbacks can invoke Lua and destroy their document. Root copied the
   owner pointer and rechecks live ownership after every send; code reviewed.
6. Scene drafts now retain a scene/action binding and validate exact command text
   as well as original active grant, current turn, state and revision. Human prose
   editing is allowed only before staging. Exact agent retries preserve the
   original request text even after a human edits the proposed prose. Focused
   native core tests cover these rules; actual editor final-send testing is pending.

The independently compiled native core/host/companion suite passed after these
core changes, including all 22 dispatch tools, wrong tokens, duplicate JSON keys,
excessive nesting, eight-agent bounds, 32KiB responses, Unicode search/clipping,
repeated replacement/disconnect expiration and endpoint removal. Fifteen Python
reference/runner tests passed. Native host tests marshal Store calls through a
genuine hidden UI HWND, with network waits confined to the worker thread.

`tests/workspace/integration_runner_package.py` passed against the packaged
`MUSHclientAgentRunner.exe`: synthetic authenticated loopback context, explicit
mock external subprocess, task/heartbeat reports and one unsent proposal. This
proves executable packaging and the cooperating loop, not real model quality or
live game delivery. The runner package includes Python/PyInstaller notices.

Still required on a successful integrated candidate: actual GUI-selected grants,
agent-read wrapped records and cancellation, clear/reset cursors, named pane
prune/disconnect expiry, wrong-session action rejection, unsent draft confirmation,
normal UI shutdown endpoint cleanup, and human acceptance with the scene/editor
revision binding. None is promoted to verified solely from source inspection.

## Final candidate desktop replay (2026-10-14)

The newer candidate `F2858477AE6C2810DC4473830E7A521BC974870321A403D665919069CE62184F`
was exercised in a fresh portable directory against only the fictional loopback
world. These observations supersede the pending checks above where specified.

- All ten plugin/trigger assertions passed. The wrapped line remained one logical
  MCP record, omitted text was absent, and the received parent preceded its local
  reentrant Note exactly once.
- Sharing started disabled; the human selected only Main. A subsequently created
  AuditPane was absent from the existing grant. The modal route editor accepted
  Ctrl+A and clipboard paste without changing the game input.
- MCP drafts remained unopened and unsent. The fixture TCP receipt contained none
  of the draft or proposed source text.
- DeleteOutput inside a received-line callback reset the cursor, removed the
  original WS_CLEARVIA source permanently, and retained the subsequent local Note.
  At 4999 original buffer rows, a reentrant omitted WS_TRIMOMIT source remained
  absent from both the original buffer and MCP search after trimming.
- Explicit sharing of the actual native editor exposed revision 4294967296. The
  packaged native companion accepted that revision and returned a pending edit
  with applied=false and sent=false. The original source stayed unchanged until
  human full-diff acceptance. Acceptance updated the Scintilla source locally to
  `&TEST #1=human reviewed source`, revision 4294967297, confirmed by MCP reread;
  no source text reached the fictional TCP server.
- Disconnect expired both output and shared-editor scopes. GUI revoke removed
  the endpoint and rejected further access. A fresh offline grant followed by
  normal Alt+F4 shutdown exited the client and deleted its endpoint.
- A separate offline restoration fixture restored only its valid FloatingTest
  Unicode regex and prefix; the reserved oUtPuT rule was skipped. A deliberately
  offscreen floating pane was clamped to visible bounds. Normal shutdown/relaunch
  preserved the clamped geometry and exact Unicode prefix, while sharing remained
  disabled with no restored launch command.

One desktop layout issue was reported to the integrating owner: the docked review
pane's child controls extended below the host and its buttons were unreachable.
Double-clicking the host title floated it and exposed the complete review UI;
the actual human acceptance above used that floating UI. A corrected docked layout
must be replayed separately. Sky activates the owner window before each keyboard
action, so direct typing in the owned modeless Scintilla window was not accepted
as a product failure or a successful paste test.

The DeleteLines command inside OnPluginTick was inconclusive: the legacy
m_bInSendToScript guard returns before deletion. This fixture therefore does not
claim actual DeleteLines removal, named-pane retained-range equality, final
scene-send UI validation, real model performance, or live game acceptance.

The final independent native suite passed all core/host/22-tool companion checks,
including the actual 2^32 editor revision and bounded history accounting. Sixteen
Python reference/runner tests passed. The packaged runner mock rehearsal passed
after its 60ms minimum RPC spacing fix (under the host's 20 requests/second cap).
Current packaged hashes are native companion
`F5E32ADC3E4EF0E69808C65B83C32FA66B26865E81BBAABE5E8A1C2AE7892BCA` and runner
`1C49955696A9E4D454FFB34FBDCFE87ABE9C65A5711ECEC022590CA623EEA276`.

## Docking and retained-range follow-up

The later candidate `494C67F40A13CD3EB53D9243DAA4C806B3D2A6837756BFAA1D329A0BE61330C2`
was replayed in another fresh fictional loopback fixture at 150% display scaling.
The previously reported docked review overflow is resolved: lists, previews,
share/unshare controls, proposal accept/reject controls, refresh and status all
fit inside the main frame. Mode selection was exercised in the docked pane.

After 2100 synthetic routed records, the actual native RichEdit document contained
exactly 1099 consecutive rows numbered 1002 through 2100. An explicit AuditPane-only
grant returned exactly those same rows using paged MCP reads beginning at the
generation's initial cursor. Main was excluded; old row 999 was absent from search.
The default no-cursor read correctly returned only the last 200 rows.

A capture started through the docked human review control initially contained zero
rows. A second batch of 2100 routed records crossed two native pane pruning events.
Paged capture reads returned rows 1 through 2100 exactly once, with two gap events
and no dropped rows. Retained-history replacement did not replay old rows into the
active capture. Disconnect then expired both the named-pane output and capture
read scopes.

Scene director Direction, NPCs/turns, and Review actions tabs were opened and all
labels, fields, buttons and status stayed within the window at 150% scaling. No
scene was created or staged in this bounds check. Final scene-send UI acceptance
and actual DeleteLines removal remain explicitly unverified; the earlier tick-based
deletion fixture is still blocked by the legacy send-to-script guard.

## Final private editor-read acceptance boundary

Candidate `65F6C614D0CEF650C3DD930F3356F1997EFF0BBBC4977F3AE8BD110B09184FD4`
was opened in another fresh fictional loopback fixture. Its docked review controls
remained inside the frame at 150%. Main-only output sharing, a human-started future
capture, a sentinel route matching potential nonce replies, and a prepared draft
opened through human review were established. The native editor displayed the
fictional object #1 and attribute TEST correctly.

The actual private read was not executed. Sky activates the owner Main window
before the next input action; this dismissed the owned editor's explicit Read
menu before its command could be activated. Its accessibility menu item was also
unavailable in the cached app state. The synthetic server receipt confirmed zero
outgoing bytes and no read request. This is an automation boundary, not a passing
private-read desktop test or evidence of a product privacy failure. The independent
native privacy fixtures remain the evidence for owned-reply consumption, including
plugin omission, stale/closed reads and wrong-nonce ordinary output. No real game
or personal attribute was involved. Actual private-read UI acceptance, actual
DeleteLines removal and final scene-send UI acceptance remain unverified.

The complete `.workspace-preview-20261014` portable tree was then copied to a
fresh temporary directory. Its final executable hash
`54E0C1258D5B1182FBC585C35AC04514E52C8653B7F33BD42F1B0E044CE4FD05` started
normally, displayed the Welcome output with Lua 5.1.4 and opened only a fictional
offline world. No DLL error appeared. Normal Alt+F4 exited the copied client.
All private-read test clients also exited normally and their grant endpoint was
removed. The packaged manifest tree was left unchanged by this startup check;
test preferences and fictional worlds existed only in the temporary copy.

## October 15 workflow follow-up

The completion candidate builds with exact production publication/dispatch
execution fixtures and the real native editor/scene Store. Owned private-read
replies, including malformed, duplicate, stale and closed-editor responses, do
not reach routes, shared output or active captures. Unknown/wrong-runtime nonces
remain ordinary output. Native scene review tests cover explicit confirmation,
cancel, changed source, scene expiration and editor destruction during dispatch.
Callback invalidation no longer leaves a dangling binding iterator or an uncaught
stale-scene exception. Unconfirmed dispatch retains its check-before-retry warning.

The earlier actual packaged-client DeleteLines check passed: retained rows
16 to 15, deleted marker absent, surviving anchor present, stale agent cursor
reset, and capture records retained once without replay. Its executable SHA256
was `d08f3b83eecf79bbe5a42019c2f9c275476df18654cf916598ed3c2d8152e94b`.
Another supported desktop attempt could not set the owned editor controls even
before opening review; no private-read or final scene-send request was made.
This remains an actual-desktop acceptance gap, not a claimed UI pass. The owned
fixture exited, with zero outgoing server bytes. See `WORKSPACE_TESTS.md` for
the combined candidate build and native test evidence.
