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
