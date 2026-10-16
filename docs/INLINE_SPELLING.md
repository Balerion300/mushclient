# Inline spelling and dictionaries

Main and channel input use the Windows English spelling engine on a background
worker. After a short typing pause, unknown words receive red wavy underlines.
Right-click an underlined word for suggestions or to add it to a dictionary.
Nothing is replaced or sent automatically. The ordinary input buffer, native
selection, undo, command history and game connection remain in place.

**Tools > Spelling dictionaries** (also in the Workspace menu) opens the dictionary manager for the selected original world.

The right-click menu includes **Check spelling for this game**, **Use Westeros
word pack for this game**, and **Manage personal and game dictionaries**. The
manager lets you review, add and remove words. Choose Personal for words accepted
across this installation, or the named game for words shared by its character
profiles. The Westeros pack is a separate read-only bundled vocabulary. Its
selection is automatic for recognized Blood of Dragons profiles and can be
changed independently for any game. Capitalization does not require adding
duplicate dictionary entries. Apostrophes and hyphens are supported.

The game scope follows the navigator's game grouping: a configured game label,
or otherwise its server and port. Profiles grouped under the same game label
share their game dictionary. Different games retain independent vocabulary and
spelling/pack preferences. Changing a game grouping selects that group's
dictionary; it does not silently copy the previous group's private words.

Personal words and game-specific words are stored locally as UTF-8 text in the
portable `spelling` directory beside the executable. They are not added to the
Windows-wide dictionary, sent to the game, or included in clean release packages.
The bundled `spelling/packs/westeros.txt` contains vocabulary authored for this
client: setting terms, places, houses and selected established names. Player
character names can be added explicitly; the client does not collect them from
private output. Copy your private spelling directory when moving your setup.

Analysis waits at least 350 ms after a change and runs outside the input thread.
IME composition temporarily suspends markings. Results must match the current
editor revision and owning game before use. Saved actions retain their original
game identity. Dictionary writes are atomic, and accepted writes are drained on
ordinary program shutdown. Queue size, error count and dictionary sizes are
bounded. Inline analysis pauses for drafts over 16,384 UTF-16 code units rather
than making large code pastes expensive. URLs, obvious command prefixes and
lines beginning with attribute/programming commands are excluded from marking.

Windows English spelling features must be installed for the native engine.
When unavailable, the menu explains the missing engine and existing legacy
spell-check-on-send can still operate. **More input actions** retains the older
Main input context menu; its explicit legacy spell dialog uses the older Lua
dictionary. Inline spelling replaces that dialog during ordinary native input
submission, so accepted game words do not trigger a second spelling dialog.

Validation and physical-device limits are recorded separately in the spelling
and channel-appearance acceptance reports. Real IME composition and physical
mixed-DPI pointer behavior still require hands-on checks.
