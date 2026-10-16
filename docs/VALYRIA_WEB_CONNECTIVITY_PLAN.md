# Valyria Web: connection and protocol plan

Status: preparatory architecture, 2026-10-15. This document authorizes no new
repository, live connection, server change, build, deployment, or code transfer.
Start implementation only after the current desktop workspace UI checkpoint is
settled. Valyria Web is a separate Blood of Dragons web client, not a second
build of the desktop MUSHclient fork.

## Product boundary and evidence

Valyria Web has one world: Blood of Dragons. A user may open several of their
own characters at once. Each character owns a separate connection and its own
Main, Pages, Public, Roleplay, Staff, and user-defined filtered streams. Those
names describe **local output filters**, not necessarily server-side channels.
Every command goes to an explicit character connection. Profiles, routing rules,
triggers, aliases, import/export, and optional local logs belong to the browser
user and must not be shipped from any personal checkout. The interface must work
at phone sizes with touch-safe actions; the desktop prototype's game rail and
fictional multiworld examples are not part of this product.

The inspected portal checkout is `bod-portal-test`, branch
`codex/native-tls-portal-test` at `7880304`, with uncommitted local edits and
generated build changes. Its `origin` is
https://github.com/Balerion300/mush-portal; GitHub marks that repository as a
fork of https://github.com/grapenut/mush-portal. It is reference evidence, not a
clean source snapshot. `package.json` specifies React 16.6.3, React Scripts
2.1.1, and Material UI 3.6.0 ranges; no dependency upgrade or installation was
attempted. `MUSHclient-inline/docs/MODERN_WORKSPACE_DESIGN.md` supplies the
character/channel/composer identity model, while its browser prototype expressly
does not implement transport or authentication.

The current **local test customization** (`bod-portal-test/public/local.js`)
forces encrypted direct native WebSocket to a fixed BoD host, port, and
`/wsclient` path, then calls `client.connect()`. The connection is constructed in
`src/client/index.js:1230-1240` and `src/client/connection.js:50-77`. An older
portal README discusses a WebSocket proxy and arbitrary games; that is not
evidence that the present local test uses either. Historical operational notes
recorded direct native WSS at `wss://bod.westeros.org:3001/wsclient` on
2026-10-11; this document has **not** rechecked the current deployed browser
assets, served certificate, WebSocket response, or server config. Confirm the
actual current production path read-only before implementation. Do not change
transport or restart PennMUSH as part of this phase. Native TLS sessions have
previously been sensitive to PennMUSH reboot, so the future rollout must use a
separate test deployment and preserve existing player connections.

### License gate

The original grapenut project and the Balerion300 fork list no repository-wide
LICENSE/COPYING file in their public root listings; the local checkout likewise
has no explicit project-wide license. The README does encourage hosting and
modification and provides build instructions. Those instructions do not clearly
state the redistribution or relicensing terms for this proposed new client, so
reuse rights need clarification rather than an assumed MIT grant. The fork
relationship does not establish additional rights. GitHub's licensing guidance
explains the general limits of public code without an explicit license.
Individual bundled files such as
`Chargen/jHash.js` contain their own notices, but those do not license the portal
as a whole. Therefore **do not copy portal connection, emulator, UI, assets,
build output, or `jsonapi.mush` into Valyria Web, or label them MIT**, on the
present evidence. Before any code reuse, obtain an affirmative license or
permission from the relevant copyright holder(s), determine whether it covers
the upstream and fork changes, and retain the actual required copyright notices
and dependency licenses. If permission remains unavailable, implement the
documented/observed wire behavior independently with original code and
independent fixtures. The project's use as a running portal is not proof of
permission to publish a new derivative.

