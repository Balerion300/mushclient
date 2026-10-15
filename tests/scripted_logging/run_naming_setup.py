"""Build/run the actual logging forms in an isolated fictional Temp fixture."""
import os
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests'))
from run_native_tests import compiler_environment

out=Path(tempfile.mkdtemp(prefix='mush-logging-setup-ui-'))
(out/'style.cpp').write_text((ROOT/'WorkspaceStyle.cpp').read_text().replace(
    '#include "stdafx.h"','#include <algorithm>\nusing std::min;using std::max;'),encoding='utf-8')
env=compiler_environment()
compiler=Path(env['VCToolsInstallDir'])/'bin/Hostx86/x86/cl.exe'
exe=out/'naming-form-probe.exe'
subprocess.run([str(compiler),'/nologo','/EHsc','/std:c++17','/MTd','/Od',
    '/D_WIN32_WINNT=0x0601','/I'+str(ROOT),str(ROOT/'tests/scripted_logging/naming_setup.cpp'),
    str(out/'style.cpp'),
    '/Fe:'+str(exe),'/Fo:'+str(out)+os.sep,
    'user32.lib','gdi32.lib','advapi32.lib','comctl32.lib'],cwd=out,env=env,check=True)
result=subprocess.run([str(exe),str(out)],cwd=out,capture_output=True,text=True,timeout=45)
(out/'results.txt').write_text(result.stdout+result.stderr)
(out/'candidate.json').write_text(json.dumps({'kind':'native fictional logging fixture; no client or game traffic',
    'executable':str(exe),'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),
    'sources':{s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in ['workspace/LogStartSetup.cpp','WorkspaceStyle.cpp','workspace/WorkflowUi.h','tests/scripted_logging/naming_setup.cpp']}},indent=2))
try:
    from PIL import Image
    for bitmap in out.glob('*.bmp'):Image.open(bitmap).save(bitmap.with_suffix('.png'))
except ImportError: pass
print(result.stdout+result.stderr,flush=True)
print('Logging setup evidence: '+str(out),flush=True)
assert result.returncode==0,result.returncode
