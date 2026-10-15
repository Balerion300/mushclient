# Valyria Web: one-world mobile interaction and profile contract

Status: preparatory architecture, 15 October 2026. Implementation begins after the current Valyria UI design checkpoint is settled; finishing a native Windows rewrite is not a prerequisite. This document proposes the web interface and local data contract; it does not create a web client or authorize live game access. The target is **Blood of Dragons only**. There is one fixed world context and no game rail, server browser, or arbitrary-host connection form. The new repository is intended to start private when that phase opens.

The visual thesis follows the approved Valyria direction: neutral System/Light/Dark surfaces, precise readable output, a restrained red default accent with blue as an alternative, and the authorized Fourteen Flames mark from `res/`. The app begins with the working surface and concise utility labels. Its interaction thesis is that a player always knows **which of their own characters** owns the visible stream and where the composer will send; layout changes never alter that identity. The phone experience takes Discord's familiar tree drawer and clear back path as a navigation reference, without copying its assets or implementation.

## Screens and flows

**Phone, portrait (primary layout):** top bar shows Valyria, connection state, and a button labeled “Characters and channels.” The current view header shows `Character / Channel`, a channel menu, and an optional unread count. One transcript fills the available height. A pinned composer shows `Send as Character` and the connection state above the input, with a full-size Send action. The composer remains attached to its selected character when the keyboard opens. A locally focused Notes or Activity view replaces Send with its appropriate local action. There is no tiny game rail, horizontally scrolling dock, or floating pane on phone.

**Tree drawer:** a modal navigation drawer lists the user's own character logins, each with its directly nested Main, Public, Roleplay, Pages, Staff, and user-created filtered streams. Multiple character branches can be expanded. Character rows show connected/reconnecting/offline and unread indicators. Selecting a channel closes the drawer, changes the view, restores its draft and reading anchor, and updates the composer identity atomically. The background is inert while the drawer is open. Back/Escape closes the drawer and restores focus to its opener; browser Back first closes an open drawer or tool sheet, then returns to the previous character/channel selection. A deep link may select a known local channel ID, but must not trigger connect or send.

**Phone channel actions:** the header menu offers Include in Main, Open as separate view, Mark read, Search this stream, Logging, and Arrange. Arrange opens a labeled placement sheet: choose destination group, then Join tabs / Place above / below / left / right. At phone width, this edits the saved *desktop* layout without forcing a miniature split view. A “Phone order” control reorders drawer entries separately from desktop pane geometry. Swipe navigation is optional and cannot be the only path. Long press is optional and cannot be the only path.

**Tablet:** default to one transcript plus tree drawer. At adequate width, allow an explicit “Two views” mode with a chosen secondary stream and a draggable divider that also has step buttons and keyboard control. Collapse deterministically to the current channel on rotation or keyboard intrusion; keep both views and their reading anchors in state so they restore when space returns.

**Desktop:** a permanent character/channel tree and nested pane groups follow the approved workspace design, now with the single-world rail removed. Users can join tabs, split panes, resize, and detach where the browser environment truly supports it; an accessible Arrange dialog provides every placement without precise dragging. Pane headers and the composer identify the owning character even when `Public` from two characters is visible at once. No cross-character pane move may change a stream's owner. A narrow desktop window uses the phone navigation mode instead of a compressed dock.

**First run / import:** show `Add character`, `Import Valyria setup`, and a short explanation of local storage. Import accepts a user-selected `.valyria-web.json` file, previews character count, channels, panes, rules, and conflicts, and applies only after schema validation. Import does not connect. A successful import exposes `Connect selected`, `Connect all`, and per-character connect actions. Credentials are entered or recovered through a separate, explicit connection flow; setup import never asks for a secret bundled in the file. Preserve the user's intended “load a file and easily connect” outcome by making connection actions prominent after validation.

**Connection and send:** selecting a channel does not silently connect. A connected character can send only through that character's current connection. Before Send, the button and accessible name contain the character name; a compact destination line gives the stable connection identity. If the chosen session closes, reconnects with a new connection ID, or is not writable between composition and dispatch, Send aborts and retains the draft with a clear message. Switching to another character restores that character/channel's draft. A command entered for Mira / Public cannot be transmitted through Rowan / Public even if the names match.

