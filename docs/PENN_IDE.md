# Native PennMUSH editor checkpoints

`OpenPennEditor(owner, endpointIdentity, initialUtf8)` creates a native modeless
window; endpoint identity must identify the original world and connection session.
The editor has no socket. Attribute review shows the exact
single-line command, destination and source revision, then offers copying through
the Unicode clipboard. A changed draft or destination invalidates review.
`PennEditorSnapshot` and `PennEditorApplyProposal` expose revision-checked UTF-8
byte edits; proposed edits show a before/after review and require human acceptance.
Call these APIs only on the owning UI thread. Closing a world must destroy its
editor windows or invalidate their bindings; root integration owns that lifecycle.
The host registers `PennEditorSetTransport` for explicitly approved read-only
requests and passes completed UTF-8 response lines to `PennEditorHandleOutput`.
This API returns whether the line is an owned private protocol reply. The host
must call it before shared Main/spawn publication, including lines already
omitted by plugins, and suppress owned replies from shared output/routes. Reply
ownership is separate from applying source: stale, replayed, superseded, malformed,
oversized and closed-editor replies remain consumed without changing a draft.
Per-endpoint nonce tombstones survive editor close and are cleared only when
`PennEditorInvalidateEndpoint` disconnects that original endpoint. The global
registry retains at most 8192 nonces; additional reads are refused at capacity
instead of evicting ownership and leaking a delayed private attribute response.
`PennEditorInvalidateEndpoint` must be called on disconnect and world close;
it clears read requests/cache and destination bindings while preserving source.
`PennEditorSnapshotInfo` adds the stable runtime `documentID` of the selected tab
to its source revision and endpoint. All API calls belong on the UI thread.
`PennEditorListSnapshots()` inventories all ordinary unsent tabs with their
window, document ID, endpoint and source revision; reference tabs are excluded.
`PennEditorApplyDocumentProposal(documentID, revision, replacement)` locates the
exact document, selects it for human diff review and applies one undo step only
if its revision still matches. It never dispatches commands.

The optional host callback `PennEditorSetSendPrepared(endpoint, documentID,
revision, commands)` is reached only through the human menu "Review and send
prepared commands" after showing every command and rechecking the exact tab,
revision and endpoint. Filled object/attribute fields prepare an attribute-set
command; empty fields prepare exact source lines (128 lines, 4096 UTF-8 bytes
per line, 64 KiB total). Control characters are rejected. Multiline attribute
source requires deliberate user flattening before review. Proposals, reads,
recovery and autosave never call this sender. The host resolves the original
connected world, validates the entire vector before dispatch, and reports actual
transport results in the ordinary world transcript. Queued commands do not prove
server execution.

Compile `workspace/PennLanguage.cpp` and `workspace/PennEditorWindow.cpp` with
precompiled headers disabled and C++17. Run `workspace/build-scintilla.ps1` in
PowerShell, then copy `third_party/scintilla/bin/Scintilla.dll` beside the preview
executable. The script invokes the upstream Win32 nmake build under VS2022 x86.
No downloaded binary is used. Source is unmodified official Scintilla 5.5.7 from
https://www.scintilla.org/scintilla557.zip, SHA256
`801faef6a2404ec4cbe6f22c853d9bb63fc5e18f169f302a9c69b9d6f93349dd`.
Ship `third_party/scintilla/License.txt` with distributions. Its permissive
copyright/permission notice must remain in copies and supporting documentation.
Scintilla has no Lexilla dependency here: local container styling is independently
implemented. No Valyria, Atlantis or Lemuria source was used.

The prototype provides native tabs with separate undo histories, line numbers,
bracket highlighting, UTF-8 source, completion, starter reference, bounded active
function hints and advisory bracket marks, open/save, search and selected-match
replace, theme toggle, bounded recovery of open drafts, attribute preview
and revision-checked proposed edits. Analysis debounces 180 ms and examines at most
256 KiB; hints scan at most 8 KiB; source files and proposed edits are bounded to
4 MiB. Unknown functions are legal, source is never evaluated, and diagnostics
remain advisory because PennMUSH interpretation depends on command context.
Pinned local help is indexed at startup with preserved aliases. Ctrl+Space uses
the full command/function catalog plus explicitly discovered game names; F1
opens the topic at the caret or its active function in a read-only native viewer.
Active argument hints use the corresponding upstream signature when present.
Unknown custom functions remain legal. Help files are bounded to 1 MiB each,
2048 indexed topics and 64 KiB per topic; corrupt data cannot evaluate or send.
The compact built-in reference remains a fallback when packaged help is missing.
Each tab can switch to a separate Lua/plugin source mode with bounded keyword,
quoted/long-string and line/long-comment styling and keyword completion. This mode does
not run plugin code or claim a full Lua semantic checker.
The full offline-reference menu opens a read-only native tabbed editor with
unmodified upstream function/command documentation. Copy
`third_party/pennmush-reference` beside the executable to enable it, retaining
`COPYRITE` and `ORIGIN.md` notices. Upstream docs are pinned to commit
`95ad3511d0410b9e132f140fabc4ca7afec433da`; installed game capabilities may differ.

