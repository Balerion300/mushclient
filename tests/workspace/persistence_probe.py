"""Exercise actual workspace SQL statements on an isolated on-disk database.

This verifies persistence/schema/precise edits, not native frame restoration.
The C++ native probe separately checks pane sizing and connection identities.
"""
from pathlib import Path
import re
import sqlite3
import uuid

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "mainfrm.cpp").read_text()
schema_body = source.split("bool WorkspaceLayoutSchema()", 1)[1].split(
    "std::string WorkspaceLayoutUtf8", 1
)[0]
schema = "".join(re.findall(r'"(CREATE TABLE[^"\n]+)"', schema_body))

def statement(start):
    found = re.search(r'"(' + re.escape(start) + r'[^"\n]*)"', source)
    assert found, start
    return found.group(1)

output = ROOT / "WinRel" / "workspace-probe-objects"
output.mkdir(parents=True, exist_ok=True)
database = output / ("persistence-" + uuid.uuid4().hex + ".sqlite")
world = "fixture-world-id"
route = (world, "Tells", "^λ.*🐉", 1, 1)
layout = (world, "Tells", 1, 1, 0x8000, 987654, -987654, 640, 420)
prefix = (world, "Tells", "+channel λ🐉 ")

with sqlite3.connect(database) as db:
    db.executescript(schema)
    db.execute(statement("INSERT OR REPLACE INTO workspace_route_v1"), route)
    db.execute(statement("INSERT OR REPLACE INTO workspace_pane_layout_v1"), layout)
    db.execute(statement("INSERT OR REPLACE INTO workspace_input_prefix_v1"), prefix)

with sqlite3.connect(database) as db:
    db.executescript(schema)  # reopening an existing portable DB is idempotent
    restored = db.execute(statement("SELECT name,pattern,regex,omit_main"), (world,)).fetchall()
    assert restored == [route[1:]]
    assert db.execute(statement("SELECT prefix FROM workspace_input_prefix_v1"),
                      (world, "Tells")).fetchone() == (prefix[2],)
    assert db.execute(statement("SELECT detached,floating,alignment,x,y,width,height"),
                      (world, "Tells")).fetchone() == layout[2:]
    assert not db.execute(statement("SELECT name,pattern,regex,omit_main"),
                          ("another-world",)).fetchall()
    replacement = (world, "Tells", "tell λ", 0, 0)
    db.execute(statement("INSERT OR REPLACE INTO workspace_route_v1"), replacement)
    db.execute(statement("DELETE FROM workspace_route_v1"), route[:4])
    assert db.execute(statement("SELECT name,pattern,regex,omit_main"),
                      (world,)).fetchall() == [replacement[1:]]
    db.execute(statement("DELETE FROM workspace_route_v1"), replacement[:4])
    assert not db.execute(statement("SELECT name,pattern,regex,omit_main"), (world,)).fetchall()
    assert db.execute(statement("SELECT prefix FROM workspace_input_prefix_v1"),
                      (world, "Tells")).fetchone() == (prefix[2],)

print("PASS portable SQL reopen, Unicode pattern/prefix, layout, world isolation, exact edit/delete")
print("Database:", database)
