-- Loaded only by --main-composer-test into fresh, fictional loopback worlds.
local previousEntered=OnPluginCommandEntered
local label=WorldName():match('^Workspace acceptance (Alpha)$') or WorldName():match('^Workspace acceptance (Beta)$')
local scoped=label and GetAlphaOption('site')=='127.0.0.1'
local base=GetPluginInfo(GetPluginID(),6)..'.main-'..(label or 'disabled')
local function receipt(text)
 local f=assert(io.open(base..'.receipt','ab'));f:write(text,'\n');f:close()
end
function OnPluginCommandEntered(text)
 if not scoped then return text end
 receipt('entered:'..text)
 if text=='recursive' then
  receipt('nested-result:'..tostring(DoCommand('SubmitMainInput')))
  return 'RECURSIVE_ONCE'
 elseif text=='redirect' then
  local other=GetWorldById(string.rep(label=='Alpha' and '22' or '11',12))
  assert(other,'Missing other fictional world');other:Activate()
  return 'OWNER_'..label
 elseif text=='decline' then return '\r'
 elseif text=='clear' then return '\t'
 end
 return previousEntered(text)
end
function OnPluginTick()
 if not scoped then return end
 local f=io.open(base..'.control','rb');if not f then return end
 local action=f:read('*l');local value=f:read('*a');f:close();os.remove(base..'.control')
 local ok,err=pcall(function()
  if action=='draft' then SelectCommand();PasteCommand(value)
  elseif action=='submit' then receipt('submit-result:'..tostring(DoCommand('SubmitMainInput')))
  elseif action=='activate' then Activate()
  elseif action=='height' then assert(SetCommandWindowHeight(tonumber(value))==0)
  elseif action=='autoheight' then assert(SetOption('auto_resize_command_window',tonumber(value))==0)
  elseif action=='fullscreen' then Activate();assert(DoCommand('FullScreenMode')==0)
  elseif action=='reopenalpha' then assert(label=='Beta');assert(Open(GetInfo(67)..'alpha.mcl'))
  elseif action=='close' then
   -- Autoheight options modify this disposable fictional world. Save its own
   -- existing file before the ordinary Close action to avoid an unsaved prompt.
   assert(Save('')==false,'Fictional world save failed')
   assert(GetInfo(113),'Close requires this fictional world to be active on a settled tick')
   assert(DoCommand('Close')==0)
  elseif action=='snapshot' then
   receipt('draft:'..GetCommand())
   receipt('alpha-open:'..tostring(GetWorldById(string.rep('11',12))~=nil))
   receipt('active:'..tostring(GetInfo(113)))
   receipt('input-font-height:'..tostring(GetInfo(214)))
   receipt('world-windows:'..tostring(GetInfo(235)))
   for _,entry in ipairs(GetCommandList(10) or {}) do receipt('history:'..entry) end
  else error('Unsupported fixture action') end
 end)
 receipt('done:'..action..':'..(ok and 'ok' or tostring(err)))
end
