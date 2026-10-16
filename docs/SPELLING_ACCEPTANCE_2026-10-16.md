# Inline spelling: independent native QA, October 16, 2026

The standalone Release and Debug fixtures each pass 56 functional checks plus
the final PASS summary (57 PASS lines), exit 0. These are native-control checks
on disposable private Windows desktops, using fictional text and dictionaries.
They do not use the installed client, real game connections, user dictionaries,
desktop input injection, window focus, or debugger attachment.

## Platform and test identity

A separate no-window COM probe confirmed that this Windows installation can
create `SpellCheckerFactory` from a background multithreaded apartment, supports
`en-US`, and reports the fictional misspelling `qzxvvaal`. Neither this probe nor
the feature calls the Windows-global `Add`/`Ignore` dictionary mutation APIs.

Header tested by both final fixtures:
`workspace/WorkspaceSpell.h`, SHA-256
`29E14E16C48769E29DFE29E48DFA03ABAD9BACA64652A3EDD17164B351492D77`.

Evidence under `C:\Users\Elio\AppData\Local\Temp\`:

| Configuration | Fixture directory | Executable SHA-256 |
| --- | --- | --- |
| Release | `mushclient-spell-probe-8f9148ce6e6f418a94db9d3e925b2138` | `A4F6449D918109DC9347F4852ED3460ED8CAB6D723AFEA92FDC41D773B25FFFA` |
| Debug | `mushclient-spell-probe-673cd61d0df444a685e0627a95d7e9c4` | `4EA7601BF1C4ABD0ED0FDB9B7E64B1DF9667A8AF73722BFDAC7C6D8134E8D61C` |

Each directory contains `spell-probe-results.txt`, `identity.txt`, the private
desktop preflight, process exit receipt, and only its fictional dictionary files.

## Functional coverage

- Actual native Unicode EDIT analysis on the worker, including supplementary
  characters and correct UTF-16 spelling offsets after an emoji.
- Original text, caret selection and native undo remain intact during worker
  completion and repeated underline painting.
- Shared-input-style reparenting and returning the original editor preserve
  its HWND, text and spelling state.
- Fictional personal and game word additions, disk persistence and reload;
  game words stay within their game, while personal words follow games and case.
- Optional per-game word pack enabled/disabled behavior. The fixture supplies
  its own fictional term at the actual pack location, independently of the
  shipped vocabulary list.
- Failed dictionary replacement reports an error and does not claim the word
  was saved. Large drafts explicitly pause analysis while preserving text.
- Code and URL tokens are excluded from prose spelling corrections.
- IME start/end messages retain text and permit checks to resume. This is
  message-level coverage, not physical IME composition acceptance.
- Burst edits retain the latest snapshot. Closing an editor during queued
  analysis and creating a replacement remain safe.
- Sixteen accepted queued dictionary saves survive immediate destruction of
  their editors and appear in the actual private dictionary file.
- A legacy ANSI Main-style EDIT retains supported accented text and correct
  error ranges. This does not claim arbitrary Unicode support in an ANSI edit.

## Dictionary manager UX checks

The fixture creates an actual hidden native dialog with the production
`DictionaryDialog::Procedure` and invokes its own native control handlers.
It verifies all three dictionary scopes, a unique non-Cancel scope-control ID,
invalid multi-word feedback, personal Add/Remove, Enter-to-add, scoped game
Add/Remove, preservation of other entries, a read-only bundled pack, controls
within dialog bounds, and rejection after the owner changes game context.
The optional modern styling callback runs after native controls exist.
An independent successful save in another editor cannot make this dialog's
failed save report success: the final regression verifies request-specific
acknowledgment instead of borrowing a global dictionary-change notification.

This testing found the scope combo initially used ID 2, the same value as
`IDCANCEL`. Choosing a scope would take the close-dialog branch. The corrected
combo uses ID 102, and the scope/pack tests now pass.

Independent source review also identified stale context-menu ownership, native
window destruction during subclass callbacks, silently dropped full-queue
jobs, and queued dictionary writes tied to editor lifetime. The final tested
header includes the corresponding guards, retry/rejection handling and durable
queued save processing. Tests cover the visible save-lifetime behavior; they
do not claim an exhaustive scheduler proof.

An intermediate test accidentally retained an `ifstream` on `personal.txt`,
which blocked later atomic replacement on Windows. That fixture bug was fixed
by closing the read handle immediately. Failed intermediate artifacts ending
`45462b90...` and `a0c5395d...` are not valid product persistence evidence.

## Synthetic timing

On this machine, 100 native print-client repaints took 62 ms in Release and
62 ms in Debug. A burst of 250 native text changes plus spelling refresh calls
took 15 ms in Release and 16 ms in Debug. These are total synthetic fixture
times, not per-character live-game latency or physical-device measurements.
The timing checks do not include drawing an entire visible client window.

Build with `tests/workspace/build_spell_probe.ps1 -Configuration Release`
(or `Debug`), then run its reported executable and temporary directory through
`tests/workspace/run_private_desktop_probe.py`. The fixture must never be
installed into a personal profile or used with a real dictionary folder.

Remaining acceptance: visible underline appearance across fonts/themes/DPI,
physical IME input, keyboard navigation and screen-reader behavior in the modal
manager, and sustained integrated-client performance acceptance. Those
remain distinct from the successful standalone native-control tests.

## Integrated client regression

The complete Release and Debug executables each pass the existing 13-check
two-world Main composer suite in both tree and tabbed document modes after
final spelling and appearance integration (52/52 checks total). All four runs
use the verified private desktop launcher and include the bundled pack in the
temporary runtime. Evidence under the same Temp directory above:

| Configuration | Navigation | Fixture directory |
| --- | --- | --- |
| Release | Tree | `mush-modern-acceptance-j0o907xz` |
| Release | Tabs | `mush-modern-acceptance-zffkvx4b` |
| Debug | Tree | `mush-modern-acceptance-b9osgjah` |
| Debug | Tabs | `mush-modern-acceptance-giw_ooen` |

Final Release executable SHA-256:
`1496B2879C07D05C9F04B7ECD763A5DC7ED41407E056DE5EFA821F14A6B7E0F7`.
Final Debug executable SHA-256:
`C39ACD34000C034284CE817EAA44DF68CF97B2EE5BF996CD75FC59309906AF0C`.

This proves the existing command/draft/lifetime checks remain functional in
the integrated executable; it does not substitute for visible spell UX checks.
