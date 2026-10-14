import os
from pathlib import Path
import subprocess
import tempfile
import json
from run_native_tests import compiler_environment
root=Path(__file__).resolve().parents[1]
env=compiler_environment()
out=Path(tempfile.mkdtemp(prefix='mushclient-agent-tests-'))
compiler=Path(env['VCToolsInstallDir'])/'bin/Hostx86/x86/cl.exe'
exe=out/'agent_core_test.exe'
subprocess.run([str(compiler),'/nologo','/EHsc','/std:c++14','/MT',str(root/'tests/agent_core_test.cpp'),str(root/'AgentWorkspaceCore.cpp'),'/Fe:'+str(exe),'/Fo:'+str(out)+os.sep],env=env,cwd=out,check=True)
subprocess.run([str(exe)],check=True)
print('Agent core test build:',out)
bridge=out/'agent_bridge_test.exe'
subprocess.run([str(compiler),'/nologo','/EHsc','/std:c++14','/MT','/D_WIN32_WINNT=0x0601',str(root/'tests/agent_bridge_test.cpp'),str(root/'AgentWorkspaceCore.cpp'),str(root/'AgentBridgeProtocol.cpp'),str(root/'AgentBridgeHost.cpp'),'/Fe:'+str(bridge),'/Fo:'+str(out)+os.sep],env=env,cwd=out,check=True)
subprocess.run([str(bridge)],check=True,timeout=20)
companion=out/'MUSHclientAgent.exe'
subprocess.run([str(compiler),'/nologo','/EHsc','/std:c++14','/MT','/D_WIN32_WINNT=0x0601',str(root/'agent_companion/MUSHclientAgent.cpp'),str(root/'AgentWorkspaceCore.cpp'),str(root/'AgentBridgeProtocol.cpp'),'/Fe:'+str(companion),'/Fo:'+str(out)+os.sep],env=env,cwd=out,check=True)
tools=json.loads(subprocess.check_output([str(companion),'tools'],text=True))['tools']
assert len(tools)==22
requests=[{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-06-18'}},{'jsonrpc':'2.0','method':'notifications/initialized'},{'jsonrpc':'2.0','id':2,'method':'tools/list'}]
result=subprocess.run([str(companion),'--endpoint','unused','mcp'],input=''.join(json.dumps(r)+'\n' for r in requests),text=True,capture_output=True,check=True)
responses=[json.loads(line) for line in result.stdout.splitlines()]
assert len(responses)==2 and len(responses[1]['result']['tools'])==22
assert responses[0]['result']['protocolVersion']=='2025-06-18' and not result.stderr
print('Native standalone MCP companion: 22 tools, initialize, notification suppression, stdio passed')

malformed="["*10000+"0"+"]"*10000+"\n"+' {"jsonrpc":"2.0","id":1,"id":2,"method":"ping"}\n'
r=subprocess.run([str(companion),"--endpoint","unused","mcp"],input=malformed,text=True,capture_output=True,check=True)
assert len(r.stdout.splitlines())==2 and all("error" in json.loads(v) for v in r.stdout.splitlines())
print("Native stdio: excessive JSON depth and duplicate keys fail closed")
