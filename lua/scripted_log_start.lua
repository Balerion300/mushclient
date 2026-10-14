-- Optional, game-independent text log naming. Lua 5.1; no client globals.
-- The caller supplies a world-bound host. Nothing runs when this module loads.
local M = {}
local methods = {}
methods.__index = methods
local months = {january=1,february=2,march=3,april=4,may=5,june=6,
  july=7,august=8,september=9,october=10,november=11,december=12}

local function component(value)
  local text = tostring(value):gsub('[<>:"/\\|?*%z\1-\31]', '_')
  text = text:gsub('^[ .]+',''):gsub('[ .]+$','')
  if text == '' then error('Filename becomes empty after removing unsafe characters.') end
  local stem = text:match('^([^.]*)'):gsub('[ .]+$',''):upper()
  if stem=='CON' or stem=='PRN' or stem=='AUX' or stem=='NUL' or
    stem:match('^COM[1-9]$') or stem:match('^LPT[1-9]$') then text='_'..text end
  return text
end
M.safe_component = component

local function finite(value)
  return type(value)=='number' and value==value and value~=math.huge and value~=-math.huge
end
local function bound(value,default,maximum,name,integer)
  if value==nil then value=default end
  if not finite(value) or value<=0 or value>maximum or (integer and value%1~=0) then
    error(name..' must be positive and at most '..maximum..'.')
  end
  return value
end
local function compile(host,pattern)
  if type(pattern)~='string' or pattern=='' then error('A nonempty PCRE pattern is required.') end
  return host.compile(pattern)
end

