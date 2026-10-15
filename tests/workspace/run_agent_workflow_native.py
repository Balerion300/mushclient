"""Run bounded native read/scene/editor acceptance in a fresh fictional Temp dir."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tests'))
from run_native_tests import compiler_environment
from prepared_editor_dispatch_probe import production_function

out = Path(tempfile.mkdtemp(prefix='mush-agent-workflow-native-'))
shutil.copy2(ROOT/'third_party/scintilla/bin/Scintilla.dll',out/'Scintilla.dll')
(out/'prepared_editor_dispatch_body.inc').write_text(production_function(),encoding='utf-8')
(out/'private_read_publication_body.inc').write_text(production_function('WorkspacePublishRecord'),encoding='utf-8')
# The production implementation is Win32; omit its application PCH include
# only, so standalone fixtures exercise the same styling and subclass code.
(out/'workflow-style.cpp').write_text((ROOT/'WorkspaceStyle.cpp').read_text().replace('#include "stdafx.h"','#include <algorithm>\nusing std::min;using std::max;'),encoding='utf-8')
env=compiler_environment()
compiler=Path(env['VCToolsInstallDir'])/'bin/Hostx86/x86/cl.exe'
for name,sources in {
    'target-read': ['workspace/PennEditorTargetTests.cpp','workspace/PennLanguage.cpp'],
    'scene-send': ['workspace/PennSceneSendSmoke.cpp','workspace/PennLanguage.cpp','AgentWorkspaceCore.cpp'],
    'private-publication': ['workspace/PennPrivateReadSmoke.cpp','workspace/PennLanguage.cpp','AgentWorkspaceCore.cpp'],
}.items():
    exe=out/(name+'.exe')
    subprocess.run([str(compiler),'/nologo','/EHsc','/std:c++17','/MTd','/Od',
        '/D_WIN32_WINNT=0x0601','/I'+str(out),'/I'+str(ROOT),*[str(ROOT/s) for s in sources],str(out/'workflow-style.cpp'),
        '/Fe:'+str(exe),'/Fo:'+str(out)+os.sep,'user32.lib','gdi32.lib','advapi32.lib','comctl32.lib'],
        cwd=out,env=env,check=True)
    state=out/(name+'-state')
    state.mkdir()
    result=subprocess.run([str(exe),str(state)],capture_output=True,text=True,cwd=out,timeout=40)
    (out/(name+'-results.txt')).write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr,flush=True)
    assert result.returncode==0,(name,result.returncode)
print('Native agent workflow evidence: '+str(out),flush=True)