**Local configuration:** Character settings own channel filters, aliases, triggers, notifications, drafts, and logging preferences. A rule editor displays its owner, trigger phase, match, action, and routing effects; creating a rule from an output line preselects that line's character but requires an explicit save. Rule changes apply to one character unless the user deliberately duplicates them. Global appearance is separate. Notes and Activity are local views with no send destination.

## Stable domain model and versioned setup bundle

Use UUID-like stable IDs generated when entities are created. Labels are editable display data, never identity or routing keys. `worldId` is the constant `bod`; do not import a host address, arbitrary world, or a game grouping. The interface can display several independent **character logins** for that world. A character ID identifies saved configuration. A new `connectionId` identifies each actual socket attempt, including reconnects. A channel ID identifies a filtered output stream scoped to exactly one character. A view ID identifies its pane/tab presentation and can refer to one channel without owning its data.

Proposed portable JSON contract (illustrative TypeScript, not implemented):

```ts
type ID = string; // UUID, validated at import; never derived from a name
type WorldID = 'bod';
type Theme = 'system' | 'light' | 'dark';
type Accent = 'red' | 'blue';
type ChannelKind = 'main' | 'filtered' | 'local-notes' | 'local-activity';
type Filter =
  | { kind: 'tag'; tags: string[] }
  | { kind: 'text-pattern'; expression: string; flags: string }
  | { kind: 'event-type'; types: string[] };
type Rule = {
  id: ID; characterId: ID; kind: 'alias' | 'trigger'; enabled: boolean;
  name: string; match: string; action: RuleAction; order: number;
};
type RuleAction =
  | { kind: 'highlight'; styleId: string }
  | { kind: 'route'; channelIds: ID[] }
  | { kind: 'notify'; soundId?: string }
  | { kind: 'expand-command'; template: string };
type Channel = {
  id: ID; characterId: ID; label: string; kind: ChannelKind;
  filters: Filter[]; includeInMain: boolean; sortOrder: number;
};
type Character = {
  id: ID; worldId: WorldID; label: string; loginName?: string;
  autoConnect: boolean; channelIds: ID[]; ruleIds: ID[];
  logging: LoggingPreference;
};
type LoggingPreference = {
  enabled: boolean; format: 'txt' | 'html'; include: 'all' | 'selected';
  filenamePattern: string; preStart?: {
    mode: 'command' | 'script'; source: string; replyPattern: string;
    timeoutMs: number;
  };
};
type Pane =
  | { type: 'group'; id: ID; viewIds: ID[]; activeViewId: ID }
  | { type: 'split'; id: ID; axis: 'x' | 'y'; ratio: number; a: Pane; b: Pane };
type View = { id: ID; characterId: ID; channelId: ID };
type SetupBundleV1 = {
  format: 'valyria-web-setup'; schemaVersion: 1;
  worldId: WorldID; exportedAt: string;
  characters: Character[]; channels: Channel[]; rules: Rule[];
  views: View[]; desktopLayouts: Record<ID, Pane | null>;
  phoneOrder: Record<ID, ID[]>;
  appearance: { theme: Theme; accent: Accent; uiScale: number; outputScale: number };
};
```

The bundle is setup data, not a session snapshot. It includes characters, channels, pane topology, rules, and preferences. Drafts may be offered as an **opt-in** export extension after a separate privacy review, because they can contain sensitive IC text; the default export excludes them. It excludes transcript history, connection IDs, cookies, access tokens, passwords, secret-bearing scripts, filesystem handles, and live status. `loginName` may identify an account but is not a credential. The export path must inspect all free-text fields for obvious secret-bearing configuration and warn or omit before writing; it cannot promise to detect arbitrary secrets inside user text. No setup file or personal profile enters the repository or examples.

