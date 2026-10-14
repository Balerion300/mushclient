# Human spawn input checks

`workspace/WorkspaceInputPolicy.h/.cpp` is independent of MFC, game sockets,
editor proposals and agent tools. `PrepareHumanInput` compares the original
destination against the resolved transport destination, not the active world,
then checks connected state, UTF-8, control characters and the combined prefix
and draft byte count. It returns an exact command or a useful error; it never
splits, evaluates or transmits source. The default command limit is 4096 bytes.

`InputHistory::Up(currentDraft)` saves a bounded unfinished draft when browsing
begins. `Down(currentDraft)` leaves freshly typed text untouched when not browsing
and restores the saved draft after the newest history entry. `Sent(rawDraft,
success)` changes history only after successful transport processing. Callers
clear input only after a true original-world callback. The history stores raw
drafts, not prefixed commands, so changing a prefix does not accumulate prefixes.

The focused test executable covers exact channel and tell prefixes, astral
Unicode at byte boundaries, mismatched destinations, disconnect, empty input,
newline/tab/ESC/NUL/DEL injection in either prefix or draft, overlong UTF-8,
surrogate/out-of-range encodings, consecutive deduplication, history limits,
failed-send preservation and unfinished-draft restoration after Up/Down.

Compile and run `WorkspaceInputPolicyTests.cpp` with `WorkspaceInputPolicy.cpp`
using the VS2022 x86 C++ compiler (`/EHsc /std:c++14` or later). Tests require
neither a world file nor a game account. Release objects for `WorkspaceTools.cpp`
and `WorkspaceRouteManager.cpp` were compiled independently with the project's
MFC/Win32 defines; the orchestrator serializes full application builds.

`tests/workspace/run_input_tests.ps1` runs this policy suite and the separate
world-encoding suite. The latter checks exact UTF-8 (including Windows UTF-8
system locale), Windows-1252 and rejection of unrepresentable text rather than
silently replacing it. The real human bridge invokes the original world's
`OnPluginCommandEntered` and `Execute` path with the user-typing action source.
An isolated two-world client test confirmed an Alpha prefixed Unicode command
went only to Alpha while Beta was active, an existing alias ran, and plugin
refusal retained the unsent draft without transmitting it.

A temporary native MFC fixture compiled the current route-dialog implementation
against an in-memory frame/world adapter. It exercised channel/tell examples,
exact saved prefixes, rejection of an injected multiline prefix before rule
mutation, the original-world disconnect label and all interactive control bounds
at 540×700. Its PNG was visually inspected. This verifies the native form and
validation wiring, not SQLite persistence, real game command delivery or physical
docking. The fixture, objects, executable and PNG remain ignored build artifacts.

A second temporary native fixture used the tools UI with the real agent Store
and an in-memory frame/bridge adapter. It verified disabled-access composer
preservation, explicit button and Ctrl+Enter posting, Unicode and Unix-LF/CRLF
multiline display, 380×330 compact control bounds, and revocation preserving the
unfinished composer. Own-process-only window selection and the mocked bridge
prevented game or external-agent dispatch. Its rendered PNG was visually checked;
the model retains exact text while the Win32 view normalizes line endings.
