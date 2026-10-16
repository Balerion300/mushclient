# Windows channel navigation acceptance - October 16, 2026

## Shipped interaction

- The existing game > own login/character > local channel hierarchy retains exact connection identities, including same-named channels belonging to different logins.
- The navigator has a visible **Add channel** action for custom output filters. It is disabled when no connection profile is open.
- Right-click a channel for **Channel settings**, a Main routing toggle, and **Arrange panes**. Connection rows also offer **Add custom channel**.
- **Separate**, **+Main**, and **Mixed** badges describe incoming routing rules, not whether the pane is docked or floating. Clicking a badge changes all rules for that channel without selecting another input destination or changing keyboard focus. It affects future output, not retained text.
- The route manager opens the selected channel directly. Channel settings explain Main versus local channels and retain literal/regex routing, outgoing prefixes, following images, appearance overrides, mention text, priority, and focus shortcuts.
- Channel rows reserve a separate activity line for unread and mention counts; mentions use the theme accent. Long counts display `999+`; full integer state remains unchanged.
- Existing **Use Main font and colors** remains the default. Explicit per-channel overrides remain available.

## Verification

The native shell fixture was built separately from the full client and run on a verified private desktop, without switching the user's desktop, accessing real profiles, opening a game connection, or posting messages.

Evidence: `C:/Users/Elio/AppData/Local/Temp/mushclient-shell-8ab2775d42654954b1380ef9618db918/`.

The fixture passed exact-target Add channel and badge mouse-message checks, preservation of input focus and native selection during toggles, accessible routing/mention labels, state-only updates without tree rebuilds, two-character same-name-channel identity, collapsed-group preservation, narrow action layout, and light/dark appearance. Existing native theme checks also verify original transcript font and selection remain untouched. `WorkspaceRouteManager.cpp` compiled separately successfully.

## Remaining boundaries

This fixture simulates native messages and checks rendered native controls. Physical mixed-DPI dragging, screen-reader behavior, and live game traffic require separate acceptance. Arbitrary RGB channel label and Main-line highlighting and notification sound selection are not added by this change; no ANSI rendering behavior is changed. Root integration owns atomic persistence of channel-wide Main routing toggles.

## Independent keyboard routing acceptance

`tests/workspace/build_cycle_input_probe.ps1` extracts the exact production `CMainFrame::CycleWorkspaceInput` body into a fictional native-control fixture. It does not change the physical keyboard state or send game input. The fixture ran on a verified private desktop and passed 13 checks: borrowed/reparented input context; runtime-character isolation; forward, backward, and wrap-to-Main navigation; Alt+Page shortcuts; AltGr pass-through; held-key and IME guards; disabled-owner/modal guard; search-field exclusion; hidden-pane exclusion; original Main input overriding legacy world cycling when channels exist; and legacy fallback when they do not.

Evidence: `C:/Users/Elio/AppData/Local/Temp/valyria-cycle-input-86541e614e8c4d31b01dbc350e89a1af/`.

This isolates production routing logic with native HWND ownership and fictional targets. It does not replace full-client acceptance of actual focus callbacks or physical keyboard/mixed-DPI behavior.

## Actual Release client shared-history acceptance

The new `tests/workspace/shared_history_client_fixture.py` ran a copied Release client with two fresh fictional worlds and a real loopback TCP receiver, confined to a verified private desktop. It passed ten end-to-end checks through the original Main `CSendView`, document alias pipeline, actual channel input message loop, and captured wire output. Main-to-channel and channel-to-Main recall both retained the canonical command; a deliberately installed alias that would modify a re-expanded canonical command proved that recall bypassed alias expansion. Prefixes appeared once. Each pane's Down navigation restored its own draft, channel activity preserved the Main draft, and the second character did not inherit the first character's history.

Evidence: `C:/Users/Elio/AppData/Local/Temp/mush-modern-acceptance-mwlw_hi9/`. Tested copied executable SHA256: `ef6b7aac5ad59cb98084506c2583d595fd5344128047dbab49a05c0dc39803a7`.

These are actual-client loopback transport checks, not live Blood of Dragons or physical-keyboard acceptance. The helper only addresses HWNDs owned by its explicit disposable candidate PID after checking its private-desktop identity. It never switches desktops or injects global keyboard/mouse input.
