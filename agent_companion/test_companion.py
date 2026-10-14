import json
import pathlib
import socketserver
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
import companion

class CompanionTests(unittest.TestCase):
 def test_contract(self):
  self.assertEqual(len(companion.TOOLS),22)
  self.assertEqual(len(companion.BY_NAME),22)
  self.assertFalse(any(n in companion.BY_NAME for n in ['send','connect','execute','settings']))
  for tool in companion.TOOLS:
   self.assertFalse(tool['inputSchema']['additionalProperties'])
 def test_json_depth_and_duplicates(self):
  for value in ['{"token":"first","token":"second"}','['*20+'0'+']'*20,'['*2000+'0'+']'*2000]:
   with self.assertRaises(ValueError):companion.parse_json(value)
 def test_validation(self):
  companion.validate('propose_editor_edit',{'editor_id':'native-editor','revision':4294967296,'text':'proposed'})
  for tool,args in [('send',{}),('list_windows',{'extra':1}),('read_output',{}),('read_output',{'window_id':'w','limit':True}),('wait_for_output',{'window_id':'w','revision':0,'timeout_ms':10001}),('prepare_draft',{'window_id':'w','request_id':'r','text':'x'*32769}),('propose_npc_action',{'scene_id':'s','npc_id':'n','revision':1,'kind':'pose','text':'line\ncommand','request_id':'r'})]:
   with self.subTest(tool=tool,args=args),self.assertRaises(ValueError):companion.validate(tool,args)
 def test_rpc_notifications(self):
  with patch.object(companion,'invoke') as invoke:
   self.assertIsNone(companion.rpc({'jsonrpc':'2.0','method':'tools/call','params':{'name':'list_windows'}},None))
   invoke.assert_not_called()
 def test_initialize_and_list(self):
  init=companion.rpc({'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-06-18'}},None)
  self.assertEqual(init['result']['protocolVersion'],'2025-06-18')
  listed=companion.rpc({'jsonrpc':'2.0','id':2,'method':'tools/list'},None)
  self.assertEqual(len(listed['result']['tools']),22)
 def test_error_redaction(self):
  with patch.object(companion,'invoke',side_effect=OSError('SECRET TOKEN')):
   reply=companion.rpc({'jsonrpc':'2.0','id':3,'method':'tools/call','params':{'name':'list_windows'}},None)
   self.assertNotIn('SECRET',json.dumps(reply))
 def test_poll(self):
  with patch.object(companion,'call_host',side_effect=[{'revision':1},{'revision':2}]) as call:
   self.assertTrue(companion.invoke('endpoint','wait_for_output',{'window_id':'w','revision':1,'timeout_ms':500})['changed'])
   self.assertEqual(call.call_count,2)
 def test_fresh_loopback_endpoint(self):
  received=[]
  class Handler(socketserver.BaseRequestHandler):
   def handle(self):
    data=b''
    while b'\n' not in data:data+=self.request.recv(8192)
    received.append(json.loads(data));self.request.sendall(b'{"ok":true,"result":{"windows":[]}}\n')
  with socketserver.TCPServer(('127.0.0.1',0),Handler) as server,tempfile.TemporaryDirectory() as tmp:
   thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
   path=pathlib.Path(tmp)/'endpoint.json'
   config={'host':'127.0.0.1','port':server.server_address[1],'token':'a'*64,'api_version':1};path.write_text(json.dumps(config))
   self.assertEqual(companion.call_host(path,'list_windows',{}),{'windows':[]})
   config['token']='b'*64;path.write_text(json.dumps(config));companion.call_host(path,'list_windows',{})
   self.assertEqual(received[1]['token'],'b'*64)
   path.unlink()
   with self.assertRaises(OSError):companion.call_host(path,'list_windows',{})
   server.shutdown()
 def test_stdio_subprocess(self):
  requests=[{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'protocolVersion':'2025-11-25'}},{'jsonrpc':'2.0','method':'notifications/initialized'},{'jsonrpc':'2.0','id':2,'method':'tools/list'}]
  p=subprocess.run([sys.executable,str(pathlib.Path(companion.__file__)),'--endpoint','unused','mcp'],input=''.join(json.dumps(r)+'\n' for r in requests),text=True,capture_output=True,check=True)
  replies=[json.loads(line) for line in p.stdout.splitlines()]
  self.assertEqual(len(replies),2);self.assertEqual(len(replies[1]['result']['tools']),22);self.assertEqual(p.stderr,'')

if __name__=='__main__':unittest.main()
