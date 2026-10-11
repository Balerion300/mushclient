# Stability investigation: upstream issues 82 and 87

Local Windows validation, October 11, 2026. Tests use isolated runtimes and
loopback fixtures, never personal worlds or production servers.

## Issue 82: heap corruption

The exact 279-byte input and `print(utils.utf8convert(utils.fromhex(...)))`
from https://github.com/nickgammon/mushclient/issues/82 passed in both Debug
and Release builds. Each build also passed 20,000 conversions of that input
and 20,000 conversions of all byte values 1 through 255, comparing output
against independently generated UTF-8 bytes. Empty input was checked too.
The same tests passed again after the MXP recovery change.

No corruption or assertion was reproduced. The converter is unchanged.
This is a regression exercise, not proof that every caller or environment is
free of memory errors. The later memory-growth discussion in issue 82 concerns
the Debug allocator's delayed-free flag, which this source does not enable.

To reproduce, run `tests/heap82_regression.py --exe <built executable>
--runtime <clean official distribution> --destination <fresh directory>`.
Launch the copied executable with `/noregister` and that directory as its
working directory. Inspect `heap82.xml.results.txt`. No connection is enabled.

## Issue 87: same-socket MXP recovery

https://github.com/nickgammon/mushclient/issues/87 does not establish what
negotiation bytes the affected server sends. Our fixture proves a specific
client failure mode that could account for loss of MXP after a soft restart:

1. Configure Always On and successfully parse MXP color tags.
2. The server sends IAC DONT MXP on the same TCP connection, disabling MXP.
3. The server re-offers MXP with IAC DO MXP or IAC WILL MXP.
4. The original client accepts the offer but keeps MXP parsing disabled.
   Color tags then appear literally; a hard reconnect restores parsing.

`telnet_phases.cpp` now calls `MXP_On()` when either accepted offer arrives in
Always On mode, matching the existing Query behavior. `MXP_On()` already
returns without changes if parsing is active. The usual off/on reset and
plugin callbacks apply when renegotiating after a disable.

A differential test against the original executable reproduces both DO and
WILL recovery failures. The fixed Release executable passes both, including
actual foreground color checks: recovered red text has COLORREF 255, whereas
the original executable displays raw markup in default gray (12632256). The probe
also passes server-silent output, same-socket markup without renegotiation,
Query, No MXP, On Command activation, and hard reconnect. Local Debug and
Release builds pass. This does not claim a reproduction against HavenRPG.

Run `tests/mxp_restart_mud.py`, then prepare an isolated runtime with
`tests/prepare_native_worlds.py --protocol MXP --plugin tests/mxp_restart_probe.xml`.
Launch it with `/noregister`; inspect `mxp-test.xml.restart-results.txt` and
the fixture's negotiation log. The server binds only to 127.0.0.1:18766.

## Longer inline-image resource tests

`tests/run_inline_image_soak.py` repeatedly downloads distinct loopback images,
holds 16 at a time, releases them, verifies weak-reference expiry, and records
private memory and GDI handles. Default duration is 660 seconds.

`tests/run_transcript_soak.py` runs disconnected MXP and Pueblo worlds in the
actual Release application for 600 seconds. Each world simulates five images
per second between paragraphs, trims its bounded transcript, and clears it
periodically. The normal loader retrieves the public test PNG. Private memory,
GDI and USER handles are sampled every five seconds. Use a fresh destination.

These bounded-duration checks supplement the existing backend and transcript
compatibility suites. They do not establish all-day stability or cover every
third-party plugin or Wine/macOS.

The completed native loader run processed 5,264 distinct 640x240 images across
329 cycles in 660 seconds. Every released reference expired. GDI handles were
17 while each batch was held and 1 after release in every cycle. After warmup,
released private bytes ranged from 6,873,088 to 8,470,528. The final minute's
mean was 1,149,747 bytes above the first post-warmup minute; the last two
minutes leveled off rather than continuing to grow.

Both actual-client protocol soaks completed with no failed images, simulating
5,995 image entries altogether. During the active post-warmup window (120 to
600 seconds), private bytes ranged from 7,827,456 to 8,282,112; the last minute
averaged 106,155 bytes less than the first. GDI handles ranged from 118 to 121
and USER handles from 106 to 107, without sustained growth. Clearing both
transcripts reduced memory and GDI counts further.

The longer image runs use the pre-recovery Release build (SHA256
`e7e20d0e6edde3381a6f4feea8d3bed87b55621daae30619a6e137015a2657a6`);
the subsequent MXP fix changes only accepted-option negotiation. Image loader,
transcript ownership and view code are unchanged. Heap and restart probes also
ran against the corrected application build.

Raw measurements, summaries and differential probe reports are preserved in
[test-results/2026-10-11](test-results/2026-10-11).
