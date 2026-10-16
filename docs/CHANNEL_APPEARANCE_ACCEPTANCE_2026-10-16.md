# Channel appearance inheritance — October 16, 2026

Implemented in the native Windows workspace:

- A new channel reads the owning runtime world's Main font, size, weight, character set, foreground, and background before its first output. Other open worlds retain their own independent settings.
- The existing 500 ms pane timer picks up Main appearance changes without requiring fresh game output. Unchanged settings do not rebuild scrollback.
- Channel options default to **Use Main font and colors**. Unchecking it enables a font picker and text/background color pickers. Checking it again removes the saved override, including after close/reopen.
- Overrides persist by stable world identity and channel name in `workspace_spawn_appearance_v1`, separate from existing preference tables. Save operations use a savepoint so an appearance failure cannot partially save ordinary channel options.
- Rendering uses the chosen font instead of a fixed Consolas 10. Default-derived colors follow the chosen base colors; explicit ANSI, custom/RGB colors, inverse/bright styling, links, and image metadata remain preserved.
- Live updates preserve selection and scroll anchoring. A paused pane keeps its frozen text while buffering new output; changed base colors apply to retained records when it resumes. Moving a channel retains its appearance, settings, draft, and history.

## Verification

`tests/workspace/run_channel_appearance_probe.ps1` passed **27 checks** using hidden native windows, fictional records, and an in-memory settings database. No desktop focus, visible windows, personal profile, or game connection was used.

The checks cover initial empty-pane appearance including the actual RichEdit canvas background, independent worlds, plain-text inheritance, explicit ANSI/bold and hyperlink preservation, live changes without new output, selection preservation, overrides/reset, frozen buffering, moving a channel, stable-identity persistence, invalid settings, and transactional rollback after an injected appearance-write failure.

Final repeat after spelling integration: `C:\Users\Elio\AppData\Local\Temp\mushclient-channel-appearance-83cc43d887274d83b624fce35ead2260\channel-appearance-results.txt` — 27 passed, zero failures.

The earlier shared-input native pane probe also passed its 10 checks after the initial appearance rendering changes, before the parallel spelling integration. `WorkspaceIntegration.cpp` and `WorkspaceRouteManager.cpp` then passed an isolated native syntax compilation after spelling was attached; output is in `C:\Users\Elio\AppData\Local\Temp\mushclient-appearance-syntax-a3e7b7901caa4b2c9b92ec995bc124a5`.

The new appearance fixture targets Windows 8 or later (`_WIN32_WINNT=0x0602`) because the parallel native spelling integration uses the Windows spelling interfaces. It does not redefine or disable spelling.

## Limits

The fixture compiles the real pane implementation and extracts the real schema/preference read/write methods. Its world-appearance callback supplies fictional values rather than opening a real document. Shortcut-conflict validation is stubbed because the fixture isolates appearance persistence; the existing shortcut suite remains the authority for that behavior.

This does not replace an integrated Debug/Release build or physical review of the font/color dialogs, mixed-DPI behavior, image pixels, or live gameplay. No build was installed and no user settings were changed by these tests.

## Options dialog UX follow-up

The channel options dialog was condensed from 337 to 290 dialog units. Hidden native geometry checks measured 544 px client height with Segoe UI 9 and 906 px with the conservative enlarged Segoe UI 14 stress case, versus 632/1053 px before the change. Labels and controls remain inside the dialog without measured text clipping. The shorter form keeps the footer accessible within a typical 1080 px work area under the tested enlarged-font conditions. Smaller displays and real per-monitor DPI still require physical acceptance.

Color picker buttons now identify the selected text/background color. Inherited mode displays the current owning world's colors; custom mode displays the override. The chosen font is shown in the summary.

`tests/workspace/run_options_ux_probe.ps1` verified inherited mode, enabled custom controls, reset-save semantics, and both keyboard tab orders. Inherited mode skips disabled pickers; custom mode includes Font, Text color, Background before Save and Cancel. Foreground focus remained unchanged. Final evidence: `C:\Users\Elio\AppData\Local\Temp\mushclient-options-ux-74a7caca12ea4e69b0535618b26d702f`.

The BMPs are hidden `WM_PRINT` geometry previews. Some native controls omit client/edge painting while hidden; these images do not establish complete visual acceptance.
