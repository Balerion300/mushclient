# Independent agent workspace contract, revision 1

This implementation was written from the permitted user-facing Valyria guides,
not its implementation, recovered pseudocode or `contract.py`. It is not a
certified clean-room process. Model Context Protocol is separate from the game's
MUD Client Protocol. Agent tools cannot send game commands, connect, change
settings, execute scripts, read game input, or read saved credentials.

The frozen schemas are `agent_companion/tools.json`, generated from the independent
`agent_companion/companion.py` inventory by `freeze_schema.py`. The native host and
native companion consume the generated `AgentToolSchemas.h`. Both validate
arguments independently of grants. Exact undocumented Valyria schema parity is
not claimed: the permitted guides describe context/editor behavior without naming
every argument/tool. `read_agent_context`, `read_shared_editor` and
`propose_editor_edit` are independently chosen names for those three operations.
Other argument defaults absent from the guides are independently defined here.

| Group | Tools |
|---|---|
| Output and unsent drafts | list_windows, read_output, search_output, wait_for_output, prepare_draft |
| Conversation/context/editor | get_agent_workspace, read_agent_messages, post_agent_message, wait_for_agent_events, read_agent_context, read_shared_editor, propose_editor_edit |
| Runner-reported work | start_agent_task, update_agent_task, heartbeat_agent_task, get_agent_tasks |
| Human-started capture | list_debug_captures, read_debug_capture, search_debug_capture |
| Supervised scenes | list_agent_scenes, read_agent_scene, propose_npc_action |

## Integration and threading

`Store` lives on the GUI thread. Completed output enters `append`; fragments do not.
`expireWindow` withdraws that pane, its editor/context/capture permissions and scene
proposals. A subsequent `addWindow` does not restore old sharing. `resetWindow`
changes the history generation and forces a cursor reset while retaining an
explicit grant for the same pane. Switching focused characters changes no grants.

`enable(agent, selectedWindowIds, allowDrafts)` returns a session identity. A human
must select panes; enumeration alone grants nothing. Re-enabling invalidates the
old session and all editor/context/capture sharing. Up to eight simultaneous grants
and 32 panes per grant are accepted. Grants, conversations and scenes are memory
only. Revocation preserves bounded human conversation/task summaries and unsent
drafts; agent reads require an active session. Interrupted work is not labelled as
a forcibly terminated external process.

`BridgeHost::start(Store&, agent, session)` must run on the GUI thread. It creates
its own message-only HWND and binds an exclusive IPv4 loopback listener on a
random port. A 256-bit BCrypt random token is written only after creating a
protected current-account DACL directory and file. Launch commands contain only
the endpoint path. `stop()` removes that file/directory and wakes cancellation;
it does not join a worker or wait for network IO on the GUI thread. The caller
must revoke the matching Store session and stop its host on grant replacement,
global revoke, and application shutdown. No token/output is written to receipts.

Workers authenticate and bound framing, reject duplicate JSON keys and nesting
over 16, rate-limit to 20 requests per second, and post a request signal to
`WM_APP+240`. The GUI handler dispatches typed Store calls. Workers wait up to
1.5 seconds for the UI response and use bounded socket IO. Detached workers retain
only shared transport state; they never dereference Store. Closing a host cancels
pending requests without executing them later. Worker socket shutdown/close
ownership is synchronized to avoid descriptor reuse during revoke.

## Draft and review boundaries

`prepare_draft` returns sent:false, opened:false, review_required:true. Identical
request IDs/content return the same draft within the grant; changed content is
rejected. Drafts remain after revocation. Limits are 16 unique requests per grant,
eight retained drafts per destination pane and 128 drafts per process session.

An output grant never shares an editor. Only `shareEditor` does. Reading source
returns its revision. Acceptance checks active share/session/revision and upload
state again. `acceptEdit` returns replacement source to the native editor adapter;
the adapter must apply it as one undo operation and invalidate send review. It
does not perform a game upload. Reject/close/unshare/revoke expire pending edits.

Human-only scene methods expose complete director state without creating an agent
grant. NPC briefs/knowledge are removed for other assigned agents. Scenes begin
paused. Only the current assigned NPC on the latest revision can propose prose.
Direction/assignment/turn/pause/revoke/disconnect invalidates pending/staged actions.
`stageAction` creates an unsent draft with the human-selected prefix. It does not
send or advance the turn. Integration must preserve the scene revision binding
when normal editor send is eventually implemented. A queued game command is not
proof of execution. There is no agent scene approve/turn-control/director-edit tool.

## Bounds and present limitations

