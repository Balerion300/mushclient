# Native workflow tools

The modern native UI branch keeps the existing workflow destinations, command
IDs and confirmation boundaries. Tools use the shared system UI font, DPI
spacing, palette and original labeled icons. Scintilla source and game
transcripts keep their own typography and rendering; native review previews,
conversation text and form fields use the shared system UI font.

- **Channels & tells** leads with Channel, Tell and New actions. Examples fill
  local fields for review; they do not send anything. Original world, match text
  and reply prefix remain explicit. Regex and following-image routing are in
  Advanced routing. Constrained windows scroll and reveal keyboard focus.
- **Logging Setup** opens with normal file logging and automatic naming.
  Choosing a start script reveals its source and function fields. Automatic
  names starts with folder and filename; optional server request/reply rules
  expand separately and remain expanded for an existing request configuration.
  Constrained forms scroll in both directions, including when focus moves to
  Save. Saving settings does not start logging or transmit a command.
- **Agent access and conversation** retain grants off by default. Selection and
  access state explain unavailable actions. Conversation links lead to explicit
  sharing and the review workspace; launch details appear only after access is
  enabled.
- **Agent review** groups editor sharing, output snapshots, proposed edits,
  tasks, captures and unsent drafts. Editor rows show the friendly original
  world before the document/revision details. Narrow previews stack vertically.
- **Scene director** retains paused new scenes, explicitly shared destinations
  and the separate editor send review. Its full form scrolls on constrained
  work areas; selecting a scene, NPC or proposal enables the relevant actions.
- **PennMUSH editor** exposes New draft, Save source, Read attribute and Review
  & send. Persistent labels identify object, attribute, find and replacement
  fields. Alt+O and Alt+A focus targets; Ctrl+F remains available for search.
  Enter activates a focused tool pushbutton once. Ordinary field, multiline
  composer and Scintilla Enter behavior remains on its existing path.

## Reproducible focused acceptance

Run these from the repository with the existing Windows C++ build tools:

```
python tests/workspace/run_workflow_forms.py
python tests/workspace/run_agent_workflow_native.py
python tests/scripted_logging/run_naming_setup.py
```

The form fixture executes production window procedures with fictional frame IO
and the actual style implementation. It checks narrow route keyboard navigation,
advanced disclosure, caption width, scene row spacing/scroll reachability,
editor mnemonic/button input, grant-off behavior, keyboard selection and preview
selection retention. The workflow regression also exercises source-only recovery,
revisions, private-read publication, scene staging and exact final native review.
The logging fixture covers progressive disclosure, constrained keyboard focus,
icon caption fit, configuration validation, rejected-edit preservation and Cancel.

Each new form/logging runner writes a fresh Temp evidence folder with its own
executable hash and source hashes. Screenshots are native fictional fixture
windows. They are not packaged-client physical acceptance. Actual owned-editor
desktop private-read and scene-send acceptance remains a recorded gap because
the supported automation could not operate those owned controls; native results
must not be described as completing that desktop check. Mixed-monitor behavior,
screen-reader operation and daily use still need human acceptance.
