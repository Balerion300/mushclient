import copy
import json
import sys
import time
import unittest
from runner import Runner, Interrupted, command_generator

class Fixture:
 def __init__(self, agent):
  self.agent=agent;self.task={'task_id':'task','cancellation_requested':False};self.calls=[];self.proposals=[]
  self.scene={'scene_id':'scene','title':'Offline rehearsal','notes':'A closed gate','direction':'Wait for human review','revision':1,'state':'running','turn':agent,'actions':[],
              'npcs':[{'npc_id':agent,'name':agent,'window_id':agent+'-output','assigned_to_you':True,'brief':'private-'+agent,'knowledge':'secret-'+agent},
                      {'npc_id':'other','name':'Other','assigned_to_you':False,'public_summary':'visible public description'}]}
 def call(self, tool, args):
  self.calls.append(tool)
  if tool=='start_agent_task':return self.task
  if tool=='get_agent_tasks':return {'tasks':[self.task]}
  if tool in ['heartbeat_agent_task','update_agent_task']:return self.task
  if tool=='list_agent_scenes':return {'scenes':[{'scene_id':'scene'}]}
  if tool=='read_agent_scene':return copy.deepcopy(self.scene)
  if tool=='get_agent_workspace':return {'contexts':[],'shared_editors':[]}
  if tool=='read_agent_messages':return {'messages':[{'role':'human','text':'Respect the shared notes'}]}
  if tool=='read_output':return {'lines':[{'text':'fictional completed output'}]}
  if tool=='propose_npc_action':self.proposals.append(args);return {'sent':False}
  raise AssertionError(tool)

class RunnerTests(unittest.TestCase):
 def test_separate_private_contexts_and_unsent_proposals(self):
  contexts=[]
  for agent in ['Guard','Merchant']:
   fixture=Fixture(agent)
   def generate(context,check):
    check();contexts.append(context);return {'kind':'dialogue','text':context['npc']['name']+' speaks.'}
   self.assertEqual(Runner(fixture.call,generate,duration=2).run(),1)
   self.assertEqual(len(fixture.proposals),1)
   self.assertFalse(set(fixture.calls)&{'send','connect','approve','advance_turn'})
  self.assertNotIn('private-Merchant',json.dumps(contexts[0]))
  self.assertNotIn('private-Guard',json.dumps(contexts[1]))
  self.assertNotIn('endpoint',json.dumps(contexts))
 def test_changed_direction_discarded(self):
  fixture=Fixture('Guard');generated=[]
  def generate(context,check):
   generated.append(context)
   if len(generated)==1:
    fixture.scene['revision']=2;fixture.scene['direction']='New direction';check()
   return {'kind':'pose','text':'responds to new direction'}
  self.assertEqual(Runner(fixture.call,generate,duration=2).run(),1)
  self.assertEqual(len(generated),2);self.assertEqual(fixture.proposals[0]['revision'],2)
 def test_cancel_acknowledged(self):
  fixture=Fixture('Guard')
  def generate(context,check):
   fixture.task['cancellation_requested']=True;check()
  self.assertEqual(Runner(fixture.call,generate,duration=2).run(),0)
  self.assertFalse(fixture.proposals)
 def test_wrong_turn_never_generates(self):
  fixture=Fixture('Guard');fixture.scene['turn']='other';seen=[]
  runner=Runner(fixture.call,lambda *args:seen.append(args),duration=1)
  self.assertEqual(runner.run(),0);self.assertFalse(seen)
 def test_external_command_no_shell(self):
  generator=command_generator([sys.executable,'-c','import json,sys; c=json.load(sys.stdin); print(json.dumps({"kind":"pose","text":c["npc"]["name"]+" waits"}))'])
  result=generator({'npc':{'name':'Guard'}},lambda:None)
  self.assertEqual(result,{'kind':'pose','text':'Guard waits'})
 def test_external_command_cancellation(self):
  generator=command_generator([sys.executable,'-c','import time; time.sleep(30)'])
  def cancel():raise Interrupted('human cancellation')
  with self.assertRaises(Interrupted):generator({},cancel)
 def test_runner_respects_host_rate_during_wait(self):
  fixture=Fixture('Guard');fixture.scene['turn']='other';times=[]
  def call(tool,args):
   times.append(time.monotonic());return fixture.call(tool,args)
  Runner(call,lambda *args:None,duration=1).run()
  self.assertTrue(all(sum(1 for other in times if moment<=other<moment+1)<=20 for moment in times))

if __name__=='__main__':unittest.main()
