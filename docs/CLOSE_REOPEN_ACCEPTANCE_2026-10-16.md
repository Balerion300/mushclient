# Windows close/reopen acceptance: October 16, 2026

The actual Debug and Release clients pass all 13 fictional two-world Main
composer checks in both tree and tabbed document modes. This tests ordinary
world closure, continued use of the remaining world, reopening the first world,
and closing it again. It does not claim physical mouse/keyboard acceptance or
an application Exit/restart test; the fixture terminates its disposable process
after collecting results.

## Corrections found during acceptance

The earlier Unicode RichEdit destruction correction no longer asserts during
ordinary world closure. Two additional defects were reproduced and corrected:

1. Reopening a world reset its requested input body height from 120 to 39 pixels.
   `CChildFrame::FixUpSplitterBar` could cache a provisional default during frame
   construction, before the named world was loaded. Caching now waits for a
   loaded, named document with initialized input-font metrics. The existing
   end-to-end regression fails before the fix and passes after it.
2. Debug reopening stopped at `afxdockingmanager.cpp:391`, where `InsertPane`
   asserts that the pane is not already registered. An empty reusable channel
   pane still belonged to the surviving world's native tab group.
   `RestoreWorkspaceDockState` docked that tab child before the general layout
   restorer could detach it, leaving inconsistent ownership. It now explicitly
   detaches the reused pane before its preliminary docking. The same Debug
   fixture passes after the correction. No assertion was suppressed, ignored,
   or automatically dismissed.

The Debug assertion was read from only the disposable candidate's dialog on its
private desktop, without attaching a debugger or sending UI input.

## Background isolation and its initial correction

The first harness attempt assigned `lpDesktop` to Python 3.11's
`subprocess.STARTUPINFO`. That Python interface ignores the extra attribute.
An independent helper detected that it was still on `Default` and refused to
inspect windows there. Those early runs must not be described as isolated:
startup was hidden, but MFC could subsequently display its own windows. All
early candidate processes were stopped, and testing paused until isolation was
corrected.

The final harness uses explicit Win32 `CreateProcessW` and a real
`STARTUPINFOW.lpDesktop`. Before launching any candidate, a separate helper must
prove its own thread desktop name matches the fresh `ValyriaQA-...` desktop.
The desktop handle requests no `DESKTOP_SWITCHDESKTOP` right; no switch API,
input injection, debugger attachment, or user-profile operation is performed.
Diagnostic helpers refuse any non-`ValyriaQA-` desktop and restrict window
inspection to the disposable candidate PID. Diagnostic failure cannot bypass
candidate cleanup. Private-desktop runs omit the fixture's optional remote
image fetch; game traffic stays on fictional loopback sockets.

The final Debug rerun records the same private desktop before and after the
test, while the input desktop remains `Default`. Each candidate runs from a
fresh temporary package with fictional worlds and preferences. Installed
clients, personal worlds, drafts, plugins, and credentials are untouched.

## Final actual-client matrix

Each directory below is under `C:\Users\Elio\AppData\Local\Temp\` and contains
`main-composer-results.json`, the desktop preflight proof, plugin receipts and
loopback wire receipts. All 13 checks pass in every row.

| Build | Document navigation | Evidence directory |
| --- | --- | --- |
| Debug | Tree | `mush-modern-acceptance-uuccrp1n` |
| Debug | Tabs | `mush-modern-acceptance-wy3t76x1` |
| Release | Tree | `mush-modern-acceptance-91s2rcg3` |
| Release | Tabs | `mush-modern-acceptance-08vs7phx` |

An additional Debug tree rerun, `mush-modern-acceptance-lvj2cik1`, passes 13/13
with explicit input-desktop before/after evidence.

The checks cover independent original Main drafts, original-owner submission
across callback activation, recursive-submit rejection, declined and cleared
commands, aliases, plugin transformation/history, scripted input height,
fullscreen, automatic multiline growth, first-world close while the other
world continues sending, persisted height at close, and height after reopen.

Final executable SHA-256:

- Release `WinRel/MUSHclient.exe`:
  `E4894E8A80724180A9CE9E0187D64ACC861AA6208E1D563824C839FA9776A191`
- Debug `WinDebug/MUSHclient.exe`:
  `FFE83A902D9E9F6D1F867CFA83550BEE3B6BF6870A2EE56BBA939F7E5F0D80A0`

Both full Win32 builds pass. The Release native docking regression also passes
all 45 functional checks plus its final PASS summary (46 PASS lines), exit 0,
on a separately verified private desktop. Evidence:
`mushclient-dock-layout-341fe93a09664000803cf5e480498cf3`.
It covers nested splits, ordered/active tabs, floating and auto-hide layouts,
late arrivals, close/reopen placement, manual arrangement preservation,
Arrange Detach/Join, native transcript identity, selection and drafts.

Reproduce the full-client matrix with
`python tests/workspace/main_composer_fixture.py --exe WinDebug/MUSHclient.exe --isolated-desktop`,
adding `--tabbed-worlds` and/or choosing `WinRel/MUSHclient.exe` as appropriate.
`run_private_desktop_probe.py` provides the same verified isolation for a built
disposable native component fixture. Neither command switches the input desktop.

Physical clicking/caret behavior, real IME, monitor/DPI transitions, screen-reader
acceptance, and sustained everyday use remain separate gates. These tests do
not replace them.
