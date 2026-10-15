# Spawn docking: interaction acceptance

The Windows alternative needs an understandable way to join and split spawns.
Framework API tests establish layout capabilities but do not establish usable
mouse interaction. The user's physical drag report remains an acceptance failure.

## Observed reference

Valyria 0.55.0rc1, installed on the user's computer, was opened with fictional,
offline profiles for visual comparison. Dragging a tab displayed a large shaded
destination rectangle with a plain label such as "Place below". This document
records observable behavior; no Valyria docking implementation is incorporated.
The desktop automation did not establish precise drop-coordinate reliability.

The intended MUSHclient interaction is:

- Drag one tab to a pane's center to join its tab group.
- Drag to the left, right, top or bottom to split at that edge.
- Preview the resulting rectangle and label the action before releasing.
- Distinguish a pane-relative split from docking at the outer window edge.
- Cancel with Escape, retain drafts and reading position, and save the result.
- Provide the same actions from an accessible menu for keyboard use.

MFC's small smart-docking arrows remain the current interaction. Enlarged,
labeled targets and direct pane-region drops remain design/implementation work;
the fixes below do not claim parity with the reference interaction.

## Correctness fixes

Individual transcript panes now advertise multi-pane floating support. MFC's
mouse-docking path explicitly rejects floating edge-split targets without that
flag, even though direct layout restoration can construct the same split.

The resource-aware tab and floating host classes are now installed after native
pane creation. Native CreateEx replaces those class settings with its defaults,
so installing them beforehand silently lost the application's wrappers.

Smart-docking guides are initialized with executable framework resources before
the first modal drag loop. Legacy localized application resources remain separate.

The private Valyria layout conversion also confused CBRS_ALIGN_BOTTOM (0x8000)
with CBRS_ALIGN_RIGHT (0x4000). The personal configuration was corrected outside
the repository, using the current portable character layout. No personal profile,
world, endpoint, credentials, output or screenshot is included here.

## Evidence and remaining acceptance

Release and Debug builds and actual-client startup/plugin/logging smoke checks
passed. The native docking fixture covers the floating-target mouse-docking
decision, resource-aware tab/float host creation, nested split restoration,
tab identity, reading position and preservation across close/reopen.

An isolated loopback run of the actual Release client reopened all five fictional
spawns, exited normally, and saved the exact expected topology: a right docked
root with two side-by-side upper leaves above a three-tab lower leaf. This proves
restoration/persistence, not the physical drag operation.

Before calling the interaction complete, perform desktop center joins and all
four edge splits, float/redock, Escape cancellation, nested group moves, restarts,
and mixed-DPI dragging. Verify the preview agrees with the final result and that
no pane changes connection, draft, selection or scroll position during a move.