Primary references: [original repository](https://github.com/grapenut/mush-portal),
[fork repository](https://github.com/Balerion300/mush-portal), and
[GitHub licensing guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository).

## What the local portal actually does

`Connection` holds one browser WebSocket. Incoming messages are assumed to be
string data whose first character is a channel: `t` text, `j` JSON, `h` HTML,
`p` Pueblo tag, `>` prompt. Outgoing text is `t` + command + CRLF; outgoing
JSON is `j` + serialized object. `Client.connect()` creates a single global
`window.client`, one `conn`, one output and one login dialog. It marks login by
matching text such as `Last connect was from`; the first incoming update opens
the login dialog. `Login` sends `connect "<name>" <password>` over the socket
and clears the password field afterward. This source is a useful behavioral
inventory, not a multi-character or security-ready foundation.

`onText` runs triggers before writing text. Text uses the ANSI emulator and a
link scanner. `h` inserts HTML into a temporary DOM node; `p` builds a tag from
the incoming text and inserts it with `innerHTML`, including a generated inline
`onclick` for `xch_cmd`. `j` parses an object and emits its `gmcp` or `op` event.
The renderer recognizes native image tags and suppresses `XCH_PAGE` and image
tags with `xch_graph`; user-facing test assets add label/touch treatments around
the native output. The raw HTML/Pueblo insertion and inline command handler are
not suitable to copy as a trust boundary. Native WebSocket markup and Telnet
Pueblo are separate capabilities: BoD room guidance explicitly warns that
`pueblo(player)` may be false for a working WebSocket client and can select only
one descriptor when a player has several connections. The server should route
tags per descriptor; the web client must not infer its own capability from
`pueblo(player)`.

No portal client-side `PUEBLOCLIENT` or MXP negotiation string was found in the
inspected startup/connect path. That does **not** establish exactly what the
server negotiates or emits during first login. Capture a sanitized, explicitly
authorized test-session trace later, including opening handshake, first frames,
login, first `look`, native image and command link. Preserve frame type, order,
and channel; remove credentials and private output. The earlier direct-WSS
acceptance specifically called for binary WebSocket framing, while the local
`onmessage` assumes a string. This mismatch is a required fixture, not a reason
to guess the live wire format.

Important correctness gaps in the inspected source:

- `Connection.reconnect()` sends `QUIT` to an open socket, constructs another
  socket, and keeps callbacks on the shared object. There is no attempt token or
  stale-callback guard. `Client.onClose` schedules a reconnect timer without
  cancellation; an intentional close can therefore reopen a session.
- `sendText` silently drops commands while disconnected; `sendCommand` triggers
  reconnect and returns, so a draft can appear handled without being sent. The
  WebSocket `bufferedAmount` is not bounded. There is no explicit incoming frame,
  line, image, JSON, or transcript size budget.
- `appendHTML` and `appendPueblo` use `innerHTML` and dynamic click attributes;
  the login error also uses `dangerouslySetInnerHTML`. URLs and images need
  explicit protocol/origin/size policy. Text-link detection is best-effort and
  does not guarantee safe handling across ANSI spans.
- User scripts can run JavaScript; imported localStorage data includes settings
  and actions. `portal-storage.js` only prefixes the test deployment's browser
  keys. A Valyria Web import must use a versioned, validated schema and must not
  execute imported code or accept a server address from imported data.
- One global client and single `loggedIn` flag cannot isolate concurrent
  characters, triggers, timers, outputs, JSON events, and reconnection state.

## Proposed modules and trust boundaries

1. **Fixed BoD endpoint policy.** Compile an immutable allowlisted WSS origin,
   port, and path into the transport configuration. Revalidate it after parsing
   and before every connect/reconnect. Ignore URL query parameters and imported
   profile fields that propose hosts, paths, proxy targets, or `ws:`. No open
   WebSocket-to-Telnet proxy. If a same-origin relay is later needed, it must
   have the same fixed upstream allowlist and an explicit origin policy; it
   must not accept arbitrary destinations. Use HTTPS for the web app. Confirm
   PennMUSH's Origin handling and certificate chain in the isolated test path.
2. **Connection manager.** One socket per selected character profile. Each
   attempt gets a fresh `connectionId` and state machine
   `idle -> connecting -> awaiting-login -> authenticated -> closing/closed`
   with distinct error/retry states. Generation-guard every callback/timer;
   cancel timers on user disconnect; use capped exponential backoff with jitter
   for unintentional failure. Never auto-resend a command across reconnect.
   A reconnect starts a new login and clearly reports whether the prior
   character session closed. A tab-refresh recovery policy needs explicit
   design; no claim of server-side session resumption.
3. **Frame decoder.** Accept the server's verified string/binary frame forms,
   decode bytes with an explicit charset policy, preserve frame order, and parse
   the five channel markers above only after a bounded decode. Reject unknown,
   malformed, oversized, or invalid JSON frames with a visible diagnostic and
   without executing content. Keep raw transport events separate from parsed
   protocol events. Define limits for frames, buffered output, queues, and logs
   before implementation and exercise both overload directions.
4. **Terminal/document parser.** Convert ANSI and the supported Pueblo/MXP
   subset into inert typed tokens. Maintain parser state across frame boundaries
   and preserve text/image adjacency. Allow only known tags/attributes and
   `https:` image/link URLs from an explicit policy. Reject script, event
   attributes, unsafe schemes, CSS injection, redirects to unsafe resources,
   oversized images, and pathological nesting. Render command links as
   application actions with explicit user activation; never evaluate `xch_cmd`
   as code. Preserve `pre`/fixed-width structure and soft-wrapped prose.
5. **Per-character routing and automation.** Parse each incoming connection
   once, then apply ordered local filters to produce Main and optional channel
   copies with a shared source event ID. Pages/Public/Roleplay/Staff rules are
   editable per character and can be exported without credentials. A route can
   include text plus its associated image. Triggers/aliases must be scoped to
   one character, disabled by default on import, bounded for regex runtime and
   recursive sends, and never execute arbitrary JavaScript. Local logging is
   opt-in per character/channel and records rendered output with clear gaps,
   rotation/export controls, and no credentials.
6. **Profiles and secret handling.** Keep a stable local `characterId` separate
   from display name and socket attempt. Prefer entering passwords at login;
   if remember-login is later requested, design a dedicated secret store and
   threat model before implementing it. Never put passwords in URLs, logs,
   exports, analytics, IndexedDB transcript records, or debug events. Validate
   imports atomically and keep settings/theme/layout separate from identities.

## Event contract with the frontend

Version the internal contract from day one. IDs are opaque local UUIDs.
`worldId` is always `bod`. `characterId` persists with a local profile;
`connectionId` changes for every actual socket attempt; `seq` increases for
each inbound event on that attempt. `channelId` is scoped to its character.
`sourceSeq` ties every routed copy and image token to the original inbound
event. No event may resolve a destination from whichever character happens to
be visible.

```ts
type Base = {
  v: 1; worldId: 'bod'; characterId: string;
  connectionId: string; seq: number; receivedAt: number;
};
type TransportEvent = Base & {
  kind: 'connection-state' | 'raw-frame' | 'protocol-error';
  state?: 'connecting' | 'awaiting-login' | 'authenticated' |
          'retrying' | 'closing' | 'closed';
};
type OutputEvent = Base & {
  kind: 'output'; channelId: string; sourceSeq: number;
  tokens: Array<TextToken | ImageToken | LinkToken | CommandToken>;
  disposition: 'main' | 'filtered-copy';
};
type SendRequest = {
  v: 1; worldId: 'bod'; characterId: string;
  connectionId: string; destinationId: string;
  requestId: string; text: string;
};
type SendResult = {
  requestId: string; status: 'queued' | 'rejected';
  reason?: 'stale-connection' | 'closed' | 'invalid-target' |
           'invalid-text' | 'backpressure';
};
```

`destinationId` names a user-visible composer target but must resolve within
the supplied character, with an approved command prefix when applicable.
`queued` means accepted into this client socket's send queue, not acknowledged
by the game. The transport must reject a stale `connectionId`, even when the
same character has already reconnected. The UI should keep the draft on every
rejection. Authentication events must never carry the password or full connect
command. Parser and routing code can be tested behind this contract before a
real server is contacted.

## Offline fixtures and acceptance checkpoints

Build synthetic fixtures without personal transcripts. Include: text/string and
binary frames; ANSI split across frames; CR/LF variants; valid/invalid UTF-8;
five channel markers; JSON op/gmcp and malformed JSON; first-login prompt and
failed login; Pueblo command links, ordinary HTTPS links, inline image, pre
block, plain-text fallback, rejected tags/attributes/URL schemes; text and image
arriving in adjacent frames; duplicate channel labels for two simultaneous
characters; trigger-created filtered copies; socket close during a pending
send; stale callback after reconnect; output flood/backpressure; oversized
image and frame; imported settings attempting an alternate server. Assert no
cross-character routing, no implicit send target, ordered rendering, and no
credential/transcript leak through logs or export.

Implementation checkpoints, after the desktop UI gate:

1. Establish a **new separate repository** and its own license for wholly
   original Valyria Web code. Record the portal permission outcome and a
   dependency/license inventory. Freeze the event contract with the frontend
   workstream. Preserve the existing portal as rollback.
2. Implement the fixed BoD policy and one-character offline transport/parser
   slice. Run adversarial frame and DOM safety fixtures. Confirm current
   server wire behavior via a sanctioned isolated test path, without disrupting
   production players.
3. Add two simultaneous character sessions, per-character routing, explicit
   composer targets, reconnect identity guards, and bounded queues. Prove two
   same-named channels cannot cross streams or send paths.
4. Add validated local profiles, rules, triggers/aliases, import/export, and
   optional local logging. Test clean browser storage, corrupt imports, and
   storage quota/eviction behavior. Keep credentials out of persistent output.
5. Complete phone/tablet/desktop interaction and accessibility checks, then
   test actual first login, Pueblo/MXP image and command links, prompts,
   reconnect, and sustained output on the isolated BoD path. Validate on
   physical mobile devices as well as desktop browsers. Compare plain-client
   output so server-generated markup does not replace required prose.
6. Only after review, plan a reversible production cutover: versioned assets,
   separate URL, tested rollback, read-only health evidence and a player-safe
   window. No PennMUSH reboot or proxy replacement is implied by a web-client
   deployment.

Open questions to resolve with observed evidence: exact currently served
transport and frame type; first-login capability negotiation; whether BoD's
JSON API is installed/enabled and which events are available; charset for all
server output; approved image origins and redirect behavior; browser storage
retention policy; whether any portal copyright holder grants source reuse.

Evidence files inspected: `bod-portal-test/README.md`, `docs/README.md`,
`package.json`, `public/local.js`, `src/client/{connection,index,emulator,
portal-storage}.js`, `src/modules/Login/index.jsx`, and
`Blood-of-Dragons/docs/BUILDING_ROOM_WORKFLOW.md`. No credentials, personal
profiles, or transcript data were read or copied.
