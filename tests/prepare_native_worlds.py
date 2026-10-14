"""Prepare a separate portable runtime with two loopback-only test worlds.

Copies only the specified compiled executable, Lua runtime and UI resource DLL;
never copies saved worlds, user preferences, logs, or credentials.
"""
import argparse
from pathlib import Path
import shutil
import sqlite3
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=ROOT / 'WinDebug/MUSHclient.exe')
    parser.add_argument('--runtime', type=Path, required=True,
                        help='Installed MUSHclient directory containing Lua and locale/en.dll')
    parser.add_argument('--destination', type=Path, default=ROOT / '.test-runtime')
    parser.add_argument('--plugin', type=Path, default=ROOT / 'tests/inline_image_transcript.xml',
                        help='Fixture plugin to install in both isolated worlds')
    parser.add_argument('--protocol', choices=['MXP', 'Pueblo'],
                        help='Open one world to serialize global clipboard compatibility tests')
    args = parser.parse_args()
    destination = args.destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    (destination / 'locale').mkdir(exist_ok=True)
    shutil.copy2(args.exe, destination / 'MUSHclient.exe')
    shutil.copy2(args.runtime / 'lua5.1.dll', destination / 'lua5.1.dll')
    shutil.copy2(args.runtime / 'locale/en.dll', destination / 'locale/en.dll')
    worlds = []
    for protocol, port in [('MXP', 18766), ('Pueblo', 18767)]:
        if args.protocol and args.protocol != protocol: continue
        plugin = destination / (protocol.lower() + '-test.xml')
        shutil.copy2(args.plugin, plugin)
        world = destination / (protocol.lower() + '-test.mcl')
        world.write_text('<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE muclient>\n'
            '<muclient><world name="Inline images - ' + protocol + ' test" '
            'site="127.0.0.1" port="' + str(port) + '" use_mxp="1" detect_pueblo="y" '
            'enable_scripts="y" enable_triggers="y" script_language="Lua" '
            'output_font_name="Consolas" output_font_height="11" wrap_column="100" '
            'max_output_lines="5000"/><include name=' + quoteattr(str(plugin)) + ' plugin="y"/>'
            '</muclient>\n', encoding='utf-8')
        worlds.append(str(world))
    db = sqlite3.connect(destination / 'mushclient_prefs.sqlite')
    db.executescript('CREATE TABLE IF NOT EXISTS control(name TEXT PRIMARY KEY,value INT);'
                    'CREATE TABLE IF NOT EXISTS prefs(name TEXT PRIMARY KEY,value TEXT);'
                    'CREATE TABLE IF NOT EXISTS worlds(name TEXT PRIMARY KEY,value TEXT);')
    db.execute("INSERT OR REPLACE INTO control VALUES('database_version',1)")
    db.execute("INSERT OR REPLACE INTO control VALUES('First time',0)")
    db.execute("INSERT OR REPLACE INTO control VALUES('Version',507)")
    db.execute("INSERT OR REPLACE INTO control VALUES('Tip_StartUp',1)")
    for name, value in {'WorldList': '*'.join(worlds), 'EnableSpellCheck': '0',
                        'AutoConnectWorlds': '1', 'OpenWorldsMaximised': '0' if 'compatibility' in args.plugin.name else '1',
                        'ConfirmBeforeClosingMushclient': '0',
                        'ConfirmBeforeClosingWorld': '0'}.items():
        db.execute('INSERT OR REPLACE INTO prefs VALUES(?,?)', (name, value))
    db.commit()
    db.close()
    print(destination)
    print('Launch MUSHclient.exe /noregister with this directory as working directory.')


if __name__ == '__main__':
    main()
