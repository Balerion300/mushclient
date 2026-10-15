# Valyria workspace study

Open `modern-workspace.html` from the repository, or serve the repository root
locally and open `/docs/prototypes/modern-workspace.html`. Keep the relative
`../../res/valyria-desktop.svg` asset path intact. There are no build steps,
packages, remote fonts, or framework dependencies.

The interface uses fictional profiles and local sample transcripts. It does not
connect to a game, execute scripts, run an agent, modify installed clients, or
open native windows. The only download action is an explicitly labeled fictional
log sample. Browser storage contains prototype preferences and local drafts only.

## Files

- `modern-workspace.html`: semantic app shell and dialogs.
- `workspace.css`: semantic light/dark and red/blue themes, workspace controls,
  responsive behavior, forced colors and reduced motion.
- `workspace-model.js`: browser-independent nested layout operations and
  persistence validation, with no UI or transport dependency.
- `workspace.js`: fictional presentation data and local UI interactions.
- `workspace-logging.js`: game-agnostic pre-start field collection and filename
  preview; never executes the command/script text.
- `log-field-worker.js`: disposable local sample regex parser. A one-second
  parent timeout stops expensive regexes. Direct-file mode supports fixed
  presets; custom regex requires serving the full package locally.
- `courtyard-study.svg`: original minimal courtyard illustration retained from
  the earlier study; unrelated to the authorized Valyria brand mark.
- `workspace-model.test.cjs`: focused model regression checks; run with
  `node docs/prototypes/workspace-model.test.cjs` from the repository root.

The authorized brand SVG is reused from `res/`, without modifying its geometry
or colors. See `../VALYRIA_BRANDING.md` for its provenance.

## Try these flows

1. Expand Rowan while Mira remains expanded. Write a draft in Mira / Public,
   select Rowan / Public, write another, and return. The destination and draft
   must match the selected owner. Switch games and return to verify restoration.
2. Drag Public onto Main's center to join. Drag Pages onto Main's lower edge.
   Move Staff to the right of Pages for another level of nesting. Reorder tabs
   by dropping onto another tab. Escape cancels a pending drag.
3. Use Arrange to repeat the moves without a pointer. Focus a tab and use arrows
   to switch, or Ctrl+Shift+Left/Right to reorder. Focus a split separator and
   use its orientation's arrow keys to resize it.
4. Detach a tab through Arrange. Move the in-browser preview by its header,
   resize its corner, then Rejoin. This is intentionally not an OS window.
5. Reload and inspect the saved layout/drafts. Reset layout and verify drafts,
   editor text and appearance remain. Storage is origin-specific; a file URL
   and a localhost URL do not necessarily share data.
6. Open a channel's `•••` menu and choose “Show in Main as well.” Main gains a
   labeled example from that stream. Nothing changes in a native client.
7. Open Editor, type `world.`, use Ctrl+Space, insert a completion, and edit the
   buffer. Arrows, Home/End, Enter and Escape operate the completion choices.
   Tab/Shift+Tab leave the editor; an explicit button inserts two spaces.
   Check example only recognizes configured literal review keywords.
8. Review the fixed agent proposal. Apply only one change, reject another, then
   inspect Triggers & aliases or Notes. There is no running agent or permission
   grant. Reset review status resets decisions and keeps local applied changes;
   repeated example application does not duplicate an alias, trigger or note.
9. Change the logging naming pattern and format. Enable “Gather fields before
   logging,” inspect the optional command/script, and preview a reply match.
   Named groups populate `{game_year}`, `{game_month}`, `{game_day}` and other
   custom fields. Combine those with `{date}` or the real-date component tokens.
   Try a nonmatching reply, an invalid expression and an unknown filename token:
   the UI must retain a missing/waiting/error state and disable sample download.
   The BoD `+loginfo` preset uses an explicitly illustrative response, not a
   verified live protocol. Save local preferences or download fictional sample
   output. No command/script runs and the displayed folder is not accessed.
10. Compare System/Light/Dark and red/blue, scale text, search sample output,
    use Ctrl+K, and resize to a narrow window. Open the inline courtyard image.

## Known limits

The browser sample rebuilds presentation nodes after layout changes; this is not
a production rendering strategy or a native selection-retention guarantee.
Transcript data is fixed. Completion is a small predefined list; no Lua parser
or language server is present. Drag targets are pane-relative; outer-window
docking, auto-hide, multi-monitor drag, native caption buttons and snap are not
implemented. Browser resize corners are pointer-only; pane split resizing has
a keyboard equivalent. Small dock workspaces can scroll internally. Native
accessibility and real-device mixed-DPI acceptance remain separate work.

State uses the `valyria-workspace-study-v3` local-storage key. Invalid stored
layout trees fall back to that fictional character's default layout. The study
has `connect-src 'none'`; all required resources are packaged local files.
