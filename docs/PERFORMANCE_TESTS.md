# Windows performance investigation, October 14, 2026

This investigation uses fictional localhost worlds and synthetic images. It
never reads installed worlds, passwords, preferences or player output. Raw
receipts, process counters, symbols and trace files stay outside Git.

## Changes under test

- Maintain retained-byte totals instead of scanning the entire transcript on
  every completed paragraph.
- Prune only the rendered prefix, preserving retained selections and links.
- Stream each styled paragraph as one RTF insertion instead of formatting each
  run through separate layout and text-length queries. Use tracked record ends
  and the actual insertion cursor to avoid rescanning the retained text.
- Coalesce unread captions; defer rendering for paused and inactive native tabs.
  Activating a tab renders its bounded model without replaying triggers.
- Restore view position and redraw spawned transcripts once per received socket
  packet, including nested receive callbacks and panes created by the first route.
  Completed records, trigger processing and agent publication remain immediate.
- Preserve retained-byte accounting when moving transcripts between panes.
- Replace a loading image's rendered record in place. Adjust subsequent record
  ends, hyperlink ranges and selections instead of rebuilding the transcript.
- Disable output undo storage and keep programmatic output read-only.
- Skip redundant main-output mirror reconstruction when all its records survive
  legacy buffer pruning. Agent history replacement semantics remain unchanged.
- Follow fresh empty transcripts despite an uninitialized scrollbar range, and
  retain the end selection when an image becomes ready.
- Coalesce MFC idle menu/toolbar updates to 100 milliseconds. Phase-1 temporary
  object cleanup still runs; connection, scripting and command execution are
  not delayed by this UI cadence.

## Measurement boundaries

The component fixture measures 5,000 appended records per case, view capture and
own-pane input dispatch. It has no game transport and does not time screen paint.
Native regression cases separately check RTF escaping, Greek/astral Unicode,
newlines, styles, retained hyperlink targets, selection rebasing, deferred tab
activation and image readiness transitions.

The full-client fixture runs an optimized Win32 Release client on Windows 11,
with one UTF-8 Telnet world, two named routes and the legacy main output buffer.
It repeats 30 seconds each of plain text, styled text, static images and idle
traffic. Successful-image cases fetch the public synthetic 640 by 240 PNG from
the fork's GitHub fixture with rotating cache keys. Lua receipts record actual
decoded dimensions and image states. Startup does not request foreground activation.

The initial fixture incorrectly used loopback image URLs with the production
loader, which rejects them. Its image-phase results measure loading/failure
fallback transitions, not successful decoding. Keep these separate from the
public-image cases and the independently verified native image/pixel tests.
`--blocked-images` deliberately reproduces the rejected-URL case without
weakening the production guard.

Output latency means receipt of a batch-end marker after preceding completed
paragraphs have been processed. Input latency means a Lua `Execute` command's
socket/echo round trip, not physical keystroke-to-pixel latency or the native
human command-entry hook. Timer gaps measure GUI message-loop responsiveness.
CPU percentage is relative to one logical core, not the entire 24-thread host.
The idle phase still sends timing markers and input probes.

Private bytes, working set, GDI objects, USER objects and process handles are
sampled once per second. Repeated late-idle plateaus can show whether retained
resources stabilize; they cannot prove that every plugin or real session is
leak-free. Public fixture images involve Internet latency, but these tests do
not exercise the game's Cloudflare image delivery, TLS, Mac/Wine,
multiple monitors or extended daily use.

## Baseline findings

The previous modern preview executable has SHA256
`7c663d029cc7fa009b7bf11a480aaf9c24b4705732efbe2a1c84e8e1b8143581`.
Its component fixture needed 11.88 seconds for plain text and 27.21 seconds for
styled text. A single styled pruning operation stalled for 2.89 seconds. Paused
output still needed 2.76 seconds for 5,000 model appends. A full-client run with
two routes at 80 text lines per second exceeded the fixture's ten-second TCP
backpressure timeout in the styled phase at 53 seconds.

