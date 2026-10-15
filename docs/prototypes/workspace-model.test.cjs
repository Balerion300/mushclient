'use strict';
const assert = require('node:assert/strict');
const M = require('./workspace-model.js');
let checks=0;
const check=(condition,message)=>{assert.ok(condition,message);checks++;};
function valid(w){check(M.validate(w,w.profile),'valid tree, one owner and one occurrence of every channel');}
function leaves(w){return M.groups(w).map(g=>({tabs:[...g.tabs],active:g.active}));}
for(const side of ['left','right','above','below']){
  const w=M.initial('mira');valid(w);
  check(M.move(w,'mira:Public','g1',side),'edge placement succeeds');valid(w);
  const g=M.owner(w,'mira:Public');check(g.tabs.length===1,'edge creates an independent pane');
  check(M.move(w,'mira:Public',M.owner(w,'mira:Main').id,'join'),'center join succeeds');valid(w);
  check(M.owner(w,'mira:Main')===M.owner(w,'mira:Public'),'center combines tab ownership');
}
{
  const w=M.initial('mira');
  check(M.move(w,'mira:Notes','g1','join','mira:Main'),'tab reordering succeeds');
  assert.deepEqual(M.owner(w,'mira:Main').tabs,['mira:Notes','mira:Main']);checks++;valid(w);
  check(!M.move(w,'rowan:Public','g1','join'),'another character cannot be introduced by a move');
  check(!M.activate(w,'rowan:Main'),'another character cannot become this layout destination');valid(w);
  check(M.move(w,'mira:Public','g1','right'),'first nested split');
  const publicGroup=M.owner(w,'mira:Public').id;
  check(M.move(w,'mira:Staff',publicGroup,'below'),'second nested split');
  check(M.move(w,'mira:Pages',M.owner(w,'mira:Staff').id,'left'),'third nested split');valid(w);
  const serialized=JSON.stringify(w),restored=JSON.parse(serialized);
  valid(restored);assert.deepEqual(leaves(restored),leaves(w));checks++;
  check(!M.validate(restored,'rowan'),'persistence rejects a wrong connection owner');
}
{
  const w=M.initial('rowan');
  for(const c of M.CHANNELS){check(M.detach(w,`rowan:${c}`),'detach channel');valid(w);}
  check(w.tree===null,'all channels can be detached without losing them');
  for(const c of [...M.CHANNELS].reverse()){check(M.rejoin(w,`rowan:${c}`),'rejoin channel');valid(w);}
  check(w.floating.length===0,'all detached channels rejoined');
  check(M.groups(w).length===1,'rejoins recover one usable group when former groups no longer exist');
  M.resize(w,M.groups(w)[0].id,.4);valid(w);
}
{
  const w=M.initial('quinn');M.resize(w,'s1',1);check(w.tree.ratio===.85,'split maximum clamped');M.resize(w,'s1',0);check(w.tree.ratio===.15,'split minimum clamped');valid(w);
  const duplicate=M.clone(w);duplicate.tree.a.tabs.push('quinn:Main');check(!M.validate(duplicate,'quinn'),'duplicate channels rejected');
  const alien=M.clone(w);alien.tree.a.tabs[0]='mira:Main';check(!M.validate(alien,'quinn'),'cross-owner channels rejected');
  const invalidRatio=M.clone(w);invalidRatio.tree.ratio=null;check(!M.validate(invalidRatio,'quinn'),'non-number split rejected');
  const missing=M.clone(w);missing.tree.b.a.tabs.pop();check(!M.validate(missing,'quinn'),'lost channels rejected');
  const inactive=M.clone(w);inactive.active='quinn:Missing';check(!M.validate(inactive,'quinn'),'missing active destination rejected');
}
console.log(JSON.stringify({pass:true,checks,scope:'Original browser layout model only; not native docking acceptance.'}));
