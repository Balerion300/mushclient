# Valyria development checkpoint: October 15, 2026

Computer-use work stopped at the user's request when they returned to the
computer. Owned fictional test windows and debugger processes were closed.
Installed clients, personal worlds, plugins and credentials were not replaced.

## Windows

PR 7 is merged into master. PR 8 contains the native character/channel workspace
and the follow-on Main input controls, preserving the original CSendView and
command engine. The latest Debug and Release executables compile successfully.

Actual computer-use verification confirmed a readable Main editor at startup,
typing and clicking the new Send button through the original plugin hooks to
only its owning fictional connection, and neighboring channel Detach / Join
with the Main input and inline image retained. Native component and independent
QA fixtures cover geometry, themes, drafts, selection, multiple original windows,
document-wide recursive submission rejection and callback/window lifetimes.

The current Main-composer work is **not release-ready**. Its actual-client suite
passes 12 functional checks through input-height persistence, but closing and
reopening a world exposed a Debug assertion during ordinary closure:

`MFC wincore.cpp:1078: ASSERT(pMap->LookupPermanent(hWndOrig)==NULL)`

The document remains mid-teardown with no original Main views. This is a
confirmed blocker; focus and COM-retention explanations were not supported by
the subsequent evidence. No assertion was suppressed and no generic Close
behavior was changed. Next work is to resolve the native destruction/subclass
chain, then rerun Debug and Release tree/tabbed close-and-reopen acceptance.

Latest built executable hashes (before any further fixes):

- Debug: `9BF1657C6DB89F1E06C258F0F50CD25B23C9C0C270D22A34F3D466E5BC5C2C5F`
- Release: `1AB0532BA7E90236BBE2F3ECF4F6EC2CFBDF59848E5EAABA2E15C19128228771`

Earlier clean portable workspace checkpoint, before the new Main-composer slice:
`C:\Users\Elio\AppData\Local\Temp\Valyria-native-pr8-b8eb439.zip`.
All 283 listed files were independently hash-verified; Lua and other required
runtime components are included, with no personal world files. It is a development
preview, not a fully accepted release or an installation into MUSHClient-New.

Physical IME, manual drag, mixed-DPI, screen-reader and sustained-use acceptance
remain open. Several legacy menus/dialogs and compact optional document tabs
also retain their previous appearance.

## Web

Private repository: https://github.com/Balerion300/Valyria-Web

Draft PR 1 uses the existing native encrypted game service:
`wss://bod.westeros.org:3001/wsclient`. Login, command round trip, clickable exits
and a real room image were checked using an owned isolated Guest session, then
the test sessions were disconnected. The production portal was not changed.

Current clean static build: `a4de33fcfd25125e2352895e40f98e52ec543480`,
`dist/valyria-web-0.1.0-dev.1-20261015T123546112Z` in Valyria-Web. All 21 manifest
entries were verified. Local preview: http://127.0.0.1:8784/ .

37 repository tests pass. Twelve browser-pointer acceptance checks, emulated
phone touch navigation, 10,000-line persistence/reload, concurrent history writes,
bounded overload/gap warnings, duplicate-free exports, modal replacement races,
and two-character mixed image/link output pass. Fictional stress traffic never
reached the game. These checks are not physical phone/IME or sustained-use proof.

The web client remains a development candidate. Physical-device acceptance,
deployment configuration and remaining feature parity are release gates. No
production deployment was performed; no personal credentials or world files
are included in either repository.
