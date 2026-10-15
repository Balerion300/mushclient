# Portable workspace docking state

Workspace layout version 3 stores an explicit forest in the existing portable
SQLite preferences database (`workspace_dock_forest_v3`, slot 1). Each root is a
docked region, floating mini-frame, or native auto-hide button group. Binary split
nodes retain both child subtrees, orientation and divider proportion. Leaves
retain native tab order and the selected tab. Identities contain persistent
world/name pairs only; runtime connection suffixes, agent grants and captures
are never serialized. A physical transcript host contributes exactly one entry.

The older layout rows remain available as fallback. Unknown versions, malformed
rows, empty/null/oversized rows and database read failures protect the existing
version 3 bytes from replacement during that session. The decoder accepts at
most 256 panes, 512 nodes, 128 split levels and 256 KiB. It rejects repeated
identities or nodes, cycles, invalid tab indices, ratios outside 1..999,
unreachable nodes, invalid sides, excessive identity lengths and extreme or
invalid rectangles before native layout operations. Native restore also rejects
two identities mapped to the same pane. Invalid native capture does not replace
a valid saved forest.

Restore inserts sibling representatives before expanding either child subtree.
It then restores root thickness and child divider proportions through native
dividers, respecting pane minimum sizes. Floating bounds are clamped to the
available monitor work area. Existing transcript controls are reparented rather
than replayed or rebuilt. The Float selected transcript action first uses the
native tab-detach operation, preserving recent docking information and removing
the old tab before floating.

Unavailable worlds and tools are projected out until opened. Saving retains
their persistent identities and tree positions. Later arrivals restore only
their own saved component. If the reader has changed that component's structure
or joined it to an unrelated component, the arriving pane stays standalone and
the reader's arrangement takes precedence. This deliberate opt-out is separate
from a native restoration failure, which permits legacy placement fallback.
Auto-hide means native edge buttons; hidden panes and closed worlds are distinct
states. Closed worlds have no live transcript entry until reopened.

## Focused acceptance

`tests/workspace/run_dock_layout_probe.ps1` builds a fresh Temp-only native MFC
fixture. It tests a horizontal split with two independent vertical sibling
subtrees, ordered selected tabs, a floating split, a native auto-hide group,
hidden floating state, root/divider geometry, out-of-order world/tool arrivals,
close/save/reopen, manual standalone and unrelated-split opt-outs, data bounds,
and native failure versus manual opt-out. It checks original RichEdit HWNDs,
styled Unicode text, selected ranges and reading anchor positions. A separate
three-tab float/redock case requires exactly three tabs after redocking.

These are native API simulations on the current desktop configuration. They do
not establish physical mouse dragging or cross-monitor mixed-DPI acceptance.
Stored rectangles currently use pixels; monitor availability is clamped, but
logical-size continuity when monitor DPI changes has not been validated. Full
application restart, integrated Release/Debug and existing docking/image
regressions are orchestrator checks against the combined candidate.