Import uses a parser and schema validator with size, count, depth, regex-length, and string-length limits. Reject unknown schema major versions without partial writes; apply known migrations in a transaction to a new local record. Validate unique IDs, all references, character ownership of every channel/rule/view, split ratios, active views, one fixed world, and no duplicate view in a layout. Show a dry-run summary and conflict choices (`keep existing`, `replace selected`, `import as new IDs`). Before applying, retain a restorable snapshot of the prior local setup; commit the new setup atomically and provide an Undo import action until the next deliberate setup change. Never infer ownership from labels. A legacy prototype `localStorage` value must **not** be auto-imported: it contains fictional worlds/profiles and sample strings. If a review-only migration helper is useful, offer a one-time extraction of generic theme/layout ideas only, with explicit user selection and no fictional game or character data.

## State and event boundaries

| Layer | Owns | Never owns |
| --- | --- | --- |
| Document store (IndexedDB) | Validated characters, channels, rules, desktop layouts, phone order, drafts, notes, appearance, logging preferences, bounded transcript chunks if enabled | Live sockets, auth secrets in the setup bundle, DOM nodes |
| Session manager | Per-character connection attempt, connection ID and generation, writable status, reconnect state, bounded pending sends | Pane geometry, labels as routing keys |
| Protocol/normalization adapter | Transport decoding, line/image association, immutable normalized output events, order and sequence | UI selection, DOM, layout |
| Rule and routing engine | One character's ordered filters and actions; derived stream membership and Main inclusion | Another character's rules or connection |
| Presentation store | Selected character/channel, drawer/sheet state, focus return target, per-view reading anchor, ephemeral scroll-follow state | Socket payload parsing, long transcript bodies |
| Renderer | Accessible tree, visible transcript window, panes, composer, status and tool surfaces | Sending or deciding destination from a DOM label |

The transport plan owns the final wire protocol. Its version-1 application boundary uses `worldId: 'bod'`, stable `characterId`, a fresh `connectionId` for each actual socket attempt, and a monotonically increasing inbound `seq` per connection. A `TransportEvent` adds `kind` (`connection-state`, `raw-frame`, or `protocol-error`) and `receivedAt`. A routed `OutputEvent` has `kind: 'output'`, character-scoped `channelId`, `sourceSeq`, tokenized text/image/link content, and `disposition` (`main` or `filtered-copy`). If one source event appears in Main and a separate channel, routed copies share `sourceSeq`; text and inline image remain together. A `SendRequest` carries `requestId`, `worldId`, `characterId`, `connectionId`, explicit `destinationId`, and exact text. The UI may keep `sourceChannelId` in its local intent for draft bookkeeping, but it is not part of the transport request. The session manager verifies all IDs and writable state immediately before transport dispatch, then returns a `SendResult` with `queued` or `rejected` and a reason. `queued` means accepted into the client's socket queue, not acknowledged by the game. `destinationId` names a user-visible composer target that resolves within that character, never a display label or the last selected pane. A UI `select.channel` intent carries character and channel IDs only; selection never acts as implicit send authority. One WebSocket and one ordered event stream per character preserve output order. On reconnect, `connectionId` changes; old output may remain in history under its old ID while late events cannot change new status or receive new sends.

Store immutable raw/normalized output once per character, then index derived channel membership rather than duplicating the transcript for Main and separated channels. Attach inline images to the originating output span before routing, so text and image move together. Include channel IDs on the derived event, not on raw transport payloads. Keep the renderer subscribed to small selectors keyed by character/channel/view; append visible rows without rebuilding the whole pane tree. The current prototype recreates transcript nodes after layout changes, so it is not the production long-history strategy.

## Browser logging and local data reality