`PrepareAttributeRead` and `PrepareDiscovery` construct explicit read-request
previews with nonce, endpoint and draft revision. They never transmit. After
human approval, the integrated host can route a bounded response to
`AcceptAttributeRead` or `AcceptDiscovery`. Wrong endpoint, nonce, stale revision,
error response, oversized content and replay are rejected. Source returned by
`get()` is not evaluated; it is wrapped in `encode64()` for one response line and
decoded locally. `functions()` requests names only; accepted discovery
names are bounded, deduplicated and must use a conservative name alphabet.
The host must supply an unpredictable alphanumeric nonce, bind the real response
to its original connection and expire requests on disconnect. Game text is
untrusted, so a nonce is correlation, not authentication.

Focused standalone tests: compile `PennLanguageTests.cpp` and `PennLanguage.cpp`
with the VS2022 C++ compiler (`/EHsc /std:c++17`) and run the resulting executable.
They cover malformed UTF-8, multibyte offset rejection, revision drift, endpoint
isolation, bounded parser work, unknown functions, and command review injection.

C5 still requires integrated transport/lifecycle acceptance and optional legacy
Notepad automation adapters. Draft tabs can be renamed without changing their
document identity, or closed with a save-before-close choice for nonempty source.
Ctrl+W closes the active draft; closing the last tab leaves a new empty draft.
Ctrl+Space completes, Ctrl+F focuses search, F3 finds next, Ctrl+S saves source,
and Ctrl+N opens an unsent tab. Recovery is an atomically replaced temporary
source-only bundle (32 tabs, 4 MiB per source, 8 MiB aggregate), excludes reference
windows and carries no destination or permission grants. A malformed bundle is
rejected before altering current drafts. Recovery never transmits. Full IDE completion must not
be claimed until integrated UI and lifecycle acceptance pass.
`PennEditorSaveRecoveryFile(path)` and `PennEditorRestoreRecoveryFile(window,
path)` support explicit recovery destinations. The default is exe-relative
`editor-state/drafts.recovery`; theme, source font size and bounded window size
persist in `editor-state/preferences.ini` through native file APIs, without the
registry. `PennEditorConfigureStorage(folder)` may choose another writable folder
before any editor opens. Ctrl++ and Ctrl+- change font size (8–36 pt); restored
geometry is clamped to the current monitor work area. A write failure preserves
the existing recovery and tells the user to save source explicitly. Exclude
`editor-state` contents from clean distribution packages.

Reference viewers reject New/open/recovery, mutation, sharing snapshots, proposals
and send/read/discovery paths. Their text is read-only, they are excluded from
draft recovery and agent inventories, and they do not save preferences. Prepared
command review uses a resizable, scrollable read-only native dialog so every
bounded command remains accessible before its explicit Send once button.

Native smoke fixtures use the real locally compiled Scintilla control and a
mocked transport, approving only dialogs owned by their own fixture window.
They cover UTF-8, coloring, margins, undo, tab identity, inactive-tab proposals,
generated attribute-read and discovery commands, wrong-endpoint/replayed output,
multiline Unicode base64 response, all-tab recovery and disconnect invalidation.
A separate mocked reviewed-send callback runs exactly once after its own human
review dialog; proposals and recovery do not invoke it. No live game is contacted.

Legacy Notepad adapter boundary: retain existing ODL dispatch IDs, signatures,
title lookup and return codes for SendToNotepad(51), AppendToNotepad(52),
ActivateNotepad(53), ReplaceNotepad(61), SaveNotepad(174), CloseNotepad(175),
GetNotepadLength(214), GetNotepadText(215), geometry/color/font APIs (262,267,270,
271), NotepadSaveMethod(299), GetNotepadList(310), and NotepadReadOnly(325).
Leave the established Notepad implementation active by default. An optional
adapter may map each legacy title to a dedicated editor document and convert
BSTR UTF-16 to UTF-8 at the boundary, preserving Unicode character-count semantics
for legacy length and source-save flags. Never expose grant sharing or agent
proposal APIs through legacy calls, silently redirect titles, or change the COM
surface. Host integration owns the adapter and needs script regression fixtures
before switching any existing call to the new editor.
