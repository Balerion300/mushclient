# Spelling dictionary manager UX checks — October 16, 2026

Independent review found two functional UX problems in the initial manager: the dictionary selector reused `IDCANCEL` (2), causing its selection notification to take the Close handler; and Close was the default button, so Enter from a new-word edit could close instead of adding. The owning agent corrected these, added an explicit New word label and keyboard mnemonics, and switched layout/fonts to DPI-aware sizing.

The final `tests/workspace/run_dialog_ux_probe.ps1` uses a hidden native dialog and a fictional temporary dictionary root. It verifies:

- Unique control IDs, Add word as the default button (5), changing to Game dictionary without closing, and disabled editing for the read-only pack.
- At actual 96 DPI, all measured controls stay within the dialog and all labels/buttons have sufficient measured text height.
- Under a simulated 144 DPI layout plus a conservative enlarged Segoe UI template font, the same bounds and text checks pass. The test intercepts only the header's DPI lookup; it does not change the monitor or prove physical mixed-DPI behavior.
- The desktop foreground window is unchanged. No real dictionary, profile, live connection, or desktop input is used.

Final evidence: `C:\Users\Elio\AppData\Local\Temp\mushclient-dialog-ux-3a6cda56e72441d2a587c9e97ae80956` (`dialog-ux-results.txt`, `spell-manager-native.bmp`, `spell-manager-simulated-150.bmp`).

## Hidden painting limitation

`WM_PRINT` captures layout and several native surfaces, but the hidden combo omits its selected text and the hidden edit can omit its edge/client painting. These screenshots must not be presented as complete visual proof.

The probe independently confirms the selector has selection 0, exact selected text `Personal dictionary` (19 characters) through both native text retrieval and `CB_GETLBTEXT`. The New word input exists, is enabled, has `WS_VISIBLE` in its own style and `WS_EX_CLIENTEDGE`, and has positive native bounds. The screenshot omissions therefore do not mean those controls are absent or blank in the live UI; physical rendering acceptance remains pending.
