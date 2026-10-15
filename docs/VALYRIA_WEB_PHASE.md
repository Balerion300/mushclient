# Valyria Web: next-phase handoff

Requested October 15, 2026. Begin repository creation and implementation after
the current Valyria UI design checkpoint is settled. Two Sol High agents are
preparing connectivity/license and frontend/mobile plans in parallel; this is
preparatory work, not an instruction to deploy or change the live portal.

## Product boundary

Valyria Web serves Blood of Dragons only. Remove the game/world rail and
arbitrary server selection. Keep multiple independent own-character sessions,
each with a tree of channels. Main, Public, Roleplay, Pages, Staff and user-defined
filtered views share the same channel concept. A channel can be included in Main,
opened as a tab, or arranged in a pane where the device has room. Pane movement
must never change the owning connection or command destination.

Use the accepted Valyria visual language, unchanged authorized Fourteen Flames
mark, System/Light/Dark appearance, red default accent and blue alternative.
Create the new repository privately initially, with original branding/provenance
and verified dependency notices. No personal setup bundles, worlds, transcripts,
passwords, tokens or live endpoint configuration belong in source or fixtures.

## Portable local setup

A versioned setup file carries character definitions, channel/routing rules,
triggers, aliases, layout intentions, appearance and logging preferences. Import
must validate the entire bundle, preview ownership and collisions, and retain a
rollback before applying it. Importing a file does not execute scripts or open
connections. An explicit Connect action can start the selected character sessions.
Credentials are excluded from normal exports; credential persistence requires a
separate deliberate design. Use fictional profiles in every test and screenshot.

Persist local preferences and working state using appropriate browser storage.
Treat logs and profiles as user-controlled data with export/restore, not as
repository content. Folder selection and continuous writes must follow actual
browser capabilities; provide downloadable/shareable logs where direct directory
access is unavailable. Do not promise arbitrary filesystem access on every phone.

## Mobile is a primary workflow

Desktop can show nested splits; phone layouts show one active channel with fast
character/channel navigation, unread indications and a visible send destination.
Keep channel identities and drafts shared across presentations while maintaining
device-appropriate layout preferences. Provide placement menus and touch targets
instead of requiring precision dragging. Test the onscreen keyboard, safe areas,
back navigation, connection recovery, transcript reading and local log export on
real mobile browsers before calling the client mobile-ready.

## Agent responsibilities and release gate

- Connectivity agent: identify the exact MUSH Portal checkout/upstream and
  license, audit reusable gateway/protocol code, specify per-character transport
  and parser events, and define loopback protocol/reconnect tests. No code reuse
  until its applicable license and notice obligations are established.
- Frontend agent: translate the accepted interface into desktop/phone workflows,
  specify the local setup schema and ownership model, check browser storage and
  logging capabilities, and define UI/import/export/mobile acceptance.
- Root: reconcile their shared event contract and license findings, retain this
  UI checkpoint, then create the separate repository when the phase opens. Choose
  reuse by evidence; old portal dependencies or presentation are not requirements
  for the new client. Existing server connections and production portal remain
  untouched throughout preparation.

Supporting plans: `VALYRIA_WEB_CONNECTIVITY_PLAN.md` and
`VALYRIA_WEB_MOBILE_PLAN.md`. The offline workspace prototype is a design/model
reference; it is not an authenticated game transport or finished web client.
