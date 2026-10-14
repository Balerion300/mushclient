"""Prepare isolated Lua regression worlds for upstream issue #82.

Run each generated client with /noregister in its own runtime directory.
No network connection is enabled. Results exercise the actual application API.
"""
import argparse
from pathlib import Path
import shutil
import sqlite3
from xml.sax.saxutils import quoteattr

REPRO_HEX = 'FFFAC9636F6D6D2E6368616E6E656C207B20226368616E223A20226774656C6C222C20226D7367223A2022405928405747726F757040592920405931333337204834783072204669656E646973683A2027686D274077222C2022706C61796572223A20224669656E6469736822207DFFF01B5B303B33376D5B1B5B313B33316D61796C6F722D301B5B303B33376D20546865204772616E642043697479206F662041796C6F72201B5B313B33326D45786974733A4E45535755441B5B303B33376D5D205B1B5B313B33366D57697A693A1B5B313B33356D201B5B313B33366D496E636F673A1B5B313B33356D1B5B303B33376D5D201B5B313B33316D54656C6C733A30201B5B6D1B5B303B33376D1B5B303B33376D0A0D'

parser = argparse.ArgumentParser()
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--destination', type=Path, required=True)
args = parser.parse_args()
dest = args.destination.resolve()
dest.mkdir(parents=True, exist_ok=False)
(dest / 'locale').mkdir()
shutil.copy2(args.exe, dest / 'MUSHclient.exe')
shutil.copy2(args.runtime / 'lua5.1.dll', dest / 'lua5.1.dll')
shutil.copy2(args.runtime / 'locale/en.dll', dest / 'locale/en.dll')
expected = bytes.fromhex(REPRO_HEX).decode('latin1').encode('utf-8').hex()
all_bytes = bytes(range(1, 256)).hex()
all_expected = bytes(range(1, 256)).decode('latin1').encode('utf-8').hex()
plugin = dest / 'heap82.xml'
plugin.write_text(r'''<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE muclient><muclient>
<plugin name="Heap82_Regression" author="MUSHclient contributors"
id="bf51c66782aa762f55acc082" language="Lua" version="1.0" requires="5.07"/>
<script><![CDATA[
function OnPluginInstall()
  if WorldName() ~= 'Isolated heap82 regression' then return end
  local report = GetPluginInfo(GetPluginID(), 6) .. '.results.txt'
  local f = assert(io.open(report, 'w'))
  f:write('START issue 82 regression\n'); f:flush()
  local ok, err = pcall(function()
    local input = utils.fromhex(''' + repr(REPRO_HEX) + r''')
    local expected = utils.fromhex(''' + repr(expected) + r''')
    assert(utils.utf8convert(input) == expected, 'reported input conversion mismatch')
    print(utils.utf8convert(input)) -- exact original reproduction, including output
    f:write('PASS exact issue input and print\n'); f:flush()
    local all = utils.fromhex(''' + repr(all_bytes) + r''')
    local all_expected = utils.fromhex(''' + repr(all_expected) + r''')
    for i = 1, 20000 do
      assert(utils.utf8convert(input) == expected, 'repeated issue input mismatch')
      assert(utils.utf8convert(all) == all_expected, 'byte-range conversion mismatch')
      if i % 1000 == 0 then collectgarbage('collect') end
    end
    f:write('PASS 20000 repetitions each of issue input and all nonzero byte values\n')
    assert(utils.utf8convert('') == '', 'empty input mismatch')
    f:write('PASS empty string\nPASS COMPLETE\n')
  end)
  if not ok then f:write('FAIL ', tostring(err), '\n') end
  f:close()
end
]]></script></muclient>''', encoding='utf-8')
world = dest / 'heap82.mcl'
world.write_text('<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE muclient>\n'
    '<muclient><world name="Isolated heap82 regression" site="127.0.0.1" port="1" '
    'enable_scripts="y" script_language="Lua"/><include name=' + quoteattr(str(plugin)) +
    ' plugin="y"/></muclient>\n', encoding='utf-8')
db = sqlite3.connect(dest / 'mushclient_prefs.sqlite')
db.executescript('CREATE TABLE control(name TEXT PRIMARY KEY,value INT);'
                'CREATE TABLE prefs(name TEXT PRIMARY KEY,value TEXT);'
                'CREATE TABLE worlds(name TEXT PRIMARY KEY,value TEXT);')
db.executemany('INSERT INTO control VALUES(?,?)', [('database_version',1),('First time',0),('Version',507),('Tip_StartUp',1)])
db.executemany('INSERT INTO prefs VALUES(?,?)', [('WorldList',str(world)),('EnableSpellCheck','0'),('AutoConnectWorlds','0'),('ConfirmBeforeClosingMushclient','0'),('ConfirmBeforeClosingWorld','0')])
db.commit()
db.close()
print(dest)