Reads retain up to 10,000 logical lines per pane, additionally bounded to 2MiB per
pane and 16MiB across the process. Accounting includes 128 bytes per retained line
for metadata, preventing empty lines from bypassing the global bound. Oldest
retained records are evicted; absolute cursors detect the pruned range and reset.
Window enumeration builds metadata without copying retained line contents.
Returned lines are clipped at
4,096 UTF-8 bytes on a code-point boundary, marked truncated, and capped at 32KiB
per response and 200 lines. Search is literal, Windows Unicode lowercase, limited
to the last 2,000 completed lines and 50 matches. Output cursors encode generation
and next logical index. Wrong generation or pruned cursor returns reset:true.
Requests are capped at 128KiB; replies at 2MiB. Waits poll in the companion for up
to ten seconds and never sleep in the GUI.

Captures are human-started, future-only, capped at eight, 10,000 lines or 1MiB each.
Capture line IDs are stable and preserve source generation/index, original receipt
time when supplied, and separate collection time. Core collection is synchronous
per completed record; reset counts gaps. Detailed retained 32 gap summaries and
asynchronous collection/backlog gaps are not implemented yet.

Core text storage presently enforces conservative UTF-8 byte bounds for drafts,
snapshots and editor source, while advertised string schemas count characters;
multibyte strings may hit the byte bound earlier. The UI must report this error
honestly. Scene notes/NPC material are bounded to 64KiB and actions to 32/64KiB.
Scene revision overflow protection, permanent scene history and complete human
GUI acceptance remain later acceptance work. `validateStagedDraft(draftId)` checks
the active grant, destination pane, scene revision/state/turn and staged action
immediately before human send. `markStagedQueued` rechecks and records only local
queueing. The editor adapter must preserve this ID separately from ordinary text.

## Native companion and validation

The preferred shipped companion is `MUSHclientAgent.exe`, built from independently
authored `agent_companion/MUSHclientAgent.cpp` with the core/protocol modules. It
uses `/MT`, Windows socket APIs, and the vendored JSON header; no Python installation
is needed. It supports `tools`, `--endpoint PATH call TOOL --arguments-file JSON`,
and `--endpoint PATH mcp`. The independent Python stdlib reference offers identical
22-tool inventory for development testing. Both read the endpoint anew on every
call/poll, redact transport errors, and never invoke tools for notifications.

`python tests/run_agent_tests.py` compiles/runs MSVC x86 core assertions, native
loopback/UI-marshalling assertions, and native subprocess MCP initialize/tools/list
checks. `python -m unittest discover -s agent_companion -v` runs eight Python schema,
MCP, loopback, credential-refresh/revoke and subprocess tests. Six additional
runner tests verify private context separation, wrong-turn waiting, changed
direction discard, cancellation, and an actual no-shell external subprocess.
These are synthetic
offline tests, not evidence of a paid model running or live game delivery.

## Optional external performer

`agent_companion/runner.py` is an opt-in Python 3.11 standard-library runner:

```
python runner.py --endpoint "COPIED PATH" --generator-command-json provider.json --duration 600 --max-proposals 1
```

`provider.json` contains an explicit executable argument array. No shell is used.
The user-selected program receives bounded JSON context on stdin and must return
one JSON object with `kind` (`pose` or `dialogue`) and single-line `text`. The runner
uses a fresh temporary working directory per generation. It shares only assigned
NPC private material, public descriptions, explicitly attached contexts/shared
editors, human messages, bounded shared output and queued scene history. Endpoint
paths/tokens are not part of model context. Provider credentials/configuration
stay in the external provider; no credentials are discovered from MUSHclient.

The runner reports tasks/heartbeats, waits for the current assigned turn, rereads
scene state before submission, discards preparation on direction/turn/pause changes,
honors task cancellation, and exits on revoked access. It does not advance turns,
approve proposals or send. Default one proposal; duration is bounded to 1–3600
seconds and proposal count to 1–32. Cooperative cancellation terminates the invoked
provider process; this is not an OS process-tree sandbox and cannot guarantee
termination of arbitrary provider child processes. The external executable owns
its provider-specific capabilities. Configuring a restricted drafting executable
is the human's explicit action. The runner has no automatic default provider.

The test rehearsal uses separate Guard/Merchant fixtures, a controlled mocked
generator and real Python subprocesses for IO/cancellation. It is not real model
characterization or a live game rehearsal. `WinRel/MUSHclientAgentRunner.exe` is a
standalone optional runner built with PyInstaller 6.22.3 and Python 3.11.0 (x64).
It needs no installed Python; its user-selected provider has its own requirements.
Retained notices are `vendor/PYTHON-LICENSE.txt` and `vendor/PYINSTALLER-COPYING.txt`.
`tests/workspace/integration_runner_package.py` verifies the packaged binary with
a synthetic authenticated loopback host, explicit mock subprocess provider,
task/heartbeat reports and exactly one unsent proposal. The native MCP companion
is independently compiled x86 C++ and requires neither Python nor PyInstaller.

New dependency: nlohmann/json 3.11.3, MIT, official upstream single header at
https://github.com/nlohmann/json/releases/tag/v3.11.3; retained license in
`agent_companion/vendor/LICENSE.MIT`. Header SHA256 is
9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6.
No Valyria companion code or unlicensed implementation was reused.
