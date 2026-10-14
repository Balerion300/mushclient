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

## Current candidate: component passes, sustained test fails

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
The changes remain experimental in the draft PR. A system CPU trace is the next
diagnostic step; neither the font comparison nor these component passes establish
everyday readiness.

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
