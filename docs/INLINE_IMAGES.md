# Static images in the transcript

This fork adds static image blocks to MUSHclient's scrolling output. Pueblo
`<img src="https://host/path/image.png">` and MXP
`<image url="https://host/path/" fname="image.png">` insert an image between
text entries. Existing text hyperlinks and command links remain available.
An image inside an existing `<a>` or `<send>` inherits that action; otherwise
clicking it opens its source URL.

Images are part of the output buffer, rather than floating miniwindows. They
scroll with surrounding text, scale down to the output width, and have a
bounded display height. Width-only or height-only requests preserve the
image's aspect ratio. An explicit width and height specify its display box.
Each split output view measures its own layout. Loading an image preserves
the reader's position or follows the bottom when the view was already there.

## Supported subset

- Static PNG, JPEG, BMP, and the first frame of GIF, including PNG transparency.
- HTTP and HTTPS sources, asynchronous loading and in-flight URL deduplication.
- Image blocks between paragraphs; no text wrapping beside images, animation,
  MXP frames, or Pueblo panes.
- Loading/failed images retain a clickable URL fallback.
- Plain text copying, HTML copying/logging, recall, and printing retain a text
  URL fallback. Logs and printed pages do not embed image pixels.
- Synthetic image entries do not fire text triggers or received-line plugin
  callbacks. They do not enter the received-text multiline trigger history.

The fork targets Windows 7 or later and uses Windows Imaging Component and
WinHTTP. The upstream project still needs Visual Studio C++, MFC, and its
existing libraries and runtime files.

## Resource handling

Four background workers download and decode images without accessing document
or view pointers. Requests are limited to 16 MiB downloaded data, 16 million
pixels and 8192 pixels per dimension; live decoded images share a 128 MiB and
512-bitmap budget. The queue and weak cache are bounded. Workers retain their
own reference so closing or clearing a transcript during a download is safe.

Only HTTP(S) URLs are accepted. File paths, credentials in URLs, literal IP
hosts, localhost and local/single-label hostnames are rejected. Redirects are
revalidated and cannot downgrade HTTPS to HTTP. These hostname checks do not
verify DNS-resolved address ranges. There is no persistent disk cache.

## Script inspection

Existing `GetLineInfo` selectors 1–13 are unchanged. Additional selectors:

| Selector | Value |
| --- | --- |
| 14 | Whether this entry is an image |
| 15 | Image source URL |
| 16, 17 | Intrinsic image width and height |
| 18 | Loading state: 0 loading, 1 ready, 2 failed; null for text |
| 19, 20 | Entry's pixel top and display height in the first output view |

An image counts as one output-buffer entry regardless of its display height.
Plugins that infer pixel positions by multiplying line numbers by font height
need the new layout selectors when images are present. `SetScroll` retains
pixel positioning and its existing special values.

## Verification

Run `python tests/run_native_tests.py` with Visual Studio C++/MFC and Pillow
installed. It compiles a separate test harness and starts a loopback HTTP
fixture. Only that harness enables `MUSHCLIENT_INLINE_IMAGE_TEST_ALLOW_LOOPBACK`;
the application project does not enable it.

The native tests cover format decoding, alpha compositing, GIF first-frame
pixels, Unicode URLs, asynchronous loading, deduplication, redirects, failed
images, byte/dimension/global-memory limits, and releasing references during
loading.

For full-client checks, start `python tests/inline_image_mud.py`, connect separate
test worlds to localhost ports 18766 (MXP) and 18767 (Pueblo), and load
`tests/inline_image_transcript.xml`. Its report is written beside the plugin.
The fixture uses this fork's public test PNG; it never contacts a live game.
It checks transcript entries, ordering, layout, link actions, trigger isolation
and scrolling. `prepare_native_worlds.py --runtime <installed-client-directory>`
can create an isolated runtime using a compiled executable and only the Lua
and English resource DLLs from an existing installation. It copies no saved
worlds, credentials, logs, or user preferences. Launch with `/noregister` from
that runtime directory. The plugin runs only in the named loopback fixture
worlds. Visually check image placement, clipping, resize, scrollback,
selection, actual command clicks, and split views before treating a build as
ready for player use.

The implementation at commit `ca2aa6e` passed GitHub Debug and Release builds
and the native image backend suite. The Release executable passed both
full-client protocol fixtures, including paused scroll position preservation,
tail deletion of image entries, buffer clearing, and output after clearing.
Native mouse checks confirmed delivery of text and image commands in both
protocols, and visual checks confirmed image placement and scrollback clipping.
The follow-up implementation at `a4ce069` fixes viewport-height invalidation,
paused/bottom resize anchoring, image selection bounds, link hits below the
transcript, and worker startup under resource pressure. Local Debug and Release
builds pass. The native backend suite reports zero failures, including drawing,
failed-image no-op, oversized URLs, and 270 sequential URLs through cache pruning.
The updated Release executable passes both original transcript suites and the
compatibility suite for each protocol separately: multiline triggers across an
image, received-line callbacks, Copy_Output-style reconstruction, actual plain
and HTML copying, miniwindows, height/width resizing, paused resize position,
bottom following, and creation of a second output window. Native visual checks
confirm width-specific rendering in both protocols' second views, independent
Pueblo scrolling/pane resizing, and mouse selection across an MXP image.

To repeat compatibility checks, run `python tests/inline_image_mud.py --compat-only`
and prepare one world at a time using `prepare_native_worlds.py --runtime
<installed-client-directory> --plugin tests/inline_image_compatibility.xml
--protocol MXP` (then `--protocol Pueblo` in a separate destination). Clipboard
tests must be serialized because Windows has one shared clipboard. Reports are
written beside the fixture plugin as `.compat-results.txt`. The fixture leaves
the transcript visible for secondary-view inspection.

This remains a preview: representative plugin interfaces were tested, not every
third-party plugin. Long everyday sessions and Wine/macOS execution have not
been validated. These tests never replace or connect the installed live client.