An intermediate optimized candidate completed a 600-second, 250-line/s offered
load with all 5,951 batches received. Late-idle private memory varied by about
0.52 MiB after warm-up, and GDI/USER counts plateaued. It nevertheless suffered
image-phase output stalls of up to 8.44 seconds: a loading-state transition rebuilt the
whole transcript. This evidence justified the in-place image replacement and
following-state fixes; it is not a soak result for the delivered executable.

Additional controlled high-rate runs exposed output backlog in both intermediate
and earlier binaries. Stopped or backpressured runs are not counted as completed
soaks. Their results must not be extrapolated into a sustained throughput claim.

## Fixedsys compared with Consolas

Two separate 60-second runs used the same optimized executable
(`aca3662906f8e8b3be9eb5199b5169745292ffb8ec978014e9ff6f6e1d45fc67`),
250 offered text lines per second, no spawned routes, and identical plain/styled
traffic. Lua confirmed both the input and output font names in each world.
Both delivered all 598 batches and 15,667 offered lines; neither fell behind.

| Measurement | Fixedsys | Consolas |
|---|---:|---:|
| Output processing latency, 95th percentile | 5.89 ms | 6.41 ms |
| Scripted command/echo round trip, 95th percentile | 2.45 ms | 2.74 ms |
| CPU, percent of one logical core | 9.49% | 9.26% |
| Peak private bytes | 9.98 MiB | 9.86 MiB |

These small differences do not establish a font performance advantage. Fixedsys
was not the cause of the spawned-pane backlog. Consolas can be chosen for its
appearance and scaling; no font preference was changed in installed worlds.
This comparison excludes native spawned transcripts, which use Consolas, and
does not measure physical typing or screen-paint latency.

Earlier multi-pane image runs exposed substantial backlog and increasing private
bytes/USER resources. A public-image diagnostic confirmed 49 ready 640 by 240
images, but timed out under backpressure at 566.8 seconds; a component fixture
also overlapped part of that run. It is not a controlled acceptance pass and
does not establish a leak: the process retained queued work and UI resources.

## First optimized candidate: component passes, sustained test fails

Release executable SHA256
`6b25c591f43f77a3633076248adc48b305b4de36235a65547700bfbfe43693b2`.
The current component regression passes Unicode/style conversion, retained
selection and links through pruning inside nested batches, image replacement,
automatic following, and deferred native tab activation. In the same component
run, 5,000 styled records took 12.44 seconds individually and 6.65 seconds in
groups of 25; plain records took 8.69 and 6.70 seconds respectively. These are
component measurements, not a sustained full-client throughput claim.

An isolated public-image full-client run requested 600 seconds at 80 offered
text lines/second with two spawned routes. It hit TCP backpressure at 176.36
seconds in the second styled phase. Thirteen images were confirmed ready at
640 by 240 pixels, none failed, and 17 left the legacy buffer before their
readiness could be recorded. After the bounded drain, only 1,553 of
1,699 sent batches completed. Output latency reached 40.99 seconds; scripted
command/echo latency reached 40.54 seconds. Peak private bytes were 48.09 MiB,
with 200 GDI objects, 234 USER objects and 671 handles. This is a failed
acceptance test. The remaining backlog prevents interpreting memory growth as
either a proven leak or a clean memory plateau.

No stress-tested replacement was installed in the user's client directory.
The changes remain experimental in the draft PR. At that point a system CPU
trace was the next diagnostic step; neither the font comparison nor those
component passes established everyday readiness. Later results follow below.

## Reproduction

Run these from the repository root:

```powershell
tests\workspace\run_performance_probe.ps1
python tests\workspace\performance_client.py --seconds 600 --rate 250 --routes 2 --label soak
python tests\workspace\analyze_performance.py "DIRECTORY_PRINTED_BY_THE_FIXTURE"
```

For a font comparison, use `--routes 0 --font Fixedsys` and `--routes 0 --font
Consolas` in separate runs with identical traffic and duration. This isolates the
legacy world's font configuration: native spawned transcripts currently use
Consolas independently. The fixture configures both output and input font names;
its scripted input probe still does not measure physical typing performance.

