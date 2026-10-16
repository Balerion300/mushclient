# Windows input UX - October 16, 2026

## Implemented

- Runtime sent-command history is shared by all Main/channel panes belonging to the same original connection. Separate characters/worlds do not share history. Each pane retains its draft and history navigation scratch.
- History captures actual synchronous outgoing lines after aliases/auto-say expansion, during human send scopes only. Canonical recalled commands bypass pane prefixes, aliases and auto-say; the existing send-plugin checks still run. Main and channel input labels identify recalled full-command mode. Clearing a recalled draft returns to normal mode.
- Bare command prefixes add a separator automatically (`+staff` plus `Hi` sends `+staff Hi`). Existing spaces, richer syntax such as `page Person=`, and pose punctuation remain literal, matching the web client.
- Native per-pane drafts, Unicode channel input, original Main CEditView HWND, scripting, plugin processing and spell checking remain in place. Channel prefix labels display literal configured text without presentation quotes.
- Connection-scoped history has a 1,000-command/256 KiB limit. Capture buffers have the same byte/count cap. Common authentication commands, no-echo/password input, failed sends and plugin-omitted commands do not enter the new shared history. History is not serialized or synced. Closed worlds release their sessions; closed editors/transcripts release cursors. Moving a transcript transfers full-command mode and navigation scratch before deleting its old identity.
- The shared composer has an Input Size menu: Auto (1-10 lines), 1/2/3/6/10 fixed lines, and Custom for the existing draggable divider. Channel input now soft-wraps. Mode choices persist in the local settings database keyed by world ID. Existing custom geometry is preserved; existing auto-resize worlds retain auto behavior. Initialization waits for a loaded/named world and measured font geometry.
- Root integration supplies keyboard cycling between same-character panes; pane selection focuses the shared editor.

## Verification

- `tests/workspace/run_input_tests.ps1`: input validation, UTF-8/Windows encoding and new shared-history unit suites pass. Tests cover shared sent commands, independent pane scratch, full mode, authentication exclusions, omitted capture, connection isolation, closed-session cleanup, cursor transfer/cleanup and capacity limits.
- `tests/workspace/run_native_probe.ps1 -Source shared_pane_probe.cpp`: hidden native actual-pane fixture passes. Checks Unicode send, canonical recall, move recalled draft to a different Transcript identity and send with its prefix exactly once, independent character history, rejection preserving drafts and callback-driven pane destruction.
- `tests/workspace/run_composer_probe.ps1 -Hidden`: native original-view lifetime and geometry fixture passes, including fixed height stability, auto growth/shrinkage and custom height preservation. Latest evidence: `C:/Users/Elio/AppData/Local/Temp/mushclient-composer-ce6eae35db52469ea45f6752ebfd8f86`.
- Tests do not connect to live worlds, take physical desktop focus or alter user profiles. Root owns integrated client build and post-integration acceptance/performance.

## Explicit limits

- Delayed/queued alias output sent after the human submission scope ends is intentionally excluded from canonical shared history; the existing Main command-history dialog remains a legacy source-history view. Existing legacy history and plugin APIs are retained.
- Channel input still submits one logical command per send. Soft-wrapped text works; pasted explicit line breaks are rejected with draft preservation rather than silently splitting or executing code. Main retains its existing multiline/script behavior.
- Drafts are kept per pane during this client session. This work does not introduce a new disk draft store or cloud synchronization.
- Physical IME, mixed-DPI monitor transitions and real-user overnight usage still require platform acceptance; hidden native tests are not claimed as that evidence.
