"""Package a clean, extracted official distribution with the preview executable."""
import argparse
import hashlib
from pathlib import Path
import shutil
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--distribution', type=Path, required=True)
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--destination', type=Path, required=True)
parser.add_argument('--zip', type=Path, required=True)
parser.add_argument('--source-commit', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
destination = args.destination.resolve()
destination.mkdir(parents=True, exist_ok=False)
for item in args.distribution.iterdir():
    if item.name.startswith('$') or item.name.lower() == 'uninstall.exe':
        continue  # Installer helpers do not belong in a portable distribution.
    target = destination / item.name
    if item.is_dir():
        shutil.copytree(item, target)
    else:
        shutil.copy2(item, target)
shutil.copy2(args.exe, destination / 'MUSHclient.exe')
shutil.copy2(root / 'docs/INLINE_IMAGES.md', destination / 'INLINE_IMAGES.md')
shutil.copytree(root / 'licences', destination / 'licences')
for folder in ('logs', 'sounds', 'worlds'):
    (destination / folder).mkdir(exist_ok=True)
(destination / 'Start MUSHclient.cmd').write_text(
    '@echo off\ncd /d "%~dp0"\nstart "" "%~dp0MUSHclient.exe" /noregister\n',
    encoding='ascii')
(destination / 'README-FIRST.txt').write_text(
    'MUSHclient inline images - full portable Windows preview\n\n'
    'Extract the entire ZIP into a writable folder. Open Start MUSHclient.cmd.\n'
    'Keep the folders and DLLs beside MUSHclient.exe; no separate Lua install is needed.\n'
    'Create a new world or open a copy of your existing world file.\n'
    'This package has fresh preferences and no personal worlds or credentials.\n\n'
    'Static Pueblo IMG and MXP IMAGE illustrations scroll between paragraphs.\n'
    'Toolbar drag outlines follow the toolbar on scaled displays.\n'
    'Windows 7+; 32-bit unsigned preview. Mac/Wine has not been validated.\n'
    'See INLINE_IMAGES.md for behavior, limits and verification results.\n\n'
    'Runtime files: official MUSHclient 5.06 distribution from\n'
    'https://www.gammon.com.au/files/mushclient/mushclient506.exe\n'
    'Installer MD5: ba0815ce7f84dac6695a8ef5b6ace2fb (matches publisher).\n'
    f'Source commit: {args.source_commit}\n'
    f'Executable SHA256: {hashlib.sha256(args.exe.read_bytes()).hexdigest()}\n'
    'Source: https://github.com/Balerion300/mushclient/pull/1\n', encoding='utf-8')
files = sorted(p for p in destination.rglob('*') if p.is_file())
manifest = '\n'.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' +
                     p.relative_to(destination).as_posix() for p in files) + '\n'
(destination / 'SHA256SUMS.txt').write_text(manifest, encoding='utf-8')
with zipfile.ZipFile(args.zip, 'w', zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(destination.rglob('*')):
        if file.is_file():
            archive.write(file, Path('MUSHclient-inline-images') / file.relative_to(destination))
print(args.zip.resolve())
print('Packaged files:', len(files) + 1)