### Process-local sampling

`tools\build_workspace.ps1 -Configuration Release -ProfileSymbols` creates local
PDB symbols without changing Release optimization. While an owned fictional
performance fixture is running, use:

```powershell
tests\workspace\run_cpu_sample.ps1 -ProcessId FIXTURE_PID -ExpectedExecutable "EXACT_FIXTURE_EXE"
```

The sampler refuses a different executable or a path outside its fictional
fixture naming convention. It briefly suspends only that process's main thread,
walks its stack, then resumes it between samples. Counts are inclusive wall-time
stack samples, not a kernel CPU trace. Export-only module offsets are not
misrepresented as resolved function names. Do not run this diagnostic sampler
during a throughput measurement used for final acceptance.

### System CPU tracing

The current unelevated session cannot start WPR CPU recording: Windows returns
`0xc5585011`, "Failed to enable the policy to profile system performance."
Microsoft documents an administrator terminal as the first remedy:
[Windows application profiling guidance](https://learn.microsoft.com/en-us/windows/apps/develop/performance/disk-memory).

Open PowerShell with **Run as administrator**, enter this repository, and run:

```powershell
tools\cpu_trace.ps1 -Action Start
# Reproduce the slowdown for 30-60 seconds.
tools\cpu_trace.ps1 -Action Stop
```

The helper refuses to alter an existing unrelated recording and records CPU
only. It stores the ETL and its ownership marker in the local temporary
`MUSHclient-Profiling` folder, outside Git. Analyze the stopped trace with Windows
Performance Analyzer and filter to the relevant client PID. WPA is part of the
Windows Performance Toolkit; the Windows-built-in WPR does not establish that
WPA is installed. If an elevated start still fails, check the **Profile system
performance** user right for the administrator account rather than granting
profiling rights broadly.

## Elevated system trace follow-up

With user-authorized elevation, a 210-second WPR CPU recording was successfully
saved locally on October 14. The fictional client used the earlier `6b25c591...`
executable, two routed panes, and 180 seconds of 80 offered text lines/second.
This diagnostic completed 1,790 of 1,790 batches: output p95 was 33.64 ms and
scripted command/echo p95 was 12.66 ms. Private bytes peaked at 36.73 MiB and
fell to 18.14 MiB in the final drain; USER objects peaked at 167 and ended at
165. The earlier failed run of the same executable remains counterevidence to
a universal stability claim. Instrumentation, desktop/window state and timing
can affect results; the diagnostic is not an uninstrumented acceptance test.

The system ETL contains 52,929 CPU samples on threads belonging to the fictional
process. A local ETW reader matched 52,613 to captured stacks, with no lost
events reported. Matching used timestamp and thread ID against SampledProfile
events, not all context-switch/ready-thread stacks. Private application PDBs
resolved the application frames. Available inclusive stack coverage showed
`CWorkspacePane::RenderRecord` in 24.08% of matched samples, `RestoreView` in
15.47%, and application idle processing in about 1.98%. The GUI thread accounted
for 99.36% of matched samples. Inclusive counts overlap; partial user-stack
unwinding and unresolved kernel frames prevent adding these percentages or
claiming a complete breakdown. A separate process-local sample confirmed
RichEdit calls beneath the transcript renderer.

This evidence supports testing a targeted removal of redundant selection
updates: compare the current selection before setting it, use known positive
insertion positions, and retain explicit scrolling/selection restoration.
Correctness tests pass after that change. Its component run needed 5.63 seconds
for 5,000 styled records and 3.73 seconds with groups of 25; results still require
sustained full-client validation. The evidence does not justify replacing the
docking/window framework. If rendering continues to dominate after these fixes,
the next architectural option is a virtualized spawned transcript surface,
retaining docking, routing, connections and script behavior.

A heap-trace launcher targeted only the unique name
`MUSHclient-HeapFixture.exe`; Windows reported its launch canceled, so it did
not capture heap allocations or enable heap tracing. A toolkit installation was
also blocked by automatic approval review. CPU analysis used existing Windows
ETW/DbgHelp APIs instead. No kernel trace or raw desktop metadata was committed.
Heap allocation stacks are still needed to diagnose any growth that persists
after bounded scrollback has stabilized.

The traffic fixture now records visibility, minimization, size and foreground
state of only its own process's windows, without titles or other applications'
content. It also retains matching PDBs with future fictional executable copies
so later builds do not invalidate their private symbols.

## Selection-change sustained result

The selection-change Release executable is
`39e0523cb20427d7bd5230dff9a7bd121c5f21d6c33e0bb1cd41f0effb1e6c82`.
An isolated, uninstrumented 600-second run at 80 offered text lines/second with
two routed panes completed all 5,967 sent batches (43,116 offered lines). No TCP
backpressure occurred. Output processing p95 was 13.06 ms, maximum 82.55 ms;
scripted command/echo p95 was 6.47 ms, maximum 13.98 ms. Average CPU was 17.42%
of one logical core including the bounded drain. Lua confirmed 86 decoded images
at 640 by 240, none failed, and 62 left scrollback before readiness was recorded.

Late-idle private bytes across the five cycles were 34.66, 36.28, 36.46, 36.80
and 37.97 MiB. Peak private bytes were 41.27 MiB. USER resources remained around
165-166; handles were around 579-582 and fell to 569 in the final drain. Image
phases raised memory/GDI counts; subsequent text phases removed images and
lowered them. This is substantially different from the earlier backlogged run,
but the warm-cycle private-byte drift of 1.69 MiB remains unclassified without
heap allocation stacks. Do not call the client leak-free or attribute the entire
before/after difference to this change: the earlier executable also completed
the shorter instrumented diagnostic.

The owned primary window was visible, not minimized, 1,920 by 1,015 pixels, and
never foreground during this run. It did not measure physical typing, a focused
4K desktop session, multiple active panes, or higher sustained rates. The result
supports retaining the current docking framework and continuing targeted
renderer/heap investigation; it does not close the broader release gates.

## Installed toolkit and heap follow-up

The user subsequently installed Microsoft's Windows Performance Toolkit. Its
`xperf` successfully analyzed the saved CPU trace and a new elevated heap trace
of the isolated `MUSHclient-HeapFixture.exe`, using matching private symbols.
The recording completed with zero lost events/buffers. Recording stopped and
the unique executable's heap tracing setting was disabled afterwards.

The heap-instrumented 360-second fixture completed all 3,575 batches at 80
offered lines/second with two routed panes. Instrumentation increased latency;
use the uninstrumented 600-second measurements above for responsiveness.
Outstanding heap allocations at three late-idle checkpoints were 7,156.9,
8,372.7 and 9,164.5 KiB. The checkpoints retained different amounts of output,
so their difference is not a steady-state leak measurement.

Two matched picture-storage stack families through `RenderImage`, OLE storage
and RichEdit accounted for 1,457.5 KiB, or 72.6%, of the first-to-last 2,007.6
KiB increase. Their retained picture counts increased from four to seven to
nine. Transcript strings, record/run storage and RichEdit buffers also grew.
This supports retained output history, including pictures, as the main cause
of observed growth. Scripted command/echo probes do not measure human input
history. Some smaller allocations remain unclassified; this is not proof that
all allocation paths are leak-free.

The performance priority is sustained responsiveness and stability. The current
results do not justify a wholesale rendering or window-management rewrite.
Continue normal-use acceptance, especially focused typing and foreground 4K
window movement. At the user's direction, the memory investigation is closed;
further work should advance feature and real-use acceptance, with responsiveness
and stability as the criteria. Raw system traces and private analysis artifacts
remain local only.

## October 15 functional-completion regression

Candidate `b4bf0d61e7a9f9a04fd30afea8cb3d18ca5c352e24ecf92808a286c106928a52`
completed a clean 120-second actual-client test at 80 offered text lines/second,
with four configured visible split spawns, opt-in mentions and following-image
routes. The world input/output font receipt reports Fixedsys. All 1,191 batches
arrived (8,611 offered lines), with no TCP backpressure or client exit during the
requested interval. Output receipt p95 was 63.04 ms (maximum 173.03 ms); scripted
command/echo p95 was 28.90 ms (maximum 62.55 ms). Seventeen images were confirmed
ready, zero failed, and thirteen left retained history before readiness was
recorded. Builds and component fixtures were idle during this measurement.

Reproduce with `performance_client.py --seconds 120 --rate 80 --routes 4
--font Fixedsys --spawn-features --split-spawns --visible`, then run
`analyze_performance.py` on its reported Temp folder. The new opt-in fixture
flags seed isolated settings and a balanced WDL3 split forest; they never use
installed profiles. These conditions differ from the earlier two-pane tests,
so the numbers do not establish an equal-workload speed regression or gain.
They measure completed output and scripted Execute/echo, not physical typing
or display paint latency. This is a short responsiveness gate; daily use and
mixed-DPI mouse interaction remain separate. No memory investigation was resumed.

## October 15 native UI regression

Release `48573ebca09f3cdd4cdfdb7475bb1d4a3582d730314e7a40da3c4fa877c8b432`
completed the same 120-second, 80 offered text lines/second workload with four
configured split spawns, mentions/following-image routes and visible startup.
The world font receipt remains Fixedsys for input and output. All 1,190 batches
completed (8,610 offered lines), with no TCP backpressure or client exit during
the requested interval. Output p95 was 62.97 ms (maximum 177.18 ms); scripted
command/echo p95 was 9.49 ms (maximum 40.11 ms). GUI timer-gap p95 was 85.18 ms
and maximum 170.43 ms. Fourteen images were confirmed ready, zero failed, and
sixteen left retained history before readiness was recorded.

Builds and native fixtures were stopped before measurement. The timing supports
steady bounded responsiveness after the UI changes; it is not a sustained-use,
physical typing, foreground paint or statistically established speed-gain claim.
Reproduce with the preceding command and label `modern-ui`. No memory analysis
was reopened. Raw evidence remains private Temp data outside the package.

## October 16 spelling and inherited appearance sample

Release `1496b2879c07d05c9f04b7ecd763a5dc7ed41407e056de5efa821f14a6b7e0f7`
completed a quiet 30-second actual-client sample on a verified private Windows
desktop, followed by a ten-second drain. The fixture offered 80 text lines per
second with four configured split spawns, Fixedsys, mentions/following-image
settings and periodically replaced fictional unsent Main prose to exercise
debounced spelling. No user input, installed profile or game connection was used.
All builds and other fixtures were idle. The new `--isolated-desktop` flag proves
the private desktop before launching the copied client; it never switches to it.

All 299 offered batches completed, comprising 2,750 offered lines plus local
echoes. Output receipt p95 was 46.92 ms (maximum 155.03 ms); scripted Execute/echo
p95 was 17.55 ms (maximum 22.79 ms). GUI timer-gap p95 was 81.61 ms. No TCP
backpressure or client exit occurred. This short run covered the plain-text
phase only, with no image traffic. It is not a physical key-to-pixel measure,
a sustained soak, or a controlled before/after speed comparison.

Separately, the standalone final spelling fixture measured 100 native print-client
repaints in about 62 ms and a burst of 250 native text changes plus spelling
refresh in 15–16 ms total. These are synthetic totals, not per-character latency.
Native analysis and dictionary I/O remain off the input thread.

Reproduce using `performance_client.py --seconds 30 --rate 80 --routes 4
--font Fixedsys --spawn-features --split-spawns --blocked-images --spell-drafts
--isolated-desktop --label spelling-ux`. Sanitized configuration, receipt and
desktop proof remain outside Git in Temp `mush-perf-spelling-ux-a1b3cr50`.
