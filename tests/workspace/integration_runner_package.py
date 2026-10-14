"""Packaged performer smoke test with synthetic loopback host and explicit mock provider."""
import argparse
import json
from pathlib import Path
import socketserver
import subprocess
import sys
import tempfile
import threading
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'agent_companion'))
from test_runner import Fixture
parser=argparse.ArgumentParser()
parser.add_argument('--exe',type=Path,default=ROOT/'WinRel/MUSHclientAgentRunner.exe')
args=parser.parse_args()
fixture=Fixture('Guard')
token='offline-fixture-token-0123456789abcdef0123456789abcdef'
class Handler(socketserver.BaseRequestHandler):
 def handle(self):
  data=b''
  while b'\n' not in data:
   chunk=self.request.recv(8192)
   if not chunk:return
   data+=chunk
   if len(data)>131072:return
  req=json.loads(data)
  assert req['token']==token
  result=fixture.call(req['tool'],req['arguments'])
  self.request.sendall(json.dumps({'ok':True,'result':result}).encode()+b'\n')
with socketserver.TCPServer(('127.0.0.1',0),Handler) as server,tempfile.TemporaryDirectory(prefix='mushclient-packaged-runner-') as directory:
 threading.Thread(target=server.serve_forever,daemon=True).start()
 directory=Path(directory);endpoint=directory/'endpoint.json';provider=directory/'provider.json'
 endpoint.write_text(json.dumps({'host':'127.0.0.1','port':server.server_address[1],'token':token,'api_version':1}))
 provider.write_text(json.dumps([sys.executable,'-c','import json,sys; c=json.load(sys.stdin); assert "endpoint" not in c; print(json.dumps({"kind":"pose","text":"Guard waits for human review."}))']))
 result=subprocess.run([str(args.exe.resolve()),'--endpoint',str(endpoint),'--generator-command-json',str(provider),'--duration','5'],capture_output=True,text=True,timeout=15)
 assert result.returncode==0,(result.returncode,result.stderr)
 assert len(fixture.proposals)==1 and fixture.proposals[0]['text']=='Guard waits for human review.'
 assert not set(fixture.calls)&{'send','approve','advance_turn','connect'}
 server.shutdown()
 print('PASS packaged standalone performer: mock subprocess, authenticated loopback context, task/heartbeat, one unsent proposal; no game send')
