# Spawn windows

Open a world, then use **Workspace → Output routes**. The route manager keeps
the original world shown at the top even when you switch to another world.

1. Choose **New**, or use **Channel example** / **Tell example** as a starting
   point. Examples must be adjusted to your game's actual output and commands.
2. Name the transcript and paste a distinctive received label into **Incoming
   text to match**. Literal matching is case-sensitive. Enable regular expressions
   only when you need them; invalid expressions stay unsaved.
3. Leave **Show matches only here** unchecked to copy matching lines while keeping
   them in Main. Check it to hide matching lines from Main's presentation.
4. Set an optional outgoing prefix and save. The prefix is used exactly as entered:
   `+pub ` plus `Hello` becomes `+pub Hello`; `page Alice=` plus `Hello` becomes
   `page Alice=Hello`. Include the needed space or equals sign. A tell prefix has
   a fixed recipient; incoming tells never change it automatically.

With a blank prefix, type a complete game command. **Enter / Submit** processes
ordinary human input through that original world's command handling, including
its aliases and command-entry plugins. It does not use whichever world currently
has focus or inject Main's AutoSay prefix. “Submitted” means the client accepted
and processed the command; it does not prove server execution or delivery.

Each transcript keeps its own unfinished draft and successful-input history.
**Up** recalls earlier raw messages; **Down** returns toward newer ones and then
restores the unfinished draft you had before browsing. The current prefix is
applied when a recalled message is submitted. History is bounded to 100 entries
and 64 KiB per transcript, and consecutive identical submissions do not duplicate
it. Failed validation, plugin rejection or disconnect keeps the draft and history.
Reconnect the original world before retrying a disconnected input.

One submission contains one input line, with a default limit of 4096 UTF-8 bytes
including the prefix. Line breaks, tabs, control characters and invalid Unicode
are refused; pasted multiline input is not automatically split. Existing aliases
and scripts can still deliberately expand a submitted command according to that
world's normal configuration. Text that cannot be represented in a non-UTF-8
world's encoding is refused without losing the draft.

Select an existing routing rule to edit it; **Save changes** keeps its destination
bound to the same world. Deleting a rule preserves the transcript and existing
text. Rules, prefixes and layouts use the client's portable preferences. Draft
and command history are local human input, not agent-shared unsent input.

Agent, editor and scene proposals use their separate review flows. Receiving a
proposal, switching tabs, restoring a draft or receiving routed output never
submits it through the human spawn-input handler.

Routing retains the supported text styling, links and static images already
attached to matched completed records. A separate following image-only record
has no channel text to match and is not automatically associated with the prior
line. Main/spawn views do not run the receive/trigger pipeline a second time.
The native docking migration still needs physical drag, mixed-DPI and daily-use
acceptance; this guide does not claim complete docking parity.

Agent conversations have a multiline message view and composer. **Ctrl+Enter**
posts the composer to the selected named agent's enabled conversation; **Enter**
adds a line. A ready bridge is reported separately from an unconfirmed external
runner. The compact **Options** menu retains refresh, revoke and sharing/review
actions when the pane is narrow.
