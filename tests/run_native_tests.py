"""Build/run image backend tests on Windows with VS C++, MFC, SDK and Pillow."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tests'))
from inline_image_fixture import Handler, ThreadingHTTPServer


def compiler_environment():
    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    installations = json.loads(subprocess.check_output([str(vswhere), '-all', '-products', '*', '-format', 'json']))
    for installation in installations:
        vcvars = Path(installation['installationPath']) / 'VC/Auxiliary/Build/vcvars32.bat'
        if vcvars.exists():
            with tempfile.NamedTemporaryFile(mode='w', suffix='.cmd', delete=False) as batch:
                batch.write(f'@call "{vcvars}" >nul\n@set\n')
            output = subprocess.check_output(['cmd.exe', '/d', '/c', batch.name], text=True)
            env = dict(os.environ)
            env.update(line.split('=', 1) for line in output.splitlines() if '=' in line and not line.startswith('='))
            # Some local VS installs contain only Spectre MFC static libraries.
            extra = Path(env['VCToolsInstallDir']) / 'atlmfc/lib/spectre/x86'
            if extra.exists():
                env['LIB'] = str(extra) + ';' + env.get('LIB', '')
            return env
    raise RuntimeError('Visual Studio C++ tools were not found')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--keep-build', action='store_true')
    args = parser.parse_args()
    env = compiler_environment()
    output = Path(tempfile.mkdtemp(prefix='mushclient-image-tests-'))
    exe = output / 'inline_image_backend.exe'
    compiler = Path(env['VCToolsInstallDir']) / 'bin/Hostx86/x86/cl.exe'
    command = [str(compiler), '/nologo', '/std:c++14', '/EHsc', '/MT', '/DWIN32', '/D_WINDOWS',
               '/D_WIN32_WINNT=0x0601', '/DLUA51', '/D_CRT_SECURE_NO_WARNINGS',
               '/DMUSHCLIENT_INLINE_IMAGE_TEST_ALLOW_LOOPBACK', '/I' + str(ROOT),
               str(ROOT / 'tests/inline_image_backend.cpp'), str(ROOT / 'InlineImage.cpp'),
               '/Fe:' + str(exe), '/Fo:' + str(output) + os.sep,
               '/link', '/SUBSYSTEM:CONSOLE']
    subprocess.run(command, cwd=output, env=env, check=True)
    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        result = subprocess.run([str(exe), f'http://127.0.0.1:{server.server_port}'], timeout=90)
    finally:
        server.shutdown()
        server.server_close()
    print('Test build:', output)
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
