"""Prepare an isolated portable frame-restoration fixture; does not launch it.

Acceptance: open Workspace / Output, inspect restored FloatingTest placement,
then select its rule and verify Unicode pattern/prefix. Close/relaunch and repeat.
No server, credentials, player worlds, or copied personal preferences are used.
"""
import argparse
from pathlib import Path
import shutil
import sqlite3
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--runtime", type=Path, required=True,
                    help="Directory containing lua5.1.dll and locale/en.dll")
parser.add_argument("--exe", type=Path, default=ROOT / "WinRel/MUSHclient.exe")
parser.add_argument("--destination", type=Path,
                    default=ROOT / "WinRel/workspace-probe-objects/frame-restore")
args = parser.parse_args()
destination = args.destination.resolve()
destination.mkdir(parents=True, exist_ok=True)
(destination / "locale").mkdir(exist_ok=True)
shutil.copy2(args.exe, destination / "MUSHclient.exe")
shutil.copy2(args.runtime / "lua5.1.dll", destination / "lua5.1.dll")
shutil.copy2(args.runtime / "locale/en.dll", destination / "locale/en.dll")
world_id = "0123456789abcdef01234567"
world = destination / "restoration-fixture.mcl"
world.write_text('<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE muclient>\n'
                 '<muclient><world name="Workspace restoration fixture" '
                 'id="' + world_id + '" site="127.0.0.1" port="18799" '
                 'output_font_name="Consolas" output_font_height="11" '
                 'script_language="Lua" max_output_lines="5000"/></muclient>\n',
                 encoding="utf-8")
db = sqlite3.connect(destination / "mushclient_prefs.sqlite")
db.executescript('CREATE TABLE IF NOT EXISTS control(name TEXT PRIMARY KEY,value INT);'
                'CREATE TABLE IF NOT EXISTS prefs(name TEXT PRIMARY KEY,value TEXT);'
                'CREATE TABLE IF NOT EXISTS worlds(name TEXT PRIMARY KEY,value TEXT);'
                'CREATE TABLE IF NOT EXISTS workspace_route_v1(world_id TEXT,name TEXT,pattern TEXT,regex INTEGER,omit_main INTEGER,PRIMARY KEY(world_id,name,pattern,regex));'
                'CREATE TABLE IF NOT EXISTS workspace_pane_layout_v1(world_id TEXT,name TEXT,detached INTEGER,floating INTEGER,alignment INTEGER,x INTEGER,y INTEGER,width INTEGER,height INTEGER,PRIMARY KEY(world_id,name));'
                'CREATE TABLE IF NOT EXISTS workspace_input_prefix_v1(world_id TEXT,name TEXT,prefix TEXT,PRIMARY KEY(world_id,name));')
for name, value in {"database_version": 1, "First time": 0,
                    "Version": 507, "Tip_StartUp": 1}.items():
    db.execute("INSERT OR REPLACE INTO control VALUES(?,?)", (name, value))
for name, value in {"WorldList": str(world), "EnableSpellCheck": "0",
                    "AutoConnectWorlds": "0", "OpenWorldsMaximised": "1",
                    "ConfirmBeforeClosingMushclient": "0",
                    "ConfirmBeforeClosingWorld": "0"}.items():
    db.execute("INSERT OR REPLACE INTO prefs VALUES(?,?)", (name, value))
db.execute("INSERT OR REPLACE INTO workspace_route_v1 VALUES(?,?,?,?,?)",
           (world_id, "FloatingTest", "^restoration λ🐉", 1, 0))
db.execute("INSERT OR REPLACE INTO workspace_pane_layout_v1 VALUES(?,?,?,?,?,?,?,?,?)",
           (world_id, "FloatingTest", 1, 1, 0x8000, 999999, -999999, 640, 420))
db.execute("INSERT OR REPLACE INTO workspace_input_prefix_v1 VALUES(?,?,?)",
           (world_id, "FloatingTest", "+channel λ🐉 "))
# This obsolete reserved-name rule must be skipped by restored validation.
db.execute("INSERT OR REPLACE INTO workspace_route_v1 VALUES(?,?,?,?,?)",
           (world_id, "oUtPuT", "never-match", 0, 1))
db.commit()
db.close()
print(destination)
print("Launch MUSHclient.exe /noregister with this working directory; keep disconnected.")
