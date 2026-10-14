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
