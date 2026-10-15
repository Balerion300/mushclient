"""Native UI form acceptance; copied production forms with fictional frame IO."""
import os
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests'))
from run_native_tests import compiler_environment
out=Path(tempfile.mkdtemp(prefix='mush-workflow-forms-'))
for source,target in [('WorkspaceRouteManager.cpp','route-form-production.inc'),('WorkspaceSceneDirector.cpp','scene-form-production.inc')]:
    # Only application headers are substituted; native form code is unchanged.
    text=(ROOT/source).read_text()
    (out/target).write_text(re.sub(r'^#include .*\n','',text,flags=re.M),encoding='utf-8')
shutil.copy2(ROOT/'third_party/scintilla/bin/Scintilla.dll',out/'Scintilla.dll')
env=compiler_environment();compiler=Path(env['VCToolsInstallDir'])/'bin/Hostx86/x86/cl.exe'
sources=['tests/workspace/workflow_forms_probe.cpp','workspace/WorkspaceInputPolicy.cpp','workspace/PennEditorWindow.cpp','workspace/PennLanguage.cpp','workspace/AgentReviewPane.cpp','AgentWorkspaceCore.cpp','WorkspaceStyle.cpp']
subprocess.run([str(compiler),'/nologo','/MT','/EHsc','/std:c++14','/D_WIN32_WINNT=0x0601','/DNO_HTMLHELP','/DNDEBUG','/DWIN32','/D_WINDOWS','/DZLIB_DLL','/DLUA51','/D_CRT_SECURE_NO_WARNINGS','/D_CRT_NON_CONFORMING_SWPRINTFS','/DNO_WARN_MBCS_MFC_DEPRECATION',
    '/I'+str(ROOT),'/I'+str(ROOT/'luacom'),'/I'+str(ROOT/'openssl/include'),'/I'+str(out),
    *[str(ROOT/s) for s in sources],'/Fe:'+str(out/'workflow-forms.exe'),'/Fo:'+str(out)+os.sep,
    '/link','/SUBSYSTEM:WINDOWS','Oleacc.lib'],cwd=out,env=env,check=True)
process=subprocess.Popen([str(out/'workflow-forms.exe')],cwd=out)
try: process.wait(timeout=45)
except subprocess.TimeoutExpired:
    process.terminate();process.wait();raise
result=(out/'workflow-forms-results.txt').read_text();print(result,flush=True)
(out/'candidate.json').write_text(json.dumps({'kind':'native fictional form fixture; not packaged client acceptance',
    'executable':str(out/'workflow-forms.exe'),'sha256':hashlib.sha256((out/'workflow-forms.exe').read_bytes()).hexdigest(),
    'sources':{s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in sources+['WorkspaceRouteManager.cpp','WorkspaceSceneDirector.cpp','workspace/WorkflowUi.h']}},indent=2))
try:
    from PIL import Image
    for bitmap in out.glob('*.bmp'):Image.open(bitmap).save(bitmap.with_suffix('.png'))
except ImportError: pass
print('Workflow form evidence: '+str(out),flush=True)
assert 'failures=0' in result,result
assert process.returncode==0,process.returncode
