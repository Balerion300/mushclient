# Cross-platform and profile portability assessment

October 16, 2026. Architecture recommendation, not an implemented port or sync
feature. Continue shipping and testing Windows and web in parallel.

## Existing port worth evaluating first

QMud already describes itself as a Qt 6 continuation of MUSHclient with
Windows/macOS/Linux support and migration of worlds/plugins. Its current upstream
README and license were checked directly, not inferred from the older local
checkout. Evaluate it against our inline Pueblo images, docking, spell checking,
IDE, agent interfaces and plugin compatibility before independently repeating
an entire framework port. Upstream's compatibility claim is not our acceptance.

QMud is GPL-3.0, unlike our MIT engine baseline. Reusing its implementation in a
distributed derivative requires assessing GPL obligations; do not copy it into
an MIT-only codebase as if the licenses were identical. Qt's own module licenses
are a separate consideration. This assessment copies no QMud implementation.

- https://github.com/Nodens-/QMud
- https://github.com/Nodens-/QMud/blob/main/LICENSE.md

## Recommendation

Prototype a Qt Widgets shell as a separate experiment before committing to a
port. Qt provides dockable/floating widgets, while its main-window framework
supports tabbed and split docking. Audit the exact modules and distribution
obligations before choosing LGPL/commercial packaging. wxWidgets is a credible
alternative with a license exception designed for application distribution.
Tauri could reuse the web presentation through system webviews, but wrapping the
web client alone would not preserve MUSHclient's existing desktop engine.

Sources, checked October 16:

- https://doc.qt.io/qt-6/qdockwidget.html
- https://doc.qt.io/qt-6/licensing.html
- https://www.qt.io/development/open-source-lgpl-obligations
- https://wxwidgets.org/about/licence/
- https://wxwidgets.org/about/
- https://v2.tauri.app/concept/architecture/

This repository derives its shell from CMDIFrameWndEx/CDockablePane. Network,
document/view lifetime, CString, timers, scripting automation, editor handles,
GDI rendering, printing and plugins cross the framework boundary. A replacement
GUI is not a drop-in toolkit swap. Existing portable workspace policy classes
are a useful seam, but do not yet constitute a platform-independent game engine.

Proposed gates:

1. Inventory Windows/MFC dependencies and freeze observable behavior with tests.
2. Isolate connection/protocol, logical transcript and routing models behind
   platform-neutral interfaces. Keep Windows adapters running in production.
3. Prototype one connection, styled transcript/images, shared input, editor and
   split/tab docking on Windows, macOS and Linux. Measure output and typing.
4. Prove Lua/plugins and their API compatibility explicitly. COM automation,
   native plugin UI and Windows filesystem behavior need adapters or a declared
   compatibility boundary; never silently remove these capabilities.
5. Only choose the new shell after native packaging, accessibility, IME,
   mixed-DPI, persistence and long-session tests pass on each supported OS.

## One portable settings subset, distinct device layouts

The desktop should retain its complete original settings. A separate reviewed
export produces a simplified web setup; web import must not replace or strip
the desktop configuration. Current web profiles are Blood of Dragons only and
have bounded lists: 8 profiles, 256 channels, 100 rules, constrained match/action
lengths. These are validation constraints, not promises of desktop parity.

| Desktop data | Web mapping |
|---|---|
| Explicitly selected BoD login profile and display label | Local/web profile, no password |
| Named output channel and literal match | Filtered channel with stable portable ID |
| Omit from Main | includeInMain = inverse |
| Reply prefix | Exact string, within web limits; report unsupported values |
| Appearance/notification choices | Supported semantic settings; device font fallback |
| Simple alias/trigger | Translate only a proven compatible action/matcher subset |
| PCRE regex | Review compatibility with the web matcher; do not assume equivalence |
| Nested desktop docking/auto-hide/monitor geometry | Preserve desktop layout; derive a separate web/mobile layout |
| Lua, script callbacks, plugin aliases/triggers, timers, COM | Desktop-only; list as excluded, never execute on import |
| Credentials, agent grants, tokens, input drafts, transcripts/logs | Excluded by default; separate explicit workflows if later supported |

Current desktop route tables are `workspace_route_v1`,
`workspace_route_images_v1`, `workspace_input_prefix_v1` and appearance/spawn
preferences. Export using a whitelist from the running document and settings
layer; never upload the whole world XML, SQLite database or plugin folder.
Multiple route rules targeting one desktop channel need an explicit OR model or
a supported translated rule; do not silently keep only the first match.

First implementation milestone: an offline export wizard with a preview of
included settings, unsupported entries and secrets excluded, followed by import
through the web's existing validated setup flow. Validate with fictional
round-trip fixtures and malicious/oversized input. Do not read private profiles
for initial converter development.

Account sync follows file conversion, rather than bypassing it: explicit
opt-in, browser-based account authorization, no client secret in the executable,
OS-protected refresh credentials, revision/ETag conflict detection, reviewable
changes, revocation and sign-out. Retain desktop and phone layout branches.
Never let an older web subset delete desktop-only data. Schema changes on either
side must preserve unknown desktop extensions or leave the original untouched.

The web already supports bounded declarative aliases/triggers and isolated rule
evaluation. It can grow more capable; arbitrary desktop scripting and native
plugins are a separate security and compatibility undertaking. Keeping desktop
power does not require exposing those capabilities to the web account service.
