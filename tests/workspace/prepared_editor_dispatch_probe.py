"""Compile the exact production prepared-send function against fictional host IO.

The real scene Store is linked. Only CMUSHclientDoc's transport-facing members
are mocked, so callbacks can reproduce deletion/expiration during SendMsg without
opening a client, game connection, profile, desktop window or model provider.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tests'))
from run_native_tests import compiler_environment

HARNESS = r'''
#include "AgentWorkspaceCore.h"
#include <cassert>
#include <crtdbg.h>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using LPCTSTR = const char*;
enum { eConnectConnectedToMud = 1 };
struct Identity { std::string id; };
struct Queue { size_t size=0; size_t GetCount() const { return size; } };
struct CMUSHclientDoc {
  int m_iConnectPhase=eConnectConnectedToMud;
  long m_nTotalLinesSent=0;
  Queue m_QueuedCommandsList;
  bool dispatch=true, queue=false;
  unsigned calls=0;
  std::function<void()> callback;
  void SendMsg(const char*,bool,bool,bool) {
    ++calls;
    if(callback) callback();
    if(dispatch) {
      if(queue) ++m_QueuedCommandsList.size;
      else ++m_nTotalLinesSent;
    }
  }
};
std::map<CMUSHclientDoc*,Identity> identities;
std::map<unsigned __int64,std::string> sceneDraftBindings;
agent_workspace::Store agentStore;
bool encodeCommand(CMUSHclientDoc*,const std::string& source,std::string& out) {
  out=source;return true;
}
// PRODUCTION_FUNCTION
int main(int argc,char** argv) {
  _set_error_mode(_OUT_TO_STDERR);
  _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
  assert(argc==2);
  const std::string mode=argv[1], endpoint="fictional-world-A";
  CMUSHclientDoc doc;
  identities[&doc].id=endpoint;
  agentStore.addWindow(endpoint,"Fictional A","Main");
  auto session=agentStore.enable("Fixture",{endpoint},true);
  auto scene=agentStore.createScene("Fictional review acceptance");
  agent_workspace::Npc npc;
  npc.id="fixture-npc";npc.agent="Fixture";npc.window=endpoint;
  npc.name="Fixture Guard";npc.posePrefix=":";
  agentStore.assignNpc(scene,npc);
  agentStore.directScene(scene,"","Initial direction",npc.id,"running");
  auto action=agentStore.proposeAction("Fixture",session,scene,npc.id,
      agentStore.humanScene(scene).revision,"pose","Fixture Guard waits.","once");
  auto draft=agentStore.stageAction(scene,action.id);
  const unsigned __int64 document=7;
  sceneDraftBindings[document]=draft.id;
  if(mode=="erase-binding") doc.callback=[&]{sceneDraftBindings.erase(document);};
  else if(mode=="expire-scene") doc.callback=[&]{
    agentStore.directScene(scene,"","Callback changed direction",npc.id,"paused");
  };
  else if(mode=="remove-world") doc.callback=[&]{identities.erase(&doc);};
  else if(mode=="change-world") doc.callback=[&]{identities[&doc].id="fictional-new-session";};
  else if(mode=="queue") doc.queue=true;
  else if(mode=="decline") doc.dispatch=false;
  else assert(mode=="normal"||mode=="replay"||mode=="modified-command");
  bool result=false;
  try {
    result=WorkspaceSendPreparedEditor(endpoint,document,1,
        {draft.text+(mode=="modified-command"?" changed":"")});
  } catch(const std::exception& e) {
    std::cerr<<"UNCAUGHT_DISPATCH_EXCEPTION "<<e.what()<<" calls="<<doc.calls<<"\n";
    return 4;
  }
  const bool expected=mode=="normal"||mode=="replay"||mode=="queue"||mode=="erase-binding";
  if(result!=expected) {
    std::cerr<<"WRONG_RESULT result="<<result<<" expected="<<expected<<" calls="<<doc.calls<<"\n";
    return 5;
  }
  assert(doc.calls==(mode=="modified-command"?0u:1u));
  if(mode=="replay") {
    assert(!WorkspaceSendPreparedEditor(endpoint,document,1,{draft.text}));
    assert(doc.calls==1);
  }
  const auto final=agentStore.humanScene(scene);
  assert(final.actions.front().status==(expected?"queued":mode=="expire-scene"?"expired":"staged"));
  std::cout<<"PASS "<<mode<<" calls="<<doc.calls<<" sent="<<doc.m_nTotalLinesSent
           <<" queued="<<doc.m_QueuedCommandsList.GetCount()<<" status="<<final.actions.front().status<<"\n";
}
'''


def production_function(name='WorkspaceSendPreparedEditor'):
    source = (ROOT/'WorkspaceIntegration.cpp').read_text(encoding='utf-8')
    start = source.index('bool '+name+'(')
    opening = source.index('{', start)
    nesting = 1
    end = opening+1
    while nesting:
        nesting += (source[end] == '{')-(source[end] == '}')
        end += 1
    # LPCTSTR is CString's conversion in the client; this fixture's identity
    # string uses c_str instead. No dispatch logic is replaced.
    return source[start:end].replace('(LPCTSTR)it->second.id','it->second.id.c_str()').replace(
        '(LPCTSTR)live->second.id','live->second.id.c_str()')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--observe', action='store_true', help='Retain failures as before-fix evidence')
    args = parser.parse_args()
    out = Path(tempfile.mkdtemp(prefix='mush-prepared-editor-dispatch-'))
    (out/'probe.cpp').write_text(HARNESS.replace('// PRODUCTION_FUNCTION',production_function()),encoding='utf-8')
    env = compiler_environment()
    compiler = Path(env['VCToolsInstallDir'])/'bin/Hostx86/x86/cl.exe'
    executable = out/'probe.exe'
    subprocess.run([str(compiler),'/nologo','/EHsc','/std:c++14','/MTd','/Od',
        '/D_WIN32_WINNT=0x0601','/I'+str(ROOT),str(out/'probe.cpp'),
        str(ROOT/'AgentWorkspaceCore.cpp'),'/Fe:'+str(executable),'/Fo:'+str(out)+os.sep],
        cwd=out,env=env,check=True)
    receipts=[]
    for case in ('normal','replay','queue','decline','modified-command','remove-world',
                 'change-world','expire-scene','erase-binding'):
        result=subprocess.run([str(executable),case],capture_output=True,text=True,timeout=10)
        receipts.append({'case':case,'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
        print(case+': exit='+str(result.returncode)+' '+(result.stdout+result.stderr).strip(),flush=True)
    (out/'results.json').write_text(json.dumps(receipts,indent=2))
    print('Evidence: '+str(out),flush=True)
    if not args.observe:
        assert all(r['exit_code']==0 for r in receipts),'Prepared-editor dispatch regression failed'
