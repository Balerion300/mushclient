#include "AgentBridgeProtocol.h"
#include "AgentToolSchemas.h"
#include "agent_companion/vendor/json.hpp"
#include <chrono>
#include <stdexcept>
namespace agent_workspace {
using Json=nlohmann::json;
namespace {
void check(bool b){if(!b)throw std::runtime_error("request rejected");}
Counter now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}
void validate(const std::string& tool,const Json& args){
 static const Json schemas=Json::parse(kAgentToolSchemas);const Json* spec=0;
 for(const auto& s:schemas)if(s.at("name")==tool){spec=&s;break;}check(spec&&args.is_object());
 const Json& schema=spec->at("inputSchema");for(const auto& field:schema.at("required"))check(args.count(field.get<std::string>())!=0);
 for(auto it=args.begin();it!=args.end();++it){check(schema.at("properties").count(it.key())!=0);const Json& p=schema.at("properties").at(it.key());const Json& v=it.value();
  if(p.at("type")=="string"){check(v.is_string());const std::string text=v.get<std::string>();check(text.find('\0')==std::string::npos);std::size_t chars=0;for(unsigned char c:text)if((c&0xc0)!=0x80)++chars;check(chars>=p.value("minLength",1)&&chars<=p.value("maxLength",128));if(p.count("enum"))check(std::find(p.at("enum").begin(),p.at("enum").end(),v)!=p.at("enum").end());}
  else {check(v.is_number_integer()&&!v.is_boolean());const long long n=v.get<long long>();check(n>=p.at("minimum").get<long long>()&&n<=p.at("maximum").get<long long>());}
 }
}
Json lines(const Output& o){Json ls=Json::array();for(const auto& l:o.lines)ls.push_back({{"index",l.index},{"line_id",l.index},{"text",l.text},{"received_at_ms",l.receivedAt?Json(l.receivedAt):Json(nullptr)},{"truncated",l.truncated},{"source_index",l.sourceIndex},{"generation",l.generation},{"captured_at_ms",l.capturedAt?Json(l.capturedAt):Json(nullptr)}});return {{"lines",ls},{"revision",o.revision},{"reset",o.reset},{"more",o.more},{"cursor",std::to_string(o.cursor.generation)+":"+std::to_string(o.cursor.next)}};}
Json draft(const Draft& d){return {{"draft_id",d.id},{"window_id",d.window},{"sent",false},{"opened",false},{"review_required",true}};}
Json task(const Task& t){return {{"task_id",t.id},{"title",t.title},{"status",t.status},{"progress",t.progress},{"heartbeat_at_ms",t.heartbeat},{"stale",now()>t.heartbeat+30000},{"cancellation_requested",t.cancellationRequested},{"runner_reported",true}};}
Json sceneJson(const Scene& s,const std::string& agent){Json npcs=Json::array(),actions=Json::array();for(const auto& p:s.npcs){const Npc& n=p.second;Json value={{"npc_id",n.id},{"name",n.name},{"public_summary",n.summary},{"assigned_to_you",n.agent==agent}};if(n.agent==agent){value["window_id"]=n.window;value["brief"]=n.brief;value["knowledge"]=n.knowledge;value["output_instruction"]="Supply prose only; emit modes require the complete visible line including NPC name.";}npcs.push_back(value);}for(const auto& a:s.actions)actions.push_back({{"action_id",a.id},{"npc_id",a.npc},{"kind",a.kind},{"text",a.text},{"status",a.status},{"revision",a.revision}});return {{"scene_id",s.id},{"title",s.title},{"notes",s.notes},{"direction",s.direction},{"turn",s.turn},{"state",s.state},{"revision",s.revision},{"npcs",npcs},{"actions",actions}};}
}
bool validateToolArguments(const std::string& tool,const std::string& arguments){try{check(arguments.size()<=129024);validate(tool,Json::parse(arguments));return true;}catch(const std::exception&){return false;}}
std::string dispatch(Store& store,const std::string& agent,Counter session,const std::string& request){
 try{
  check(request.size()<=131072);Json req=Json::parse(request);check(req.is_object()&&req.size()==3&&req.count("token")&&req.count("tool")&&req.count("arguments")&&req.at("tool").is_string());
  const std::string tool=req.at("tool").get<std::string>();const Json& args=req.at("arguments");validate(tool,args);store.authorize(agent,session);Json result;
  auto str=[&](const char* key){return args.at(key).get<std::string>();};
  if(tool=="list_windows"){result={{"windows",Json::array()}};for(const auto& w:store.listWindows(agent,session))result["windows"].push_back({{"window_id",w.id},{"world",w.world},{"title",w.title},{"generation",w.generation},{"revision",w.revision},{"next_index",w.next}});}
  else if(tool=="read_output"){Cursor c;const Cursor* ptr=0;if(args.count("cursor")){const std::string text=str("cursor");std::size_t colon=text.find(':');check(colon!=std::string::npos);std::size_t used=0;c.generation=std::stoull(text.substr(0,colon),&used);check(used==colon);c.next=std::stoull(text.substr(colon+1),&used);check(used==text.size()-colon-1);c.window=str("window_id");ptr=&c;}result=lines(store.read(agent,session,str("window_id"),ptr,args.value("limit",100)));}
  else if(tool=="search_output")result=lines(store.search(agent,session,str("window_id"),str("text"),args.value("limit",50)));
  else if(tool=="wait_for_output"){Output out=store.read(agent,session,str("window_id"),0,1);result={{"revision",out.revision},{"changed",out.revision!=args.at("revision").get<Counter>()}};}
  else if(tool=="prepare_draft")result=draft(store.prepareDraft(agent,session,str("window_id"),str("request_id"),str("text")));
  else if(tool=="get_agent_workspace"||tool=="wait_for_agent_events"){const Grant& g=store.authorize(agent,session);Json editors=Json::array(),contexts=Json::array(),props=Json::array();for(const auto& e:g.editors)editors.push_back(e);for(const auto& c:g.contexts)contexts.push_back({{"context_id",c.first},{"window_id",c.second.window},{"captured_at_ms",c.second.capturedAt}});for(const auto& p:store.proposals())if(p.second.agent==agent&&p.second.session==session)props.push_back({{"proposal_id",p.first},{"editor_id",p.second.editor},{"status",p.second.status}});result={{"agent",agent},{"revision",g.events},{"events",g.events},{"shared_editors",editors},{"contexts",contexts},{"proposals",props},{"runner_started_by_client",false}};if(tool=="wait_for_agent_events")result["changed"]=g.events!=args.at("revision").get<Counter>();}
  else if(tool=="read_agent_messages"){Json ms=Json::array();Counter cursor=args.value("cursor",Counter(0));for(const auto& m:store.messages(agent,session,cursor,args.value("limit",100))){ms.push_back({{"message_id",m.id},{"role",m.role},{"text",m.text}});cursor=m.id;}result={{"messages",ms},{"cursor",cursor}};}
  else if(tool=="post_agent_message")result={{"message_id",store.postMessage(agent,session,str("text"))},{"sent_to_game",false}};
  else if(tool=="read_agent_context"){Context c=store.context(agent,session,str("context_id"));result={{"context_id",c.id},{"window_id",c.window},{"text",c.text},{"captured_at_ms",c.capturedAt}};}
  else if(tool=="read_shared_editor"){Editor e=store.readEditor(agent,session,str("editor_id"));result={{"editor_id",e.id},{"window_id",e.window},{"revision",e.revision},{"text",e.text}};}
  else if(tool=="propose_editor_edit"){Proposal p=store.proposeEdit(agent,session,str("editor_id"),args.at("revision").get<Counter>(),str("text"),args.value("explanation",std::string()));result={{"proposal_id",p.id},{"status",p.status},{"applied",false},{"sent",false},{"review_required",true}};}
  else if(tool=="start_agent_task")result=task(store.startTask(agent,session,str("title"),now()));
  else if(tool=="update_agent_task")result=task(store.updateTask(agent,session,str("task_id"),str("status"),args.value("progress",std::string()),now()));
  else if(tool=="heartbeat_agent_task"){const Grant& g=store.authorize(agent,session);check(g.tasks.count(str("task_id")));Task t=g.tasks.at(str("task_id"));result=task(store.updateTask(agent,session,t.id,t.status,t.progress,now()));}
  else if(tool=="get_agent_tasks"){result={{"tasks",Json::array()}};for(const auto& t:store.authorize(agent,session).tasks)result["tasks"].push_back(task(t.second));}
  else if(tool=="list_debug_captures"){result={{"captures",Json::array()}};for(const auto& c:store.captures(agent,session))result["captures"].push_back({{"capture_id",c.id},{"window_id",c.window},{"name",c.name},{"active",c.active},{"dropped_lines",c.dropped},{"gap_count",c.gaps},{"next_line_id",c.next},{"retained_bytes",c.bytes}});}
  else if(tool=="read_debug_capture"||tool=="search_debug_capture"){Output out=store.readCapture(agent,session,str("capture_id"),args.value("cursor",Counter(0)),args.value("limit",tool=="search_debug_capture"?50:100),tool=="search_debug_capture"?str("text"):"");result=lines(out);result["cursor"]=out.cursor.next;}
  else if(tool=="list_agent_scenes"){result={{"scenes",Json::array()}};for(const auto& s:store.scenes(agent,session))result["scenes"].push_back({{"scene_id",s.id},{"title",s.title},{"revision",s.revision},{"state",s.state}});}
  else if(tool=="read_agent_scene")result=sceneJson(store.scene(agent,session,str("scene_id")),agent);
  else if(tool=="propose_npc_action"){Action a=store.proposeAction(agent,session,str("scene_id"),str("npc_id"),args.at("revision").get<Counter>(),str("kind"),str("text"),str("request_id"));result={{"action_id",a.id},{"status",a.status},{"sent",false},{"review_required",true}};}
  else throw std::runtime_error("unknown tool");
  std::string response=Json({{"ok",true},{"result",result}}).dump();check(response.size()<=2097152);return response+"\n";
 }catch(const std::exception&){return "{\"ok\":false,\"error\":\"Request rejected or access expired\"}\n";}
}
}
