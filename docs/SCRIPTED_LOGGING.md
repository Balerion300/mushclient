# Optional scripted log names

**Workspace > Logging setup for active world** can select normal logging, a
world script function, or a plugin function for the
normal Start Logging action. A world function is called without arguments. A
plugin function is called through `CallPlugin` with the string `"start"`. Custom
scripts choose their own formats and workflow.

The optional `plugins/Scripted_Log_Names.xml` provides a configurable **text**
workflow. Install and enable it in the intended world, open Logging Setup, and
choose **Set up automatic file names**. Choose an existing folder and file name,
then **Use settings** and **Save**. This selects the helper's `StartLog` function
for you. Save the world to retain that selection. Its default uses the configured
log folder (`GetInfo(58)`, resolving relative folders against the client folder),
world name, local date, safe local time, and a number:

    Fictional World-2026-10-14-19-30-00-001.txt

Installing, enabling, or connecting performs no request and opens no log.
Normal logging keeps the existing logging/options and filename dialogs. This
start action applies to manual Start Logging; automatic connection logging is
unchanged. A missing or failing selected script reports the problem and does not
fall back to creating a guessed file. Existing `OpenLog` semantics are unchanged;
`OpenLogExclusive(filename)` is the separate atomic, no-overwrite API.
HTML/raw logging must be handled by a custom start script; this helper reports
those modes without changing them.

## Configuration

The automatic file names form configures ordinary naming without editing XML or
Lua. Leave the request empty for names based on the world and local clock. To
collect server details, enter one request command, the regex marking the start
of its reply, and the regex marking its end. Add up to 16 captured fields using
the field name, matching regex, capture number or named group, value format,
and Required checkbox. **Add / update field** keeps a rule and clears the entry
boxes for another; selecting a rule lets you edit or remove it. Changes to the
current rule are also kept when you select another rule or use the settings.

For example, a reply line `Year: 0178` can use field `game_year`, match
`^Year: (\d+)$`, capture `1`, and **Unsigned number**. A file name
`{game_year}-{number}.txt` then produces `178-001.txt`. **Two or more digits**
pads day/month numbers; **Month name to 01-12** handles full English month names.
Choose **Keep the entire reply block** to place the received metadata at the
start of the log.

Use settings prepares the form; the outer Logging Setup **Save** validates and
saves the rules in that world's helper plugin state. Cancel leaves saved
settings unchanged. Invalid PCRE or failed state writes leave the previous
configuration active and do not change the logging hook. Cancel a pending log
start before changing its configuration. Saving, installing, connecting and
opening these forms send no request and open no log. Only a later manual
**Start Logging** runs the selected request.

The form uses the client's existing local script character set and rejects
unrepresentable text. Saved settings are bounded, hex-encoded data, never Lua
code. Custom scripts and the optional game preset remain available through the
advanced source/function controls. The form currently configures the generic
helper, leaving preset and custom script configuration independent.

For advanced configuration, edit the `ScriptedLogConfig` table between the
marked comments in a copy of the plugin. Saved form settings override this
default table in the generic helper. The folder must already exist and be
absolute. Templates name files
inside that folder; slash, backslash, control characters, Windows device names,
and other unsafe filename characters cannot turn parsed values into paths.
Empty or oversized filenames fail visibly.

| Setting | Meaning |
| --- | --- |
| `folder` | Existing absolute output directory. |
| `template` | Filename with `{field_name}` macros. |
| `request_command` | Optional single server command, sent only by the selected start action. |
| `start_regex`, `end_regex` | PCRE delimiters identifying the complete metadata block, including its header and closing line. Both are required with a request. |
| `fields` | Array of `{name=..., regex=..., capture=1, transform=...}` rules. Captures may also use a PCRE named capture. |
| `required` | Array of field names that must be present before a file is created. Template fields also must be present. |
| `preserve_header` | Write the entire delimited metadata block as the first log content. Default false. |
| `timeout` | Positive seconds, at most 30. Fractional seconds are supported. |
| `max_records`, `max_bytes` | Collection limits, at most 100 complete received records and 65,536 bytes. All pending records, including unrelated lines and deferred tail, count. |
| `collision_attempts` | Maximum exclusive creation attempts; default 100, at most 1,000. |

Built-in macros are `{world}`, `{real_date}` (`YYYY-MM-DD`), `{real_time}`
(`HH-MM-SS`), and `{number}` (starting at `001`). Real clock macros capture the
start-action time. Explicit field rules with those names replace their built-in
values: missing reply data fails instead of falling back to the local clock.
`number` is reserved for collision handling. If the template omits it, collisions
add `-002`, `-003`, and so forth before the extension.

Rules use the client's PCRE engine, **not Lua string patterns**. Use Lua long
strings, for example `regex = [[^Year:\s*(\d+)$]]`, to preserve backslashes.
Supported transforms are `number` (remove leading zeroes), `pad2` (at least two
digits), `month_number` (full English month name to two digits), or a Lua
function. Functions can validate game-specific ranges and return a filename
value; raising an error visibly aborts the attempt. Conflicting captures fail.
There is no built-in game calendar or inferred year.