The default browser log store should be IndexedDB, chunked by character and session with export by date/range/channel. Browser storage is origin-scoped, quota-limited, and usually best-effort; private browsing may be erased when the session ends. Request `navigator.storage.persist()` when a user enables long-term local logs and show the result, but a `false` result must remain usable with a visible export reminder. Check `estimate()` and handle quota/write errors by pausing logging explicitly, not silently dropping lines. Safari/WebKit documents quota, eviction, and heuristic persistence behavior. [MDN storage quotas](https://developer.mozilla.org/en-US/docs/Web/API/Storage_API/Storage_quotas_and_eviction_criteria), [WebKit storage policy](https://webkit.org/blog/14403/updates-to-storage-policy/).

Offer **Choose log folder** only when `showDirectoryPicker` exists in a secure context and the user initiates it. Request read/write access, handle cancellation/denial/revocation, and verify every write. Keep the chosen handle outside the portable setup bundle and recheck permission on later sessions. This API is limited availability and cannot be a universal mobile logging promise. Safari's origin-private file system is browser-owned storage, not a user-visible log directory. A folder selected via `<input webkitdirectory>` is for reading selected files and does not grant ongoing writes. [MDN directory picker](https://developer.mozilla.org/en-US/docs/Web/API/Window/showDirectoryPicker), [WebKit origin-private file system](https://webkit.org/blog/12257/the-file-system-access-api-with-origin-private-file-system/), [MDN directory input](https://developer.mozilla.org/en-US/docs/Web/API/HTMLInputElement/webkitdirectory).

The universal action is **Export logs** as an explicit file download. On devices where `navigator.canShare({files})` succeeds, also offer Share/Save to Files from a user gesture; always preserve a download path if sharing is unavailable or cancelled. The product must distinguish “saved in this browser,” “download offered,” “share sheet opened,” and “file written to selected folder” as separate outcomes. A successful share promise does not prove where a user saved a file. Verify real-device iOS Safari, Android Chrome, desktop Chrome/Edge/Firefox/Safari, and installed PWA behavior during implementation. [MDN Web Share](https://developer.mozilla.org/en-US/docs/Web/API/Web_Share_API), [MDN file sharing check](https://developer.mozilla.org/en-US/docs/Web/Progressive_web_apps/How_to/Share_data_between_apps), [MDN download](https://developer.mozilla.org/en-US/docs/Web/API/HTMLAnchorElement/download).

Logging preflight can preserve the prototype's game-agnostic sequence: optional authorized command/script, wait for a bounded reply match, collect named fields, finalize a filename, then start. The illustrative BoD `+loginfo` preset is not a verified live contract. For web production, the connection/rule layer must own execution and timeouts; preview parsing of untrusted patterns belongs in a worker with input limits. Missing fields, no match, timeout, or invalid pattern must leave logging visibly waiting/failed and must never create a misleading filename.

## Accessibility, responsiveness, and performance gates

- Aim for at least 44 CSS-pixel touch hit regions around drawer rows, menus, placement choices, tabs and send controls, with visible focus and text labels. Avoid placing tiny pane controls at the extreme bottom edge. Use `env(safe-area-inset-*)` for the drawer/composer and a viewport strategy that tracks the visual viewport when the keyboard opens; test without assuming one browser's resize behavior.
- Drawer and sheets have headings, modal semantics, focus trap/return, Escape and Back dismissal, and inert background. Character and channel tree disclosure state is announced; unread changes use restrained live announcements. No interaction depends on color, drag, hover, long press, or a shortcut alone.
- The transcript has a stable accessible reading order and a deliberate “new output” affordance when the reader is scrolled up. Incoming lines do not yank the reading position or keyboard focus. Live announcements are opt-in/batched to avoid screen-reader floods. Follow-output resumes by explicit button or when the reader returns to the end.
- Test System/Light/Dark, red/blue, forced colors/high contrast, 200% zoom, reduced motion, hardware keyboard, on-screen keyboard, and landscape. Put tool menus and connection state within reach without overlaying the input. Keep code/editor text sizing independent of transcript text.
- Use paged transcript storage and a virtualized visible range with measured row heights for wrapped text and images. Preserve anchored line ID and offset across prepends, channel changes, and image load. Search queries run against an index/worker and jump to a stable event ID. Bound in-memory windows; batch inserts per animation frame; avoid serializing whole history to `localStorage` or rerendering layout on every line.

## Prototype migration and implementation order

Reuse the **ideas and tests** in `prototypes/workspace-model.js`: arbitrary binary split topology, join/edge placement, all-detached recovery, ratio clamps, and validation against duplicate or cross-owner tabs. Rebuild the module with stable UUID identities, dynamic channels, versioned migration, immutable updates, and a phone projection. Reuse the semantic theme tokens and the authorized `res/valyria-desktop.svg` asset with provenance. The prototype's channel-as-filter concept, destination-visible composer, sample logging preflight state machine, keyboard Arrange equivalent, and focus/high-contrast treatment are valuable specifications.

Rewrite the fixed two-game profiles, string-concatenated `profile:channel` IDs, whole-state `localStorage`, DOM `innerHTML` transcript rendering, full node rebuilds, fictional sample output, direct-file worker special case, preview send, and sample agent/editor actions. They are suitable for a disconnected interaction study, not production authentication, transport, script execution, or long histories. Do not copy portal implementation or other client code without separate provenance and license review.

1. Freeze this bundle/event contract with the transport plan; build schema fixtures and migration/ownership validators using fictional characters only.
2. Implement the responsive shell and tree drawer against a fake event source. Prove phone Back/focus/keyboard behavior and tablet collapse before adding transport.
3. Add IndexedDB repositories and transactional import/export, then multi-character connection adapters. Demonstrate two simultaneous fictional sessions with identically named channels and forced reconnect races.
4. Add real normalized output routing, rule ownership, composer dispatch guard, and text/image association. Gate live connectivity on the transport/security design and an isolated test path.
5. Add logging capability detection, export/share/folder paths, then long-history paging and performance budgets. Test on physical mobile devices and desktop browsers before any release claim.
6. Treat the prototype's Lua editor/completion and agent review surfaces as later design references. A web editor requires an explicit web scripting sandbox, capability model, and separate compatibility decision; native Lua or plugin behavior is not implied. An agent review UI must display the exact proposed change, its character scope and authority, and require a separate permission model before any apply action; the offline study grants no web agent privileges.

## Acceptance matrix for the first integrated web slice

| Area | Required demonstration | Failure test |
| --- | --- | --- |
| One-world navigation | Only Blood of Dragons context; two own characters expanded with channels nested beneath each | Imported arbitrary world/host rejected |
| Phone navigation | Drawer selection, Back and Escape, focus return, one transcript and visible owner | Keyboard open/rotation does not hide composer or strand a sheet |
| Desktop/tablet layout | Nested joins/splits restore; phone shows current channel, tablet two-view option | Malformed/duplicate/cross-owner layout falls back without losing drafts |
| Import/export | Versioned bundle previews and restores characters, channels, panes and rules | Future major version, oversized file, dangling IDs, secret field, and conflict cause clear abort/choice |
| Multi-character connection | Two sessions with same channel labels remain independent through reconnect | Late event or stale connection ID cannot change status or dispatch |
| Correct send destination | UI and transport log show exact `characterId`/`connectionId`/`destinationId` on accepted send | Switch character or disconnect at click: retain draft and reject stale send; no cross-send |
| Rule ownership | Trigger/alias/filter runs only for its character; Main inclusion and separate channel agree | Same text from other character has no effect; image stays with originating text |
| Browser storage | IndexedDB survives reload where permitted; persistence status and quota failures visible | Private/blocked storage gives usable session mode and export path |
| Logging | Browser log, explicit download, optional feature-detected share/folder write; precise success labels | Picker denied, permission revoked, share cancelled, quota full, preflight timeout do not claim saved file |
| Long history | 100k+ mixed text/image events, search jump, anchored scroll and responsive typing on target devices | Switching tabs or adding output does not rebuild full history or steal focus |
| Accessibility | Screen reader tree/drawer/composer, keyboard Arrange, forced colors, 200% zoom, reduced motion | No action requires precision drag, hover, color only, or a gesture without a button |

Source design and prototype: [modern workspace direction](MODERN_WORKSPACE_DESIGN.md), [prototype guide](prototypes/README.md), [layout model](prototypes/workspace-model.js), [logging study](prototypes/workspace-logging.js), [brand provenance](VALYRIA_BRANDING.md). Browser API references above were checked against MDN and WebKit on 15 October 2026; implementers still need a physical-device capability run before promises about directory writing or persistence.
