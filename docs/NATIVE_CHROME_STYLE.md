# Shared native workspace chrome

`WorkspaceStyle.h/.cpp` supplies the shared System, Light and Dark chrome palette,
system message font at the window's DPI, spacing helpers and original line icons.
`ApplySurface` enrolls one container; `StyleControl` explicitly enrolls one native
control. Neither operation walks descendants. Leave game transcripts, Scintilla
and plugin miniwindows under their existing typography and colors. Workflow
forms explicitly style their immediate native controls; their plain-text native
previews use the shared UI font, while Scintilla source remains independently
styled.
Returned fonts are borrowed. System message-font changes create a new cached
generation while keeping earlier borrowed handles valid for existing controls.

`StyleButton` subclasses a native pushbutton, retaining its caption, command ID,
keyboard behavior and Windows accessibility role/name/state. It adds icon/text
painting, hover/pressed/disabled feedback, visible focus and optional tooltips.
Checkboxes retain native drawing/toggling. High contrast uses system colors and
pressed highlight text. The components contain no layout animation; callers can
query the system reduced-motion preference. `FitToWorkArea` constrains a tool's
outer rectangle; callers remain responsible for responsive inner layout.

Run `tests/workspace/run_style_components_probe.ps1` for the fresh hidden native
fixture and paired light/dark bitmap. It checks native command and checkbox
behavior, keyboard focus, MSAA names/roles/disabled state, contrast, DPI spacing,
work-area fit and content-font preservation. Test-only platform adapters exercise
high contrast and changed system fonts without changing OS settings. The screenshot
is a component fixture, not an integrated client screenshot. Physical mixed-DPI,
screen-reader interaction and daily use remain separate acceptance work. Existing
native tabs and some themed control borders retain their Windows rendering.
