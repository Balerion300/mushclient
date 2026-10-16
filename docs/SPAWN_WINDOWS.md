# Spawn windows

Open a connection profile, then use **Output channels** or **Workspace → Manage local output channels**. These transcripts filter received text on this computer; they do not join or create game channels. The route manager keeps
the original world shown at the top even when you switch to another world.

1. Choose **New**, or use **Channel example** / **Tell example** as a starting
   point. Examples must be adjusted to your game's actual output and commands.
2. Name the transcript and paste a distinctive received label into **Incoming
   text to match**. Literal matching is case-sensitive. Enable regular expressions
   only when you need them; invalid expressions stay unsaved.
3. Leave **Show matches only here** unchecked to copy matching lines while keeping
   them in Main. Check it to hide matching lines from Main's presentation.
4. Optionally enable **Include up to 4 immediately following server images**
   for a channel that sends pictures after its text. This is off by default.
5. Set an optional outgoing prefix and save. The prefix is used exactly as entered:
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
Reconnect the original world before retrying a disconnected input. Closing a world or exiting with unsent Main/channel writing asks you to **Discard input** or **Cancel**; Cancel is the default and keeps the writing open. Drafts are not automatically saved to disk.

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

Select an existing rule and choose **Options...** to configure its transcript:

- **Mention text** adds a passive unread mention badge when received text contains
  that literal text, ignoring case. It is optional and limited to 128 characters.
  Mentions cause no sound, popup or focus change. Reading/resuming or clearing the
  transcript clears its badge with the existing unread count. Local notes and
  image-only fallback text do not count as mentions.
- **Priority** ranges from -100 to 100, with a default of 0. Higher priorities
  appear first in the route list and keyboard focus order. Equal priorities use
  world identity and transcript name for deterministic navigation. Dragged tab
  and docking arrangements stay as you arranged them.
- **Focus shortcut** is optional. Use Ctrl+Alt with a letter/digit, or a modified
  function key. F4/F6/F10, Ctrl+Shift+F1-F9, existing application/world commands,
  and another saved transcript's binding are reserved. Conflicts are checked
  again when a shortcut is pressed. AltGr, Windows-key chords, active IME
  composition and modal dialogs do not move focus.

**Ctrl+Alt+PageUp / PageDown** moves backward/forward through transcript priority
order when that chord is unused by existing commands. Existing numbered pane
focus commands use the same order. Shortcuts and mention text default to empty.
Options apply to the original world and named transcript shared by its rules.

Agent, editor and scene proposals use their separate review flows. Receiving a
proposal, switching tabs, restoring a draft or receiving routed output never
submits it through the human spawn-input handler.

Routing retains the supported text styling, links and static images already
attached to matched completed records. With following images enabled on a rule,
up to four immediately adjacent image-only server records can follow its matched
text into the same transcript, within two seconds of that text. They inherit that
rule's copy/hide behavior; multiple matching rules deliver each image once per
transcript. Association stays within the original runtime world. Intervening
text, notes, omitted/plugin-cancelled records, private editor replies, missing
records, route changes, clear, disconnect or close end the association. Pictures
outside those bounds remain subject to their own ordinary routing. Main/spawn
views do not run the receive/trigger pipeline a second time.
The native docking migration still needs physical drag, mixed-DPI and daily-use
acceptance; this guide does not claim complete docking parity.

Agent conversations have a multiline message view and composer. **Ctrl+Enter**
posts the composer to the selected named agent's enabled conversation; **Enter**
adds a line. A ready bridge is reported separately from an unconfirmed external
runner. The compact **Options** menu retains refresh, revoke and sharing/review
actions when the pane is narrow.