The helper receives decoded complete lines in the originating world's
`OnPluginLineReceived` callback, leaving normal display untouched. It waits
until the next native `OnPluginTick` to create the file. Complete lines received
after the closing delimiter and before that tick are buffered and written once,
even when `preserve_header` is false. Normal subsequent lines use the world's
usual log settings. The complete header and buffered tail do not depend on
transcript routing or a spawned window.

Pressing Start again while waiting cancels without sending another command.
`CallPlugin("7c1e480cf1d24cb28f9a1062", "CancelLog", "")` also cancels. Timeout,
disconnect, plugin disable/close, a changed world identity, and collection-limit
failures invalidate pending data. Late lines cannot complete an inactive
attempt. If the server supplies no request identifier, an old reply arriving
after a later fresh start is indistinguishable from the new reply; allow a
cancelled request to finish before starting again.

`OpenLogExclusive(filename)` atomically creates a new file and never truncates
or appends to an existing file. The original `OpenLog(filename, append)` API
retains its existing behavior. This helper retries readable existing filename
collisions; an unavailable directory or other creation failure is reported.
Missing metadata, invalid rules, exhausted names, and write errors are visible.

## Optional Blood of Dragons preset

This is an editable example based on the local `+LogInfo.muc` format. Confirm its
delimiters against your server's actual reply before configuring it. The
metadata command runs only when you explicitly select Start Logging with this
configuration. The native client contains no Blood of Dragons parser.

The ready-to-load optional preset is
`plugins/examples/Blood_of_Dragons_Log_Names.xml` (plugin ID
`c2d903ce670f4e018bca7f66`). Load either the generic plugin or this preset in
the intended world, choose its friendly name and `StartLog` in Logging Setup,
and save the world. The preset sends `+loginfo` only after the selected manual
start action; installation and connection perform no action.

Its editable configuration is:

```lua
local default_folder = GetInfo(58)
if not default_folder or default_folder == "" then default_folder = "logs" end
if not default_folder:match("^%a:[/\\]") and not default_folder:match("^\\\\") then
  default_folder = GetInfo(66):gsub("[/\\]+$", "") .. "/" .. default_folder
end
local ic = [[^IC-Date: Day (\d+) of Month (\d+), (\d+) AC\.(?:\s+.*)?$]]
local rl = [[^RL-Date: ([A-Za-z]+) (\d+),\s*(\d+)\.(?:\s+.*)?$]]
ScriptedLogConfig = {
  folder = default_folder,
  template = "{game_year}-{game_month}-{game_day} - {real_year}-{real_month}-{real_day}.txt",
  request_command = "+loginfo",
  start_regex = [[^=+<Log Info>=+$]],
  end_regex = [[^={20,}$]],
  preserve_header = true,
  required = {"game_year","game_month","game_day","real_year","real_month","real_day"},
  fields = {
    {name="game_day",   regex=ic, capture=1, transform="pad2"},
    {name="game_month", regex=ic, capture=2, transform="pad2"},
    {name="game_year",  regex=ic, capture=3, transform="number"},
    {name="real_month", regex=rl, capture=1, transform="month_number"},
    {name="real_day",   regex=rl, capture=2, transform="pad2"},
    {name="real_year",  regex=rl, capture=3, transform="number"},
  },
  timeout = 30, max_records = 100, max_bytes = 65536,
  collision_attempts = 100,
}
```

An IC line `IC-Date: Day 18 of Month 4, 178 AC. (about ...)` and an RL line
`RL-Date: October 14, 2026. (...)` produce
`178-04-18 - 2026-10-14.txt`. Both dates come from that reply. A collision produces
`178-04-18 - 2026-10-14-002.txt`. The complete delimited header includes any
Participants, Locations, Comments, and Summary lines; it is not reconstructed
from just the two date lines.

## World scripts and tests

`lua/scripted_log_start.lua` is a reusable Lua 5.1 module. `new(config, host)`
accepts a world-bound host with `compile`, `now`, `world_id`, `world_name`,
`connected`, `is_open`, `send`, `open_exclusive`, `exists`, `write`, `notify`,
and `ok`; optional `timestamp`, `text_mode`, and `close` complete the adapter.
The bundled plugin shows the native bindings. `compile(pattern)` returns a
function returning a capture table on match or nil. Feed complete received
lines to `on_line`, call `tick` on the client tick, and call `cancel` on
disconnect. A world start function can call `starter:start()` without arguments.
World scripts must arrange equivalent receive/tick/disconnect callbacks; simply
calling `start` cannot collect an asynchronous metadata reply by itself.

Run `tests/scripted_logging/run_helper_tests.ps1` for standalone native Lua 5.1
and the client's PCRE engine. It uses fictional host adapters and existing
runtime/build dependencies; it does not run the integrated client, open
personal worlds, or contact a game server.
`tests/scripted_logging/naming_setup.cpp` is a separate native form fixture;
compile it with `/EHsc /std:c++17 /MT` and link `user32.lib gdi32.lib`, using an
executable name such as `lognames-ui-probe.exe`. Run it with a fresh Temp folder.
It checks form capture, data round-trip and Cancel, and writes an own-window
`names-ui.bmp` for layout inspection. It never creates a client world or sends
traffic.
