# Modern native UI implementation brief

Branch: `codex/modern-native-ui`, based on the validated functional checkpoint
`f2ee3f6` on `codex/workspace-completion` (draft fork PR #3). The prior main-branch
milestone is merged PR #2. This UI work is separate from the functional review.

## Visual thesis

A calm native Windows reading-and-writing workspace: restrained light/dark
surfaces, clear typography, one accent, consistent original icons and minimal
chrome around the game transcript.

## Content plan

The transcript and destination-labeled composer are the primary working surface.
A compact labeled daily action bar and collapsible Worlds/Channels navigator
provide orientation. Editor, agent, scene, route and logging tools appear as
secondary working surfaces when needed. Utility labels explain status and action;
there is no marketing hero, dashboard-card grid or decorative game artwork.

## Interaction thesis

- Icon-and-text actions retain visible keyboard focus, hover/pressed feedback,
  accessible names, tooltips and meaningful disabled states.
- Navigation reveals tools through existing docking and command handlers while
  preserving the current reading position, input drafts and original destination.
- Unread, connection, logging and sharing indicators update without moving focus.
  Avoid ornamental animation or repeated transcript reflow; respect system
  reduced-animation and high-contrast preferences.

## Three-agent ownership

1. Shared styling: new lightweight native style/component files for palette,
   system-aware UI typography, DPI/spacing, original icons and reusable labeled
   buttons. Keep implementation out of broadly included headers. Define the
   shared interface promptly for the other agents.
2. Navigation/action bars: new shell components, main-frame integration and the
   Worlds/Channels navigator. Own `mainfrm.cpp/.h`, `WorkspaceFrame.h` and shell
   files; coordinate narrowly with tools owner for command dispatch. Keep current
   native layout persistence and existing menu/keyboard access. Preserve a route
   back to legacy toolbars rather than destroying saved placements.
3. Tools/settings: newer route, logging, agent, scene and editor working surfaces.
   Apply common components, clearer grouping and progressive disclosure, retaining
   source/revision/destination checks and explicit review/sharing actions. Favor
   common tasks first and keep technical details available.

The orchestrator owns project/resource wiring, cross-agent integration, serialized
builds, regression selection, packaging and the independent branch boundary.
Agents must agree shared interfaces before editing another owner's file.

## Boundaries and acceptance

Retain connection, scripting, triggers, aliases, world management, parser and
bounded text/image rendering. Chrome styling must not overwrite world transcript
fonts, ANSI colors, backgrounds or plugin miniwindows. Agent access remains off
by default; style/layout changes cannot grant access, restore tokens or send game
commands. Preserve normal human input versus reviewed editor/scene workflows.

Use independent implementation and original UI assets. No Atlantis/Valyria code,
recovered implementation, artwork or personal worlds/settings are copied. Test
with fresh fictional profiles; installed clients remain outside this work.

First prove the redesigned main-window composition, then convert individual
surfaces. Verify DPI/font sizing, minimum/narrow work areas, contrast, focus order,
keyboard access, disabled actions and light/dark consistency. Run the established
native layout, workflow, input, image and logging checks where affected, followed
by a short actual-client responsiveness check. Screenshots and fixture evidence
must identify their exact candidate. Native tests do not replace physical
mixed-monitor, screen-reader or daily-use acceptance.
