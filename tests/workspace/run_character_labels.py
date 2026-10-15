"""Exercise the production display-label dialog using fictional local data."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests'))
from run_native_tests import compiler_environment

out=Path(tempfile.mkdtemp(prefix='mush-character-labels-'))
env=compiler_environment()
compiler=Path(env['VCToolsInstallDir'])/'bin/Hostx86/x86/cl.exe'
subprocess.run([str(compiler),'/nologo','/MT','/EHsc','/std:c++14',
    '/D_WIN32_WINNT=0x0601','/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT),
    str(ROOT/'tests/workspace/character_labels_probe.cpp'),
    str(ROOT/'workspace/CharacterLabelsDialog.cpp'),str(ROOT/'WorkspaceStyle.cpp'),
    '/Fe:'+str(out/'character-labels.exe'),'/Fo:'+str(out)+os.sep,
    '/link','/SUBSYSTEM:WINDOWS','Comctl32.lib','Shlwapi.lib'],cwd=out,env=env,check=True)
subprocess.run([str(out/'character-labels.exe')],cwd=out,timeout=20,check=True)
print((out/'character-label-results.txt').read_text())
print('Evidence: '+str(out))
