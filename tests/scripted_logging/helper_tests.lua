local helper=dofile('lua/scripted_log_start.lua')
local checks=0
local function check(ok,message) checks=checks+1;assert(ok,message) end
local function host()
  local state={time=0,world='fictional-one',connected=true,open=false,files={},writes={},messages={},sent={}}
  local api={ok=0}
  api.now=function() return state.time end
  api.timestamp=function() return 1791986400 end
  api.world_id=function() return state.world end
  api.world_name=function() return 'Fictional World' end
  api.connected=function() return state.connected end
  api.is_open=function() return state.open end
  api.text_mode=function() return state.mode~='html' and state.mode~='raw' end
  api.notify=function(message) state.messages[#state.messages+1]=message end
  api.send=function(command) state.sent[#state.sent+1]=command;return state.send_error or 0 end
  api.open_exclusive=function(path)
    state.attempts=(state.attempts or 0)+1
    if state.open then return 30027 end
    if state.denied or state.files[path] then return 30013 end
    state.open=true;state.path=path;state.files[path]='';return 0
  end
  api.exists=function(path) return state.files[path]~=nil end
  api.write=function(text)
    if state.write_error then return 30029 end
    state.writes[#state.writes+1]=text;state.files[state.path]=state.files[state.path]..text;return 0
  end
  api.close=function() state.open=false;return 0 end
  api.compile=function(pattern)
    local expression=rex.new(pattern)
    return function(line) local first,last,captures=expression:match(line);if first then return captures or {} end end
  end
  return api,state
end
local function config()
  return {folder='C:/fictional/logs',template='{world}-{ic_year}-{ic_month}-{ic_day}-{rl_year}-{rl_month}-{rl_day}-{number}.txt',
    request_command='fixture-date',start_regex=[[^=== Calendar ===$]],end_regex=[[^=== End ===$]],
    required={'ic_day','ic_month','ic_year','rl_day','rl_month','rl_year'},preserve_header=true,
    fields={
      {name='ic_day',regex=[[^IC-Date: Day (\d+) of Month (\d+), (\d+) AC\.$]],capture=1},
      {name='ic_month',regex=[[^IC-Date: Day (\d+) of Month (\d+), (\d+) AC\.$]],capture=2},
      {name='ic_year',regex=[[^IC-Date: Day (\d+) of Month (\d+), (\d+) AC\.$]],capture=3},
      {name='rl_month',regex=[[^RL-Date: ([A-Za-z]+) (\d+),\s*(\d+)\.?$]],capture=1,transform='month_number'},
      {name='rl_day',regex=[[^RL-Date: ([A-Za-z]+) (\d+),\s*(\d+)\.?$]],capture=2},
      {name='rl_year',regex=[[^RL-Date: ([A-Za-z]+) (\d+),\s*(\d+)\.?$]],capture=3},
    }}
end
local reply={'=== Calendar ===','Title: Fictional scene','IC-Date: Day 18 of Month 4, 178 AC.',
  'RL-Date: October 14,2026','=== End ==='}
local function receive(starter,lines) for _,line in ipairs(lines or reply) do check(starter:on_line(line)==true,'output suppressed') end end

do
  local api,state=host();local starter=helper.new({folder='C:/fictional/logs'},api)
  check(#state.sent==0 and not state.open,'module install performed an action')
  check(starter:start(),'default local-date start failed')
  check(state.path:match('Fictional World%-%d%d%d%d%-%d%d%-%d%d%-%d%d%-%d%d%-%d%d%-001%.txt$'),'unsafe real-time macro')
  check(#state.sent==0,'generic default sent a command')
end
do
  local api,state=host();local starter=helper.new(config(),api)
  check(starter:start(),'metadata start failed');check(not state.open and state.sent[1]=='fixture-date','opened before reply')
  receive(starter);check(not state.open,'opened during line callback')
  starter:on_line('The scene begins.');check(starter:tick(),'complete reply did not open')
  check(state.path=='C:/fictional/logs/Fictional World-178-4-18-2026-10-14-001.txt','actual IC/RL reply dates not used')
  check(#state.writes==1 and state.writes[1]==table.concat(reply,'\n')..'\nThe scene begins.','header/tail lost or duplicated')
  starter:tick();starter:on_line('ordinary later line');check(#state.writes==1,'stale completion wrote again')
end
do
  local api,state=host();local cfg=config();cfg.preserve_header=false;local starter=helper.new(cfg,api)
  starter:start();receive(starter);starter:on_line('tail only');starter:tick()
  check(state.writes[1]=='tail only','disabled header lost tail or wrote reply')
end
do
  local api,state=host();local starter=helper.new(config(),api);starter:start()
  receive(starter,{'=== Calendar ===','IC-Date: Day 18 of Month 4, 178 AC.','=== End ==='})
  check(not starter:tick() and not state.open,'missing fields guessed')
  check(state.messages[#state.messages]:find('Missing required fields',1,true),'missing fields not visible')
  receive(starter);starter:tick();check(not state.open,'late reply revived missing-field attempt')
end
do
  local api,state=host();local starter=helper.new(config(),api);starter:start()
  receive(starter,{'=== Calendar ===','RL-Date: Nevermonth 14,2026'})
  check(not starter.pending and not state.open,'bad month accepted')
  check(state.messages[#state.messages]:find('Malformed metadata',1,true),'parse failure not visible')
end
do
  local api,state=host();local starter=helper.new(config(),api);starter:start()
  starter:on_line('=== Calendar ===');starter:on_line('IC-Date: Day 18 of Month 4, 178 AC.')
  starter:on_line('IC-Date: Day 19 of Month 4, 178 AC.')
  check(not starter.pending,'conflicting field accepted')
end
for _,case in ipairs({'cancel','disconnect','world','timeout'}) do
  local api,state=host();local cfg=config();cfg.timeout=.5;local starter=helper.new(cfg,api);starter:start();receive(starter)
  if case=='cancel' then starter:cancel()
  elseif case=='disconnect' then state.connected=false
  elseif case=='world' then state.world='fictional-two'
  else state.time=.5 end
  starter:tick();receive(starter);check(not state.open and not starter.pending,case..' left actionable stale request')
end
for _,case in ipairs({'records','bytes'}) do
  local api,state=host();local cfg=config();cfg.max_records=2;cfg.max_bytes=64;local starter=helper.new(cfg,api);starter:start()
  if case=='records' then receive(starter,{'unrelated','unrelated','unrelated'}) else starter:on_line(string.rep('x',65)) end
  check(not starter.pending and not state.open,case..' bound failed')
end
do
  local api,state=host();local cfg={folder='C:/fictional/logs',template='{world}.txt',collision_attempts=3}
  state.files['C:/fictional/logs/Fictional World.txt']='original'
  local starter=helper.new(cfg,api);check(starter:start(),'collision fallback failed')
  check(state.path=='C:/fictional/logs/Fictional World-002.txt','fallback number not added')
  check(state.files['C:/fictional/logs/Fictional World.txt']=='original','existing file changed')
end
do
  local api,state=host();local cfg={folder='C:/fictional/logs',template='{number}.txt',collision_attempts=2}
  state.files['C:/fictional/logs/001.txt']='original';state.files['C:/fictional/logs/002.txt']='original'
  check(not helper.new(cfg,api):start() and state.attempts==2 and not state.open,'exhausted collision candidates accepted')
end
do
  local api,state=host();state.denied=true
  check(not helper.new({folder='C:/fictional/logs'},api):start() and state.attempts==1,'permission error treated as collision')
  state.open=true;state.attempts=0
  check(not helper.new(config(),api):start() and state.attempts==0 and #state.sent==0,'existing log disturbed')
end
check(helper.safe_component('..\\evil/CON:*?')=='_evil_CON___','unsafe basename not sanitized')
check(helper.safe_component('CON.txt')=='_CON.txt','reserved device filename accepted')
check(not pcall(helper.safe_component,'..'),'empty traversal component accepted')
do
  local api,state=host();check(not helper.new({folder='C:/fictional/logs',template='{missing}.txt'},api):start(),'unknown template macro accepted')
  check(not pcall(helper.new,{folder='relative/logs'},api),'relative folder accepted')
  check(not pcall(helper.new,{folder='C:/fictional/logs',request_command='one\ntwo'},api),'multiline request accepted')
  local cfg=config();cfg.fields[1].regex=[[(]];check(not pcall(helper.new,cfg,api),'invalid PCRE accepted')
  cfg=config();cfg.timeout=31;check(not pcall(helper.new,cfg,api),'timeout ceiling ignored')
end
for _,mode in ipairs({'html','raw'}) do
  local api,state=host();state.mode=mode
  check(not helper.new(config(),api):start() and #state.sent==0 and not state.open,'non-text mode silently changed')
end
do
  local api,state=host();state.write_error=true;local starter=helper.new(config(),api);starter:start();receive(starter)
  check(not starter:tick() and not state.open and not starter.pending,'write error left logging open')
end
do
  local api,state=host();local cfg=config();cfg.template='{real_date}-{ic_day}.txt'
  cfg.fields[#cfg.fields+1]={name='real_date',regex=[[^Server-Date: (.*)$]],capture=1}
  cfg.fields[1].transform='pad2';local starter=helper.new(cfg,api);starter:start()
  receive(starter,{'=== Calendar ===','IC-Date: Day 8 of Month 4, 178 AC.',
    'RL-Date: October 14,2026','Server-Date: 2001-02-03','=== End ==='})
  check(starter:tick() and state.path=='C:/fictional/logs/2001-02-03-08.txt','parsed date failed to override local date')
  state.open=false;starter=helper.new(cfg,api);starter:start();receive(starter)
  check(not starter:tick() and not state.open,'missing parsed builtin silently used real clock')
end
do
  local api,state=host();local starter=helper.new(config(),api);starter:start()
  check(not starter:start() and #state.sent==1 and not starter.pending,'second start resent pending command')
  receive(starter);starter:tick();check(not state.open,'cancelled toggle accepted old reply')
end
do
  -- Test the published wrapper and configuration markers with actual Lua/rex.
  local api,state=host();package.path='lua/?.lua;'..package.path
  local file=assert(io.open('plugins/Scripted_Log_Names.xml','rb'));local xml=file:read('*a');file:close()
  local script=assert(xml:match('<script><!%[CDATA%[(.-)%]%]></script>'))
  local env=setmetatable({
    GetInfo=function(n) if n==58 then return 'C:/fictional/logs' elseif n==2 then return api.world_name() else return 'C:/fictional/client' end end,
    GetWorldID=api.world_id,IsConnected=api.connected,IsLogOpen=api.is_open,
    GetOption=function() return 0 end,OpenLogExclusive=api.open_exclusive,WriteLog=api.write,
    CloseLog=api.close,Send=api.send,ColourNote=function(a,b,text) api.notify(text) end,
    error_code={eOK=0},utils={timer=api.now},
  },{__index=_G})
  local chunk=assert(loadstring(script));setfenv(chunk,env);chunk()
  check(not state.open and #state.sent==0,'plugin load started logging')
  env.StartLog('start');check(state.open and #state.sent==0,'published generic StartLog failed')
  state.open=false;env.ScriptedLogConfig=config()
  env.StartLog('start');check(#state.sent==1,'published configured start did not send')
  env.StartLog('start');check(#state.sent==1,'published second start resent command')
  for _,line in ipairs(reply) do env.OnPluginLineReceived(line) end
  env.OnPluginTick();check(not state.open,'published toggle failed to invalidate pending')
end
do
  local api,state=host()
  local file=assert(io.open('docs/SCRIPTED_LOGGING.md','rb'));local doc=file:read('*a');file:close()
  local preset=assert(doc:match('```lua\n(.-)\n```'))
  local env=setmetatable({GetInfo=function() return 'C:/fictional/logs' end},{__index=_G})
  local chunk=assert(loadstring(preset));setfenv(chunk,env);chunk()
  local starter=helper.new(env.ScriptedLogConfig,api);starter:start()
  local lines={'====================<Log Info>====================',
    'IC-Date: Day 8 of Month 4, 178 AC. (about noon)',
    'RL-Date: October 14, 2026. (server clock)',
    'Participants: Fictional Knight','Locations: Fictional Keep',
    'Comments: Fictional comments','Summary: Fictional summary',string.rep('=',78)}
  receive(starter,lines);check(starter:tick(),'documented BoD preset failed')
  check(state.path=='C:/fictional/logs/178-04-08 - 2026-10-14.txt','documented BoD reply dates or padding failed')
  check(state.writes[1]==table.concat(lines,'\n'),'documented full header omitted metadata')
end
do
  local api,state=host();local cfg=config()
  cfg.fields[1]={name='ic_day',regex=[[^IC-Date: Day (?P<day>\d+)]],capture='day',transform='pad2'}
  local starter=helper.new(cfg,api);starter:start();receive(starter);starter:tick()
  check(state.open,'named PCRE captures failed')
  check(not pcall(helper.new,{folder='\\relative-on-drive'},api),'drive-relative rooted folder accepted')
end
do
  local scripts={}
  for _,path in ipairs({'plugins/Scripted_Log_Names.xml','plugins/examples/Blood_of_Dragons_Log_Names.xml'}) do
    local api,state=host()
    local file=assert(io.open(path,'rb'));local xml=file:read('*a');file:close()
    local script=assert(xml:match('<script><!%[CDATA%[(.-)%]%]></script>'));scripts[#scripts+1]=script
    local env=setmetatable({
      GetInfo=function(n)
        if n==58 then return '.\\logs\\' elseif n==2 then return api.world_name()
        else return 'C:/fictional/client/' end
      end,
      GetWorldID=api.world_id,IsConnected=api.connected,IsLogOpen=api.is_open,
      GetOption=function() return 0 end,OpenLogExclusive=api.open_exclusive,WriteLog=api.write,
      CloseLog=api.close,Send=api.send,ColourNote=function(a,b,text) api.notify(text) end,
      error_code={eOK=0},utils={timer=api.now},
    },{__index=_G})
    local chunk=assert(loadstring(script));setfenv(chunk,env);chunk()
    check(env.ScriptedLogConfig.folder=='C:/fictional/client/.\\logs\\','relative default log folder not rooted at executable')
    check(not state.open and #state.sent==0,'optional preset load performed an action')
    env.StartLog('start')
    if path:find('/examples/',1,true) then
      check(#state.sent==1 and state.sent[1]=='+loginfo' and not state.open,'optional BoD preset did not defer its explicit start')
      local lines={'====================<Log Info>====================',
        'IC-Date: Day 8 of Month 4, 178 AC. (about noon)',
        'RL-Date: October 14, 2026. (server clock)',
        'Participants: Fictional Knight','Locations: Fictional Keep',
        'Comments: Fictional comments','Summary: Fictional summary',string.rep('=',78)}
      for _,line in ipairs(lines) do check(env.OnPluginLineReceived(line)==true,'optional preset suppressed output') end
      env.OnPluginTick()
      check(state.open and state.path=='C:/fictional/client/.\\logs/178-04-08 - 2026-10-14.txt','ready-to-load BoD preset date naming failed')
      check(state.writes[1]==table.concat(lines,'\n'),'optional preset lost full header')
    else
      check(state.open and #state.sent==0,'generic relative default start failed')
    end
  end
  check(scripts[1]:match('(local starter.*)')==scripts[2]:match('(local starter.*)'),'optional preset host wrapper drifted from generic')
end
print('PASS '..checks..' scripted logging assertions; native Lua '.._VERSION..' / PCRE '..rex.version())
