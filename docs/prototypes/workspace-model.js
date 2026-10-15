/* Original, browser-independent layout model. No connection or transport APIs. */
(function (root) {
  'use strict';
  const CHANNELS = ['Main', 'Public', 'Roleplay', 'Pages', 'Staff', 'Notes', 'Activity'];
  const clone = value => JSON.parse(JSON.stringify(value));
  const tabId = (profile, channel) => `${profile}:${channel}`;
  function initial(profile) {
    return { profile, next: 10, active: tabId(profile, 'Main'), floating: [], tree: {
      id: 's1', type: 'split', axis: 'x', ratio: .67,
      a: {id: 'g1', type: 'group', tabs: ['Main', 'Notes'].map(c => tabId(profile, c)), active: tabId(profile, 'Main')},
      b: {id: 's2', type: 'split', axis: 'y', ratio: .65,
        a: {id: 'g2', type: 'group', tabs: ['Public', 'Pages', 'Roleplay', 'Staff'].map(c => tabId(profile, c)), active: tabId(profile, 'Public')},
        b: {id: 'g3', type: 'group', tabs: [tabId(profile, 'Activity')], active: tabId(profile, 'Activity')}
      }
    }};
  }
  function nodes(tree) { return !tree ? [] : tree.type === 'group' ? [tree] : [tree, ...nodes(tree.a), ...nodes(tree.b)]; }
  const groups = ws => nodes(ws.tree).filter(n => n.type === 'group');
  const find = (ws, id) => nodes(ws.tree).find(n => n.id === id);
  const owner = (ws, tab) => groups(ws).find(g => g.tabs.includes(tab));
  function replace(tree, id, value) {
    if (!tree || tree.id === id) return tree?.id === id ? value : tree;
    if (tree.type === 'split') { tree.a = replace(tree.a, id, value); tree.b = replace(tree.b, id, value); }
    return tree;
  }
  function prune(tree) {
    if (!tree) return null;
    if (tree.type === 'group') return tree.tabs.length ? tree : null;
    tree.a = prune(tree.a); tree.b = prune(tree.b);
    return !tree.a ? tree.b : !tree.b ? tree.a : tree;
  }
  function remove(ws, tab) {
    const group = owner(ws, tab);
    if (group) { group.tabs = group.tabs.filter(t => t !== tab); if (group.active === tab) group.active = group.tabs[0]; }
    ws.floating = ws.floating.filter(f => f.tab !== tab);
  }
  function activate(ws, tab) {
    const group = owner(ws, tab);
    if (!group && !ws.floating.some(f => f.tab === tab)) return false;
    if (group) group.active = tab;
    ws.active = tab; return true;
  }
  function move(ws, tab, targetId, side = 'join', before = null) {
    const target = find(ws, targetId), source = owner(ws, tab);
    if (!target || target.type !== 'group' || !['join','left','right','above','below'].includes(side)) return false;
    if (!source && !ws.floating.some(f => f.tab === tab)) return false;
    if (source === target && (side !== 'join' && source.tabs.length < 2 || before === tab)) return false;
    remove(ws, tab);
    if (side === 'join') {
      const index = before ? target.tabs.indexOf(before) : -1;
      target.tabs.splice(index < 0 ? target.tabs.length : index, 0, tab); target.active = tab;
    } else {
      const leaf = {id: `g${ws.next++}`, type:'group', tabs:[tab], active:tab};
      const first = ['left','above'].includes(side);
      const split = {id:`s${ws.next++}`, type:'split', axis:['left','right'].includes(side)?'x':'y', ratio:.5, a:first?leaf:target, b:first?target:leaf};
      ws.tree = replace(ws.tree, targetId, split);
    }
    ws.tree = prune(ws.tree); ws.active = tab; return true;
  }
  function detach(ws, tab) {
    if (ws.floating.some(f => f.tab === tab) || !owner(ws, tab)) return false;
    const home = owner(ws, tab).id;
    remove(ws, tab); ws.tree = prune(ws.tree);
    ws.floating.push({id:`f${ws.next++}`, tab, home, x:90 + ws.floating.length*25, y:85 + ws.floating.length*25, w:470, h:360});
    ws.active = tab; return true;
  }
  function rejoin(ws, tab) {
    const floating = ws.floating.find(f => f.tab === tab); if (!floating) return false;
    const target = find(ws, floating.home) || groups(ws)[0];
    if (target) return move(ws, tab, target.id, 'join');
    remove(ws, tab); ws.tree={id:`g${ws.next++}`,type:'group',tabs:[tab],active:tab};ws.active=tab;return true;
  }
  function resize(ws, id, ratio) { const split=find(ws,id); if(split?.type==='split')split.ratio=Math.max(.15, Math.min(.85, ratio)); }
  function validate(ws, profile) {
    if (!ws || ws.profile !== profile || !Number.isInteger(ws.next) || ws.next < 10 || ws.next > 1000000 || !Array.isArray(ws.floating) || ws.floating.length>CHANNELS.length) return false;
    const ids=new Set(), tabs=[];let count=0;
    function walk(n, depth=0) {
      if (!n) return depth===0;
      if (++count>20 || depth>12 || typeof n.id!=='string' || ids.has(n.id)) return false;
      ids.add(n.id);
      if(n.type==='group') {if(!Array.isArray(n.tabs)||!n.tabs.length||!n.tabs.includes(n.active))return false;tabs.push(...n.tabs);return true;}
      return n.type==='split'&&['x','y'].includes(n.axis)&&Number.isFinite(n.ratio)&&n.ratio>=.15&&n.ratio<=.85&&!!n.a&&!!n.b&&walk(n.a,depth+1)&&walk(n.b,depth+1);
    }
    if(!walk(ws.tree))return false;
    for(const f of ws.floating){if(typeof f.id!=='string'||ids.has(f.id)||![f.x,f.y,f.w,f.h].every(Number.isFinite))return false;ids.add(f.id);tabs.push(f.tab);}
    const expected=CHANNELS.map(c=>tabId(profile,c));
    return tabs.length===expected.length&&new Set(tabs).size===tabs.length&&expected.every(t=>tabs.includes(t))&&tabs.includes(ws.active);
  }
  const api={CHANNELS,clone,tabId,initial,nodes,groups,find,owner,move,activate,detach,rejoin,resize,validate};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.ValyriaLayout=api;
})(typeof window!=='undefined'?window:globalThis);
