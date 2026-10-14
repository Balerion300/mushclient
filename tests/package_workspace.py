"""Create a clean full portable modernization preview; never copy installed settings."""
import argparse
import hashlib
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--distribution', type=Path, required=True)
p.add_argument('--destination', type=Path, required=True)
p.add_argument('--zip', type=Path, required=True)
p.add_argument('--source-commit', required=True)
a = p.parse_args()
dest = a.destination.resolve()
required = [ROOT/'WinRel/MUSHclient.exe', ROOT/'WinRel/MUSHclientAgent.exe',
            ROOT/'third_party/scintilla/bin/Scintilla.dll', ROOT/'WinRel/MUSHclientAgentRunner.exe']
for source in required:
    if not source.is_file(): raise SystemExit('Missing built component: '+str(source))
if dest.exists() or a.zip.exists(): raise SystemExit('Use fresh destination and ZIP names.')
# Shareable packages use a clean base; personal test overlays stay outside Git.
private_names = {'app.db', 'drafts.recovery', 'preferences.ini', '.env'}
for source in a.distribution.rglob('*'):
    if not source.is_file(): continue
    name = source.name.lower()
    if (name in private_names or name.endswith(('.mcl', '.mcl.bak'))
            or name.startswith('.env.') or 'editor-state' in source.parts):
        raise SystemExit('Distribution contains private world/runtime state; use the clean official runtime base.')

dest.mkdir(parents=True)
for source in a.distribution.iterdir():
    if source.name.startswith('$') or source.name.lower() in ('uninstall.exe','lua5.1.lib'): continue
    target = dest/source.name
    if source.is_dir(): shutil.copytree(source,target)
    else: shutil.copy2(source,target)
for source in required: shutil.copy2(source,dest/source.name)
notices = dest/'licences'
shutil.copytree(ROOT/'licences',notices)
shutil.copy2(ROOT/'third_party/scintilla/License.txt',notices/'Scintilla-LICENSE.txt')
shutil.copy2(ROOT/'third_party/SCINTILLA_NOTICE.md',notices/'SCINTILLA_NOTICE.md')
for name in ('LICENSE.MIT','PYTHON-LICENSE.txt','PYINSTALLER-COPYING.txt'):
    shutil.copy2(ROOT/'agent_companion/vendor'/name,notices/('JSON-LICENSE.txt' if name=='LICENSE.MIT' else name))
shutil.copytree(ROOT/'third_party/pennmush-reference',dest/'pennmush-reference')
for name in ('WORKSPACE_MODERNIZATION_PLAN.md','WORKSPACE_TESTS.md','PENN_IDE.md',
             'AGENT_WORKSPACE_CONTRACT.md','AGENT_REVIEW_PANE.md','AGENT_INTEGRATION_AUDIT.md','BUILD_WORKSPACE.md','INLINE_IMAGES.md','STABILITY_TESTS.md'):
    shutil.copy2(ROOT/'docs'/name,dest/name)
# Sources/examples for the explicitly configured external runner, never credentials.
(dest/'agent-examples').mkdir()
for name in ('runner.py','companion.py','tools.json'):
    shutil.copy2(ROOT/'agent_companion'/name,dest/'agent-examples'/name)
for name in ('logs','sounds','worlds'): (dest/name).mkdir(exist_ok=True)
(dest/'Start MUSHclient.cmd').write_text('@echo off\ncd /d "%~dp0"\nstart "" "%~dp0MUSHclient.exe" /noregister\n',encoding='ascii')
(dest/'README-FIRST.txt').write_text(
    'MUSHclient workspace modernization - full portable Windows preview\n\n'
    'Extract the entire ZIP into a writable folder. Open Start MUSHclient.cmd.\n'
    'Keep the DLLs and folders beside the executable; Lua is included.\n'
    'Open a copy of your world file for testing. The package has fresh preferences.\n'
    'Use the Workspace menu for dockable transcripts, route management, the PennMUSH\n'
    'editor, explicit agent access, conversations, review/captures and scene direction.\n'
    'Agent access starts disabled and has no automatic game-send capability.\n'
    'Editor and scene commands require human review before sending.\n'
    'MUSHclientAgent.exe is the native stdio MCP companion; the access dialog\n'
    'provides its connection command. MUSHclientAgentRunner.exe is an optional\n'
    'Windows x64 external-provider runner requiring explicit configuration.\n\n'
    'Windows preview: MUSHclient and its companion are x86. Tested on Windows 11.\n'
    'The package is unsigned. Mac, Linux and Wine are not validated.\n'
    'See WORKSPACE_TESTS.md for measured checks and outstanding acceptance.\n'
    'No personal worlds, passwords, grants, endpoints or player output are included.\n\n'
    'Runtime base: official MUSHclient 5.06 distribution.\n'
    'Source: https://github.com/Balerion300/mushclient\n'
    f'Source commit: {a.source_commit}\n', encoding='utf-8')
files = sorted(x for x in dest.rglob('*') if x.is_file())
manifest = ''.join(hashlib.sha256(x.read_bytes()).hexdigest()+'  '+x.relative_to(dest).as_posix()+'\n' for x in files)
(dest/'SHA256SUMS.txt').write_text(manifest,encoding='utf-8')
with zipfile.ZipFile(a.zip,'w',zipfile.ZIP_DEFLATED) as z:
    for source in sorted(dest.rglob('*')):
        if source.is_file(): z.write(source,Path('MUSHclient-workspace-preview')/source.relative_to(dest))
print(a.zip.resolve())
print('Packaged files:',len(files)+1)
