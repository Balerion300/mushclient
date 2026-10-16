# Windows review fixes — October 16, 2026

Implemented after the user reviewed and authorized the report-only Windows findings. No installed client, personal profile, live game, deployment or external service was changed.

## Result

- Spelling now observes actual text/context changes after native edit processing. Left, modifiers and selection changes retain existing error ranges; Delete/typing/Undo still trigger analysis after the normal debounce.
- Closing a world checks every original Main editor and that original connection's live channel input, including borrowed editors. Unsent writing opens **Discard input / Cancel**, with **Cancel** as the default. Cancellation leaves text and ownership intact. Disconnected worlds and application Exit are covered. Drafts are not automatically written to disk.
- The route manager saves the rule, image option, reply prefix and old-rule removal in one transaction. Live routing/prefix changes are published after commit. Failure preserves the previous saved and live configuration.
- Layout forest, geometry, tab grouping and splits save together. Failed prepares/rows/commit roll back the previous complete snapshot and report the failure; users can retry.
- The navigator distinguishes **New connection...** from **Open saved profile...** and explains local profiles versus game login. **Tools > Spelling dictionaries** and the Workspace menu expose dictionary management. **Output channels** and its manager explain local filtering. The navigator remains titled **All open connections**; the rail is explicitly a game switcher rather than a filter.

## Builds and identity

Release Win32 build passed using `tools/build_workspace.ps1`. SHA-256:

`CD32DFA1A5FEC591C36FC61957717BBD9279957EE22770F8B3527A479CFD2CEC`.

Debug Win32 build passed with debug symbols and assertions enabled. SHA-256:

`9BCB7F5E943E59F6144C0973448BBB49F816A42CA197C9CE254D19FF6F100666`.

The normal Debug build linked but the legacy BSCMAKE browse-index step hit BK1520, its internal size limit. A temporary copy of the build script added `<ClCompile><BrowseInformation>false</BrowseInformation></ClCompile>` to its existing generated local MSBuild props. This disables the optional browse database, not debug symbols, runtime checks or assertions. No project/build configuration was changed in the repository. Existing OpenSSL missing-PDB linker warnings remain.

Build logs and the exact temporary workaround script are under `C:/Users/Elio/AppData/Local/Temp/Valyria-review-20261016/` (`windows-fixes-release-final.log`, `windows-fixes-debug-final.log`, `debug-build-no-browse.ps1`).

## Fresh native verification

All executed candidates/fixtures used explicit `CreateProcessW` startup desktops named `ValyriaQA-...`, with preflight evidence. No desktop switching, interactive input injection, installed profile access or real game connection was used. Fictional servers bound IPv4 loopback only. All evidence below is beneath `C:/Users/Elio/AppData/Local/Temp/`.

| Scope | Result | Evidence folder |
| --- | --- | --- |
| Production spelling controller, Release | 59 functional checks + PASS summary | `mushclient-spell-probe-0a56df6bc58f450a878f5589bc0d6124` |
| Production spelling controller, Debug | 59 functional checks + PASS summary | `mushclient-spell-probe-3ac757749fad4d2e8fb924c8f9988e60` |
| Atomic production route/layout save methods with injected prefix, old-rule delete and late layout failures | 10 checks + PASS summary | `valyria-review-settings-a2907e94ad5f464faa57d2ae37d0f02d` |
| Shell, including distinct create/open and original identity routing | 34 checks + PASS summary | `mushclient-shell-24ed8c86251140788dce0eb505c6ed79` |
| Production workflow forms, adapted fictional world/frame IO | 65 checks, zero failures | `mush-workflow-forms-us59bqg_` |
| Integrated Main composer, Release/tree | 13/13 | `mush-modern-acceptance-lsndymr9` |
| Integrated Main composer, Release/tabs | 13/13 | `mush-modern-acceptance-99mut460` |
| Integrated Main composer, Debug/tree | 13/13 | `mush-modern-acceptance-hm_1npfb` |
| Integrated Main composer, Debug/tabs | 13/13 | `mush-modern-acceptance-ab752mi7` |
| Integrated default-setting draft close/Exit, Release | 7/7 | `mush-modern-acceptance-4ll053nb` |
| Integrated default-setting draft close/Exit, Debug | 7/7 | `mush-modern-acceptance-umgqnrjr` |

The close fixtures copy the clean base package and replace only the tested executable, add two fictional worlds and a scoped test driver, and seed startup WorldList/bootstrap control rows. Default spelling and close confirmation are preserved. They verify Main Cancel, explicit Discard, another world's independent draft, channel-only Unicode input with Main empty, no warning with empty input, two distinct world prompts during Exit, cancellation preserving both drafts, and normal process exit 0 only after explicit discard for each world. `candidate-exit.json` confirms these successful Exit runs were not terminated by fixture cleanup.

Native light/dark shell previews were inspected. They show the new connection/open actions and local output channel wording within the tested layout; these previews do not establish physical-device appearance.

Intermediate failed runs remain in Temp and are excluded from the passing evidence. The initial close helper used cross-process Get/SetWindowText for EDIT controls; it now uses marshaled WM_GETTEXT/WM_SETTEXT. A later Debug/tree run exposed a pre-existing fixture file race: the Lua tick could read an action before its direct file write completed, then open a Scripting Error dialog. Both orchestrators now publish action files by atomic rename; the fresh Debug/tree rerun passes. No assertions were disabled or suppressed.

## Short integrated performance check

Final Release, 30 seconds of fictional 250 lines/second plain output, two routes, unsent spelling drafts changed once per second, then a 10-second drain; isolated desktop and local/blocked image URLs. Builds and other performance work were quiescent.

Evidence: `mush-perf-review-fixes-final-xhrx4q73/summary.json`.

- 299/299 completed batches; requested duration completed; no TCP backpressure.
- Completed-line delivery p95 45.90 ms, p99 58.40 ms, maximum 185.72 ms.
- Script Execute/echo p95 8.54 ms, maximum 11.30 ms.
- Peak private bytes 19,431,424; CPU 29.39% of one core including drain (plain phase 38.73%).

This is completed-line/script transport evidence, not screen paint, human keystroke latency, a long soak or a matched before/after speed claim. The 30-second sample covers the plain-output phase only. Standalone Release's 250 edit/refresh calls took 15 ms; Debug took 31 ms, under incidental build load, so those figures are checks of bounded behavior rather than comparative benchmarks.

## Remaining acceptance boundaries

Physical mouse docking/drag previews, per-monitor/mixed DPI, real IME composition, screen readers, subjective visual/audio quality and sustained everyday-use/long-soak acceptance remain open. The Arrange menu remains available; no claim of physical docking parity is made. The game rail deliberately retains all connections and is documented as a switcher, not an implemented filter. Main/frame branding outside the scoped findings remains unchanged.