function M.new(config,host)
  assert(type(config)=='table' and type(host)=='table','Configuration and host are required.')
  local cfg = {}
  cfg.folder=config.folder
  local absolute=type(cfg.folder)=='string' and (cfg.folder:match('^%a:[/\\]') or
    cfg.folder:match('^\\\\[^\\/]+[/\\][^\\/]+') or
    (package.config:sub(1,1)~='\\' and cfg.folder:match('^/')))
  if not absolute
    or cfg.folder:find('[%z\1-\31]') then error('Choose an existing absolute log folder.') end
  cfg.template=config.template or '{world}-{real_date}-{real_time}-{number}.txt'
  if type(cfg.template)~='string' or cfg.template=='' then error('A filename template is required.') end
  cfg.timeout=bound(config.timeout,30,30,'timeout')
  cfg.max_records=bound(config.max_records,100,100,'max_records',true)
  cfg.max_bytes=bound(config.max_bytes,65536,65536,'max_bytes',true)
  cfg.collision_attempts=bound(config.collision_attempts,100,1000,'collision_attempts',true)
  cfg.preserve_header=config.preserve_header==true
  cfg.required=config.required or {}
  cfg.fields={}
  for _,rule in ipairs(config.fields or {}) do
    if type(rule.name)~='string' or not rule.name:match('^[%a_][%w_]*$') then error('Every field needs a valid name.') end
    if rule.name=='number' then
      error('Metadata field conflicts with built-in macro: '..rule.name)
    end
    if rule.transform~=nil and rule.transform~='month_number' and rule.transform~='number' and rule.transform~='pad2' and type(rule.transform)~='function' then
      error('Unknown transform for '..rule.name)
    end
    local capture=rule.capture or 1
    if type(capture)~='string' and (not finite(capture) or capture<1 or capture%1~=0) then error('Invalid capture for '..rule.name) end
    cfg.fields[#cfg.fields+1]={name=rule.name,capture=capture,match=compile(host,rule.regex),transform=rule.transform}
  end
  for _,name in ipairs(cfg.required) do
    if type(name)~='string' or not name:match('^[%a_][%w_]*$') then error('Invalid required field name.') end
  end
  if config.request_command~=nil and config.request_command~='' then
    if type(config.request_command)~='string' or config.request_command:find('[\r\n%z]') then error('Request command must be one line.') end
    cfg.request_command=config.request_command
    cfg.start_match=compile(host,config.start_regex)
    cfg.end_match=compile(host,config.end_regex)
  elseif #cfg.fields>0 or #cfg.required>0 or config.start_regex or config.end_regex then
    error('Reply rules need a configured request_command and start/end delimiters.')
  end
  return setmetatable({config=cfg,host=host,pending=nil},methods)
end

function methods:cancel(reason,quiet)
  local existed=self.pending~=nil;self.pending=nil
  if existed and not quiet then self.host.notify('Log start cancelled: '..(reason or 'requested by user')..'.') end
  return existed
end
function methods:fail(message)
  self.pending=nil;self.host.notify('Log start failed: '..message);return false,message
end
function methods:live()
  local pending=self.pending
  if not pending then return false end
  if self.host.world_id()~=pending.world then return self:fail('The originating world changed.') end
  if pending.requested and not self.host.connected() then return self:fail('The originating world disconnected.') end
  if self.host.now()-pending.started>=self.config.timeout then return self:fail('Metadata reply timed out; no log was opened.') end
  return true
end

function methods:start()
  if self.pending then self:cancel('requested by another start action');return false,'cancelled' end
  if self.host.is_open() then return self:fail('A log is already open.') end
  if self.host.text_mode and not self.host.text_mode() then return self:fail('This helper requires text logging; HTML and raw modes need a custom start script.') end
  local world=self.host.world_id()
  if not world or world=='' then return self:fail('No originating world identity is available.') end
  local requested=self.config.request_command~=nil
  if requested and not self.host.connected() then return self:fail('Connect the originating world before requesting metadata.') end
  local timestamp=self.host.timestamp and self.host.timestamp() or os.time()
  self.pending={world=world,started=self.host.now(),requested=requested,phase=requested and 'seeking' or 'ready',
    records=0,bytes=0,header={},tail={},fields={world=self.host.world_name(),
      real_date=os.date('%Y-%m-%d',timestamp),real_time=os.date('%H-%M-%S',timestamp)}}
  -- Explicit reply fields replace built-ins; missing data never guesses a value.
  for _,rule in ipairs(self.config.fields) do self.pending.fields[rule.name]=nil end
  if requested then
    local ok,status=pcall(self.host.send,self.config.request_command)
    if not ok or status~=self.host.ok then return self:fail('Metadata request could not be sent.') end
    self.host.notify('Log start is waiting for the configured metadata reply.')
    return true,'pending'
  end
  return self:finish()
end

local function transform(value,kind)
  if kind=='month_number' then
    local month=months[tostring(value):lower()]
    if not month then error('Unknown month name: '..tostring(value)) end
    return string.format('%02d',month)
  elseif kind=='number' or kind=='pad2' then
    if not tostring(value):match('^%d+$') then error('Expected an unsigned integer.') end
    local digits=tostring(value):gsub('^0+','');if digits=='' then digits='0' end
    if kind=='pad2' and #digits<2 then digits='0'..digits end
    return digits
  elseif type(kind)=='function' then return kind(value) end
  return value
end
function methods:on_line(line)
  if not self:live() then return true end
  local p=self.pending;local cfg=self.config
  if type(line)~='string' then self:fail('Received a non-text metadata record.');return true end
  p.records=p.records+1;p.bytes=p.bytes+#line+1
  if p.records>cfg.max_records or p.bytes>cfg.max_bytes then self:fail('Metadata collection exceeded its record or byte limit.');return true end
  if p.phase=='ready' then p.tail[#p.tail+1]=line;return true end
  local ok,problem=pcall(function()
    if p.phase=='seeking' then
      if not cfg.start_match(line) then return end
      p.phase='collecting'
    end
    p.header[#p.header+1]=line
    for _,rule in ipairs(cfg.fields) do
      local captures=rule.match(line)
      if captures then
        local value=captures[rule.capture]
        if value~=nil and value~=false then
          value=transform(value,rule.transform)
          if value==nil or tostring(value)=='' then error('Empty parsed field: '..rule.name) end
          if p.fields[rule.name]~=nil and tostring(p.fields[rule.name])~=tostring(value) then error('Conflicting metadata field: '..rule.name) end
          p.fields[rule.name]=value
        end
      end
    end
    if cfg.end_match(line) then p.phase='ready' end
  end)
  if not ok then self:fail('Malformed metadata: '..tostring(problem)) end
  return true -- Never suppress, recolor, or reroute server output.
end

function methods:filename(number)
  local p=self.pending;local missing={}
  for _,name in ipairs(self.config.required) do
    if p.fields[name]==nil or tostring(p.fields[name])=='' then missing[#missing+1]=name end
  end
  if #missing>0 then error('Missing required fields: '..table.concat(missing,', ')) end
  local uses_number=false
  local name=self.config.template:gsub('{([%w_]+)}',function(key)
    if key=='number' then uses_number=true;return string.format('%03d',number) end
    if p.fields[key]==nil or tostring(p.fields[key])=='' then error('Missing template field: '..key) end
    return component(p.fields[key])
  end)
  if name:find('[{}]') then error('Unrecognized filename template syntax.') end
  name=component(name)
  if not uses_number and number>1 then
    local stem,extension=name:match('^(.*)(%.[^.]+)$')
    name=(stem or name)..string.format('-%03d',number)..(extension or '')
  end
  if #name>220 then error('Filename is too long; shorten the template or parsed values.') end
  return self.config.folder:gsub('[/\\]+$','')..'/'..name
end

function methods:finish()
  if not self:live() then return false end
  local p=self.pending
  if p.phase~='ready' then return false,'pending' end
  if self.host.is_open() then return self:fail('Another log opened while waiting for metadata.') end
  if self.host.text_mode and not self.host.text_mode() then return self:fail('Logging mode changed while waiting for metadata.') end
  local path
  for number=1,self.config.collision_attempts do
    local ok,value=pcall(self.filename,self,number)
    if not ok then return self:fail(tostring(value)) end
    path=value
    local opened,status=pcall(self.host.open_exclusive,path)
    if not opened then return self:fail('Exclusive log creation failed: '..tostring(status)) end
    if status==self.host.ok then break end
    if self.host.is_open() then return self:fail('A log is already open.') end
    if not self.host.exists(path) then return self:fail('Cannot create log in the configured folder: '..path) end
    path=nil
  end
  if not path then return self:fail('All configured log-number candidates already exist.') end
  self.pending=nil -- Invalidate before notifying/writing callbacks can reenter.
  local lines={}
  if self.config.preserve_header then for _,line in ipairs(p.header) do lines[#lines+1]=line end end
  for _,line in ipairs(p.tail) do lines[#lines+1]=line end
  if #lines>0 then
    local ok,status=pcall(self.host.write,table.concat(lines,'\n'))
    if not ok or status~=self.host.ok then
      if self.host.close then pcall(self.host.close) end
      return self:fail('The new log header/tail could not be written: '..path)
    end
  end
  self.host.notify('Logging started: '..path);return true,path
end
function methods:tick()
  if not self:live() then return false end
  if self.pending.phase=='ready' then return self:finish() end
  return true,'pending'
end
return M
