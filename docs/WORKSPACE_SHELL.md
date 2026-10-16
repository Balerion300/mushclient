# Native main navigation

The daily action bar provides Worlds, Connect/Disconnect, Find, Logging,
Channels and Tools. Tools exposes the existing PennMUSH editor, agent access,
conversations, scene director and review windows, plus appearance and classic
toolbar access. The existing menus, accelerators and original MDI transcript
and composer remain available. New chrome uses the shared WorkspaceStyle API;
it does not change game fonts, ANSI colors, input history or review permissions.

The collapsible Worlds and Channels navigator stores friendly display text
separately from exact runtime world/channel identities. World rows show the
actual connected, connecting, disconnecting or offline state and logging state.
Channel rows show unread and mention counts. Priority affects navigation order,
with stable name ordering for ties, without reordering physical dock panes/tabs.
Existing world tabs and pane shortcuts update the selected navigator item under
a guard that prevents navigation callbacks. State-only refreshes update text in
place and retain focus, expansion and native item handles.

Every world-specific action resolves the captured original runtime world again
before dispatch. MDI activation can run synchronous focus scripts, so the
production activation guard resolves it once more afterward and verifies that
the active document is still that original world. Redirection or closure aborts
the action. Display labels and theme/navigation changes cannot authorize game
sends or sharing. Find on a channel focuses its existing transcript search field.

Shell preferences use the portable SQLite database. Navigator visibility,
appearance and the previous visible classic-toolbar subset persist. Classic
toolbars retain their native positions and remain reachable through Workspace
and Tools. The transcript split-tree format is unchanged. The new navigator's
own custom docking width, floating and auto-hide placement are not currently
serialized by shell preferences.

## Focused native evidence

`tests/workspace/run_shell_probe.ps1` builds a fresh Temp-only native fixture and
records source/executable hashes. Its 20 checks cover original runtime targets,
synthetic reentrant activation redirection/closure, native action registration,
channel selection, 500 state-only updates without focus/callback/item churn,
external active-world selection, light/dark tree colors, original content fonts
and selection, disabled actions for a closed world, navigator collapse and narrow
layout containment/nonintersection. The inherited native idle command update
runs with no worlds open; Open a world must remain enabled and dispatch only
the open-file action. It writes light, dark and narrow BMPs for
visual inspection; these represent the native fictional fixture, not an actual
connected client. The fixture retains the native control accessibility roles.

Integrated startup, existing user layouts, real world command routing and actual
client responsiveness are separate orchestrator checks. Physical mixed-monitor
DPI, screen-reader behavior and sustained daily use remain acceptance work.

## Connection profiles and game shortcuts

The navigator provides **New connection...** for local server settings and **Open saved profile...** for an existing world file. Game account login happens after connecting. **All open connections** remains the navigator title: the game rail switches to the first character and scrolls to that group; it does not filter out other games. **Tools > Spelling dictionaries** opens personal, selected-game and bundled vocabulary. Output channels are local filtered transcripts rather than commands to join server channels.
