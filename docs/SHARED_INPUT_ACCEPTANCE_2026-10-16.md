# Background shared-input acceptance — October 16, 2026

Result: all five hidden native fixture runs and both console input suites passed. No product-source fix was required.

## Safety and identity

- Repository HEAD: `0e9d13ad90662d1087f288f2a9a0740fb4562991`.
- Native fixtures created only hidden top-level windows. They did not show, activate, or focus an application window; no desktop automation, clipboard write, live connection, or personal profile was used.
- Tests compiled the current production shared-input classes into isolated fixtures. These are component/API checks, not runs of the complete client executables.
- Full-client executable hashes were read for identity only: Release `04DC69EADBD98836A59E0842A6809D115927E9AE969AD2692C6154634D8E0170`; Debug `B0BAA61D0F51BAFCD1D49640F03F7A50BC152A077CDBB99118DED2017714CE02`. Integrated application acceptance is recorded separately by the lifecycle test owner.

## Tests run

| Test | Configuration | Behavioral checks | Result |
| --- | --- | ---: | --- |
| `tests/workspace/run_composer_probe.ps1 -Hidden` | Debug | 33 | Pass |
| Same composer probe | Release | 33 | Pass |
| `tests/qa-cycle2/run_main_composer_qa.ps1 -Hidden` | Debug | 12 | Pass |
| Same Main composer QA | Release | 12 | Pass |
| `tests/workspace/run_native_probe.ps1 -Source shared_pane_probe.cpp` | Release | 10 | Pass |
| `tests/workspace/run_input_tests.ps1` | Console policy + encoding suites | 2 suites | Pass |

The 100 native behavioral checks exclude each fixture's final PASS summary. No FAIL lines occurred. The console suites use internal assertions rather than reporting a separate check count.

## Behavior verified

- Exact target character and channel, visible exact prefix including trailing spaces, Unicode draft and command preservation.
- Shared Send dispatch reaches only the selected channel; Main and other characters are untouched. Offline and rejected sends do not dispatch; rejected sends retain the draft and feedback.
- Per-channel Alt-Up history, prefix-free stored history, independent other-character history, bounded eviction, and restoration of unfinished drafts.
- Edit menu actions address the borrowed channel editor rather than the hidden Main draft. Search editing stays in its own control. Unsupported legacy transformations cannot silently modify Main.
- Switching the borrowed editor back to Main retains its draft, selection, font, original control identity, and owning view.
- Light/dark/system theme and narrow/wide resize preserve drafts and geometry. Input heights from 24 to 240 pixels round-trip without accumulated chrome growth; collapsing input cannot leave a borrowed editor over the transcript.
- Closing a host returns a live borrowed editor to its owner before original views are destroyed. Stale view leases and released editor key routing are rejected.
- A synchronous Send callback may close its own frame. Two Main windows sharing one document refresh independently; nested submission is blocked; document leases expire when the last view closes.
- Policy rejects disconnected/wrong targets, control characters, malformed UTF-8, and byte-limit overflow. Encoding preserves UTF-8 and exact Windows-1252; unrepresentable legacy text is rejected rather than silently substituted.

## Harness change

`tests/qa-cycle2/run_main_composer_qa.ps1` now accepts `-Configuration Debug|Release` and forwards `-Hidden`. Its C++ fixture suppresses its former unconditional `ShowWindow` when that flag is present. This makes the existing narrow-window and synchronous-close checks safe to run in the background. No production code changed.

## Evidence

- Debug composer: `C:\Users\Elio\AppData\Local\Temp\mushclient-composer-45e1661f4fee410cb29eb8efa033af6e\composer-results.txt`
- Release composer: `C:\Users\Elio\AppData\Local\Temp\mushclient-composer-f72ec63ab4c24c129851c89cc9cecca1\composer-results.txt`
- Debug Main QA: `C:\Users\Elio\AppData\Local\Temp\mushclient-main-composer-qa-2c0b8c3755694f0089c80b4144082e6a\composer_qa.txt`
- Release Main QA: `C:\Users\Elio\AppData\Local\Temp\mushclient-main-composer-qa-2dda3e8e118d41f3a633580b0d8bf7cf\composer_qa.txt`
- Actual pane: `WinRel/workspace-shared-pane-probe.txt`

## Remaining physical acceptance

These checks deliberately do not prove physical click-to-focus, real typing/IME composition, screen-reader operation, mixed-DPI dragging, visible docking interactions, or a live game's trigger/plugin behavior. Those remain separate hands-on acceptance items. No broad rendering or window-management rewrite is indicated by these component results.
