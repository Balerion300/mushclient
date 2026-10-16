# Windows UX and performance acceptance - October 16, 2026

## Candidate and outcome

Accepted Release executable SHA-256:
`D7670F9E64AEC3121EE2B50E2D8A075236506E44E67871D7C01CD72CABCA9912`

This is the actual rebuilt client after the docking-label, reading-position resize, and cold RichEdit follow corrections. Temporary diagnostic instrumentation is absent. The bounded automated acceptance passed: all scheduled output batches completed, the client remained alive, no TCP backpressure occurred, and all four restored panes followed output without accumulating unread/mention badges while visible. No world credentials, installed user profiles, user desktop, or live game connection were used.

## Conditions

- 180 seconds plus a 10-second idle drain, fictional loopback Telnet traffic.
- Four visible panes in a restored two-by-two split, Main retained, Fixedsys verified for output and input.
- 80 data lines/second during active phases; plain, ANSI-styled, image, idle, then repeated plain/styled phases.
- Mention matching, following-image routing, periodic unsent spelling drafts, and scripted command/echo probes enabled.
- Public static fixture PNGs were loaded and decoded through the shipping image security policy. No loopback bypass was compiled into the client.
- A verified `ValyriaQA-...` private desktop hosted the visible windows. Windows' input desktop stayed `Default`; it was never switched or controlled.
- Main was configured for 5,000 retained lines; channel buffers exercised their existing bounded retention/pruning. The run offered 14,114 lines and completed 1,790/1,790 batch receipts.

## Actual follow and UX proof

Read-only snapshots of the exact Release client confirm all four panes visible with zero unread/mention badges both shortly after startup traffic and after the image phase. The receiving client was not typed into, focused, or scrolled to make this happen.

This acceptance uncovered and corrected two real follow defects rather than clearing badges artificially:

1. Plain/height-only resizing failed to restore live-end or reading position when no inline image needed resizing. A native regression failed before the correction and passed afterward.
2. Optimized cold startup temporarily reported RichEdit's default scrollbar range `0..100`, with page size zero, despite a correct end caret and visible pane. Treating these uninitialized metrics as scrollback stopped follow. The corrected predicate still rejects an earlier caret, selected text, and real measured scrollback.

The expanded native suite also passes labeled drop hit targets, all four splits and tab joining, nested/tab/auto-hide/floating layout restoration, close/reopen and out-of-order arrival, manual arrangement preservation, reading anchors and selections, four empty Fixedsys panes receiving batched Unicode, and inherited appearance changes during a batch. Separate private appearance/style fixtures passed 34 and 27 checks covering light/dark, red/blue accents, high contrast, native keyboard/accessibility geometry, and content/frame preservation.

## Latency

Overall completed-output p95: **77.4 ms**; scripted command/echo p95: **4.1 ms**. Maximum observed completed-output latency was 164.1 ms. These are batch delivery and scripted Execute/echo measurements, not milliseconds per character typed.

| Like-for-like phase | Initial p95 | Repeated phase p95 |
|---|---:|---:|
| Plain output | 87.2 ms | 101.8 ms |
| Plain scripted input/echo | 4.1 ms | 5.1 ms |
| Styled output | 56.1 ms | 80.5 ms |
| Styled scripted input/echo | 3.0 ms | 4.1 ms |

The repeated output phases were modestly slower with retained history and image/cache state, while scripted input stayed within approximately one millisecond of its initial p95. All output completed without a growing backlog. Average CPU was 36.8% of one logical core under this sustained four-pane workload. Idle-phase output/input p95 was 1.0/1.5 ms.

## Resource check

| Resource | After warmup | Peak | After drain |
|---|---:|---:|---:|
| Private bytes | 12.3 MiB | 69.5 MiB | 26.0 MiB |
| Working set | 52.7 MiB | 111.9 MiB | 68.5 MiB |
| GDI objects | 274 | 374 | 274 |
| USER objects | 310 | 314 | 312 |
| Process handles | 591 | 801 | 708 |

GDI objects returned to the initial count; memory fell substantially after image/history pruning; USER objects and handles stabilized through the drain. Retained output, decoded-image caches, and network resources account for work that did not exist immediately after startup. This short run found no progressive handle/GDI runaway or input collapse; it does not prove the absence of every possible long-duration leak.

Image receipts explicitly confirmed 14 decoded images and 0 failures. Another 16 image markers moved out of the tracked rolling-buffer position before the delayed receipt check; their readiness is unknown from that probe, and they are not counted as failures or successful decodes.

## Evidence and limits

Exact candidate run and JSON/CSV receipts:
`C:/Users/Elio/AppData/Local/Temp/mush-perf-windows-live-final-accepted-1v4qfpqm`

Key files: `config.json`, `summary.json`, `completion.json`, `resources.jsonl`, `performance.xml.csv`, `performance.xml.images.csv`, `desktop-preflight.json`, `candidate-startup-inspection.json`, and `candidate-after-images-inspection.json`.

Expanded native regression:
`C:/Users/Elio/AppData/Local/Temp/mushclient-dock-layout-8cdfd24a693041209b21c30856a774a1`

Physical mixed-DPI dragging, screen-reader use, real hardware paint timing, and natural typing remain interactive acceptance boundaries. The separate actual-client workflow suite was run by the integration owner; this report independently covers docking/style regressions and the exact Release performance/visibility test. No additional product changes are recommended from this bounded run.
